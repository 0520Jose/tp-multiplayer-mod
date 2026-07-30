#define NOMINMAX
#include <enet/enet.h>
#include <mods/api.h>
#include "network/PacketSerializer.h"
#include "game/ActorInterface.h"
#include <mods/hook.hpp>
#include <mods/svc/log.h>
#include <cstdio>
#include <m_Do/m_Do_ext.h>

// Undefine Windows macros that conflict with Dusklight enums
#undef IN
#undef OUT
#include <d/actor/d_a_alink.h>
#include <d/d_com_inf_game.h>
#include <cmath>

#include "network/Client.h"
#include "network/ConnectionConfig.h"

// =============================================================================
// Global State
// =============================================================================
// These are accessed from ActorInterface.cpp via extern declarations.

Client* g_client = nullptr;
PlayerSync* g_playerSync = nullptr;

// =============================================================================
// Send Rate Control
// =============================================================================
// We don't need to send position every frame. At 30fps, sending every 2 frames
// gives 15 Hz update rate, which is sufficient for smooth interpolation on the
// receiving end and significantly reduces bandwidth.

static constexpr int SEND_INTERVAL_FRAMES = 2;
static int g_sendCounter = 0;

// =============================================================================
// Network Pump — Process Incoming Packets
// =============================================================================
// Called every frame. Handles:
//   - Auto-reconnection with cooldown
//   - Parsing incoming packets (position, status, assign, disconnect)
//
// Packet format: [PacketType:1][PlayerID:1][Payload:N]

void NetworkPumpCallback() {
    if (!g_client || !g_playerSync) return;

    // --- Auto-reconnect logic ---
    if (!g_client->IsConnected()) {
        static int connectCooldown = 0;
        if (connectCooldown <= 0) {
            // Read host/port from environment variables or use defaults.
            // Set TP_MULTIPLAYER_HOST / TP_MULTIPLAYER_PORT to override.
            const std::string host = ConnectionConfig::GetConfiguredHost("127.0.0.1");
            const uint16_t port = ConnectionConfig::GetConfiguredPort(1234);
            g_client->Connect(host, port);
            connectCooldown = 60;  // Retry every ~2 seconds at 30fps
        } else {
            connectCooldown--;
        }
        return;
    }

    // --- Poll for incoming packets ---
    auto packets = g_client->Update();
    for (const auto& data : packets) {
        if (data.size() < 2) continue;  // Minimum valid packet: [type][playerID]

        uint8_t type = data[0];
        uint8_t playerID = data[1];

        // Extract payload (everything after the 2-byte header)
        std::vector<uint8_t> payload(data.begin() + 2, data.end());

        switch (type) {
            case PACKET_POSITION: {
                // Position update from another player
                if (payload.size() < 16) break;
                SyncPositionPacket posData = PacketSerializer::DeserializeSyncPosition(payload);
                g_playerSync->ApplyRemotePosition(playerID, posData);
                break;
            }
            case PACKET_STATUS: {
                // Status update (health, animation) from another player
                if (payload.size() < 8) break;
                SyncStatusPacket s = PacketSerializer::DeserializeSyncStatus(payload);
                g_playerSync->ApplyRemoteStatus(playerID, s.health, s.maxHealth, s.currentAnimation);
                break;
            }
            case PACKET_PLAYER_ASSIGN: {
                // Server tells us our assigned player ID
                g_client->SetPlayerID(playerID);
                break;
            }
            case PACKET_PLAYER_DISCONNECT: {
                // Another player disconnected — remove their state and emitters
                g_playerSync->RemoveRemotePlayer(playerID);
                break;
            }
        }
    }
}

// =============================================================================
// Player Sync — Send Local State & Update Remote Rendering
// =============================================================================
// Called every frame when the local player is fully loaded and ready.
// Responsibilities:
//   1. Send local position/status to the server at a controlled rate
//   2. Try to clone Link's 3D model (safe, only happens once per session)
//   3. Update interpolation and render all remote players

void PlayerSyncCallback() {
    if (!g_client || !g_client->IsConnected() || !g_playerSync) return;

    uint8_t myID = g_client->GetPlayerID();
    if (myID == 0xFF) return;  // Server hasn't assigned our ID yet

    // --- Send local state at controlled rate ---
    g_sendCounter++;
    if (g_sendCounter >= SEND_INTERVAL_FRAMES) {
        g_sendCounter = 0;

        // Build and send position packet: [PACKET_POSITION][myID][...16 bytes...]
        SyncPositionPacket posPacket = g_playerSync->GetLocalPosition();
        std::vector<uint8_t> posData = PacketSerializer::SerializeSyncPosition(posPacket);
        posData.insert(posData.begin(), myID);               // playerID byte
        posData.insert(posData.begin(), PACKET_POSITION);    // type byte
        g_client->Send(posData);

        // Build and send status packet: [PACKET_STATUS][myID][...8 bytes...]
        SyncStatusPacket statusPacket = g_playerSync->GetLocalStatus();
        std::vector<uint8_t> statusData = PacketSerializer::SerializeSyncStatus(statusPacket);
        statusData.insert(statusData.begin(), myID);          // playerID byte
        statusData.insert(statusData.begin(), PACKET_STATUS); // type byte
        g_client->Send(statusData);
    }

    // --- Try to create remote 3D model (safe, runs once) ---
    // g_playerSync->CreateRemoteModelIfNeeded(g_playerSync->GetPlayerActor()); // Disabled: was causing a crash at offset 0x650

    // --- Update interpolation & render all remote players ---
    g_playerSync->UpdateAllRemotePlayers();
}

// =============================================================================
// Player Readiness Tracking
// =============================================================================
// We wait 120 frames (~4 seconds at 30fps) after detecting a valid player
// actor before starting to send/receive sync data. This ensures:
//   - The game world is fully loaded
//   - Actor resources are available
//   - We don't crash on loading screen transitions

bool g_isPlayerReady = false;

// =============================================================================
// Mod Service Imports
// =============================================================================

#include <mods/service.hpp>
#include "m_Do/m_Do_controller_pad.h"
#include <cstring>

DEFINE_MOD()
IMPORT_SERVICE(HookService, g_hooks);
IMPORT_SERVICE(LogService, g_log);

// =============================================================================
// Ghost Rendering via Particles (Execute Phase Hook)
// =============================================================================
static interface_of_controller_pad g_backupPad;
static bool g_isGhostExecuting = false;

void OnLinkExecutePost(ModContext* ctx, void* args, void* retval, void* userdata) {
    if (!g_playerSync || !g_isPlayerReady) return;

    daAlink_c* alink = dusk::mods::arg<daAlink_c*>(args, 0);
    if (!alink) return;

    // Restore controller IMMEDIATELY if we were spoofing for the ghost.
    // We do not check alink != localPlayer here, because if g_isGhostExecuting is true,
    // the execute that just finished WAS the ghost.
    if (g_isGhostExecuting) {
        mDoCPd_c::m_cpadInfo[0] = g_backupPad;
        g_isGhostExecuting = false;

        // CRITICAL FIX: The ghost's execute function hijacks the global player pointers!
        // We must restore them immediately so the camera and the real Link don't get broken.
        if (g_playerSync) {
            fopAc_ac_c* realPlayer = g_playerSync->GetRealPlayer();
            uint32_t realPlayerID = g_playerSync->GetRealPlayerID();
            
            // Ensure the real player hasn't been destroyed (e.g. during a map transition)
            if (realPlayer && fopAcM_SearchByID(realPlayerID) != nullptr) {
                dComIfGp_setPlayer(0, realPlayer);
                dComIfGp_setPlayerPtr(0, realPlayer);
            }
        }
    }

    fopAc_ac_c* localPlayer = g_playerSync->GetPlayerActor();

    // Only render particles and override positions once per frame (during local player execute)
    if (alink == localPlayer) {
        auto& players = g_playerSync->GetRemotePlayers();
        for (auto pair : players) {
            if (pair.second.hasData) {
                g_playerSync->RenderPlayerParticle(pair.first, pair.second);
            }
        }
    }
}

HookAction OnLinkExecutePre(ModContext* ctx, void* args, void* retval, void* userdata) {
    if (!g_playerSync || !g_isPlayerReady) return HOOK_CONTINUE;

    daAlink_c* alink = dusk::mods::arg<daAlink_c*>(args, 0);
    if (!alink) return HOOK_CONTINUE;

    auto& players = g_playerSync->GetRemotePlayers();
    for (auto& pair : players) {
        if (fopAcM_GetID(alink) == pair.second.actorID) {
            // It's a ghost! Backup the real controller and spoof it.
            g_backupPad = mDoCPd_c::m_cpadInfo[0];
            g_isGhostExecuting = true;

            // Zero out all buttons and sticks so physics doesn't interfere
            std::memset(&mDoCPd_c::m_cpadInfo[0], 0, sizeof(interface_of_controller_pad));

            // We can now safely spoof the controller for the ghost!
            // Previously this "poisoned" the local player, but that was actually because
            // the ghost was hijacking Player 0. Now that we restore Player 0 in the Post hook,
            // we can freely spoof the ghost's joystick!
            if (pair.second.lerpT < 1.0f) {
                mDoCPd_c::m_cpadInfo[0].mMainStickPosY = 1.0f; // Push stick forward to animate RUN
            }

            return HOOK_CONTINUE;
        }
    }
    return HOOK_CONTINUE;
}

static bool g_isGhostDrawing = false;

HookAction OnLinkDrawPre(ModContext* ctx, void* args, void* retval, void* userdata) {
    if (!g_playerSync || !g_isPlayerReady) return HOOK_CONTINUE;

    daAlink_c* alink = dusk::mods::arg<daAlink_c*>(args, 0);
    if (!alink) return HOOK_CONTINUE;

    auto& players = g_playerSync->GetRemotePlayers();
    for (auto& pair : players) {
        if (fopAcM_GetID(alink) == pair.second.actorID) {
            g_isGhostDrawing = true;
            
            // Give the ghost the crown temporarily so its draw functions read its own state!
            dComIfGp_setPlayer(0, alink);
            dComIfGp_setPlayerPtr(0, alink);
            
            return HOOK_CONTINUE;
        }
    }
    return HOOK_CONTINUE;
}

void OnLinkDrawPost(ModContext* ctx, void* args, void* retval, void* userdata) {
    if (g_isGhostDrawing) {
        g_isGhostDrawing = false;

        // Restore the crown to the real player!
        if (g_playerSync) {
            fopAc_ac_c* realPlayer = g_playerSync->GetRealPlayer();
            uint32_t realPlayerID = g_playerSync->GetRealPlayerID();
            
            if (realPlayer && fopAcM_SearchByID(realPlayerID) != nullptr) {
                dComIfGp_setPlayer(0, realPlayer);
                dComIfGp_setPlayerPtr(0, realPlayer);
            }
        }
    }
}

// =============================================================================
// Mod Lifecycle — Initialize
// =============================================================================
// Called once when the mod is loaded. Sets up ENet and creates the client
// and player sync instances.

extern "C" MOD_EXPORT ModResult mod_initialize(ModError* out_error) {
    if (enet_initialize() != 0) {
        return MOD_ERROR;
    }

    g_client = new Client();
    g_playerSync = new PlayerSync();

    // Hook execute to safely draw particles every frame during the active actor phase
    dusk::mods::hook_add_post<static_cast<int (daAlink_c::*)()>(&daAlink_c::execute)>(g_hooks, OnLinkExecutePost);
    // Hook execute to spoof controller inputs for ghost players
    dusk::mods::hook_add_pre<static_cast<int (daAlink_c::*)()>(&daAlink_c::execute)>(g_hooks, OnLinkExecutePre);
    
    // Hook draw to ensure the ghost reads its own state instead of the local player's
    dusk::mods::hook_add_pre<static_cast<int (daAlink_c::*)()>(&daAlink_c::draw)>(g_hooks, OnLinkDrawPre);
    dusk::mods::hook_add_post<static_cast<int (daAlink_c::*)()>(&daAlink_c::draw)>(g_hooks, OnLinkDrawPost);

    return MOD_OK;
}

// =============================================================================
// Mod Lifecycle — Update (called every frame)
// =============================================================================
// This is the main game loop entry point for the mod. It runs on the game
// thread during the "execute" phase (before draw).

extern "C" MOD_EXPORT ModResult mod_update(ModError* out_error) {
    // Always pump network — handles reconnection and incoming packets
    // even when the player isn't ready yet (so we don't miss the ASSIGN packet).
    NetworkPumpCallback();

    // --- Wait for player to be fully loaded ---
    static int framesSincePlayerLoaded = 0;

    // Player is "valid" when the actor exists AND the save file is loaded
    // (maxHealth > 0 means we're past the title screen and in-game).
    // We also explicitly check that the TITLE actor is not present, because
    // the title screen uses a real 3D world with Link in it for the background.
    if (g_playerSync && g_playerSync->GetPlayerActor() != nullptr &&
        g_playerSync->GetLocalStatus().maxHealth > 0) {

        framesSincePlayerLoaded++;

        // Wait 120 frames for complete initialization before interacting
        // with the actor system.
        if (framesSincePlayerLoaded > 120) {
            // Check for title screen or cinematic events ONLY when the engine is stable
            if (g_playerSync->IsOnTitleScreen()) {
                g_isPlayerReady = false;
                g_playerSync->ResetAll();
            } else {
                g_isPlayerReady = true;
                PlayerSyncCallback();
            }
        }
    } else {
        // Player not ready: loading screen or map transition.
        // Reset all remote player state to avoid stale emitters/data.
        framesSincePlayerLoaded = 0;
        g_isPlayerReady = false;
        if (g_playerSync) {
            g_playerSync->ResetAll();
        }
    }

    return MOD_OK;
}

// =============================================================================
// Mod Lifecycle — Shutdown
// =============================================================================
// Called when the mod is unloaded. Disconnects from the server and frees
// all allocated resources.

extern "C" MOD_EXPORT ModResult mod_shutdown(ModError* out_error) {
    if (g_client) {
        g_client->Disconnect();
        delete g_client;
        g_client = nullptr;
    }
    if (g_playerSync) {
        delete g_playerSync;
        g_playerSync = nullptr;
    }
    enet_deinitialize();
    return MOD_OK;
}
