#define NOMINMAX
#include <enet/enet.h>
#include <mods/api.h>
#include <mods/service.hpp>
#include <mods/svc/actor.h>
#include <mods/svc/log.h>
#include <cstdio>
#include <cmath>
#include <cstring>

#undef IN
#undef OUT
#include <d/actor/d_a_alink.h>
#include <d/d_com_inf_game.h>

#include "network/PacketSerializer.h"
#include "network/Client.h"
#include "network/ConnectionConfig.h"
#include "game/ActorInterface.h"
#include "game/GhostPlayer.h"

// =============================================================================
// Mod Definition & Service Imports
// =============================================================================

DEFINE_MOD()
IMPORT_SERVICE(LogService, g_log);
IMPORT_SERVICE(ActorService, g_actorService);

// =============================================================================
// Global State
// =============================================================================

Client* g_client = nullptr;
PlayerSync* g_playerSync = nullptr;
bool g_isPlayerReady = false;

// =============================================================================
// Send Rate Control (15 Hz at 30 fps)
// =============================================================================

static constexpr int SEND_INTERVAL_FRAMES = 2;
static int g_sendCounter = 0;

// =============================================================================
// Network Pump — Process Incoming Packets
// =============================================================================

void NetworkPumpCallback() {
    if (!g_client || !g_playerSync) return;

    // --- Poll for incoming packets and drive ENet event loop ---
    auto packets = g_client->Update();

    // --- Auto-reconnect logic if disconnected and not in progress ---
    if (!g_client->IsConnected() && !g_client->IsConnecting()) {
        static int connectCooldown = 0;
        if (connectCooldown <= 0) {
            const std::string host = ConnectionConfig::GetConfiguredHost("127.0.0.1");
            const uint16_t port = ConnectionConfig::GetConfiguredPort(1234);
            g_client->Connect(host, port);
            connectCooldown = 60;  // Retry every ~2 seconds at 30fps
        } else {
            connectCooldown--;
        }
    }

    if (!g_client->IsConnected()) {
        return;
    }
    for (const auto& data : packets) {
        if (data.size() < 2) continue;

        uint8_t type = data[0];
        uint8_t playerID = data[1];
        std::vector<uint8_t> payload(data.begin() + 2, data.end());

        switch (type) {
            case PACKET_POSITION: {
                if (payload.size() < 25) break;
                SyncPositionPacket posData = PacketSerializer::DeserializeSyncPosition(payload);
                g_playerSync->ApplyRemotePosition(playerID, posData);
                break;
            }
            case PACKET_STATUS: {
                if (payload.size() < 8) break;
                SyncStatusPacket s = PacketSerializer::DeserializeSyncStatus(payload);
                g_playerSync->ApplyRemoteStatus(playerID, s.health, s.maxHealth, s.currentAnimation);
                break;
            }
            case PACKET_PLAYER_ASSIGN: {
                g_client->SetPlayerID(playerID);
                break;
            }
            case PACKET_PLAYER_DISCONNECT: {
                g_playerSync->RemoveRemotePlayer(playerID);
                break;
            }
        }
    }
}

// =============================================================================
// Player Sync — Send Local State & Advance Remote Interpolation
// =============================================================================

void PlayerSyncCallback() {
    if (!g_client || !g_client->IsConnected() || !g_playerSync) return;

    uint8_t myID = g_client->GetPlayerID();
    if (myID == 0xFF) return;

    // --- Send local state at controlled rate ---
    g_sendCounter++;
    if (g_sendCounter >= SEND_INTERVAL_FRAMES) {
        g_sendCounter = 0;

        SyncPositionPacket posPacket = g_playerSync->GetLocalPosition();
        std::vector<uint8_t> posData = PacketSerializer::SerializeSyncPosition(posPacket);
        posData.insert(posData.begin(), myID);
        posData.insert(posData.begin(), PACKET_POSITION);
        // Position packets are high-frequency (15 Hz) — use unreliable/unsequenced
        // so that a single lost packet doesn't stall the reliable channel queue.
        // LERP interpolation makes individual dropped frames invisible.
        g_client->SendUnreliable(posData);

        SyncStatusPacket statusPacket = g_playerSync->GetLocalStatus();
        std::vector<uint8_t> statusData = PacketSerializer::SerializeSyncStatus(statusPacket);
        statusData.insert(statusData.begin(), myID);
        statusData.insert(statusData.begin(), PACKET_STATUS);
        g_client->Send(statusData); // Status is low-frequency — keep reliable
    }

    // --- Advance interpolation & update remote players ---
    g_playerSync->UpdateAllRemotePlayers();
}

// =============================================================================
// Mod Lifecycle — Initialize
// =============================================================================

extern "C" MOD_EXPORT ModResult mod_initialize(ModError* out_error) {
    if (enet_initialize() != 0) {
        return MOD_ERROR;
    }

    g_client = new Client();
    g_playerSync = new PlayerSync();

    // Register our custom Puppet Actor to bypass Twilight Princess Link singletons
    if (g_actorService) {
        ModResult res = g_actorService->register_actor(
            mod_ctx,
            &daGhostPlayer_c::sProfile,
            &daGhostPlayer_c::sProcName,
            &daGhostPlayer_c::sActorHandle
        );
        if (res != MOD_OK && g_log) {
            g_log->error(mod_ctx, "[Multiplayer] Failed to register ghost player actor profile");
        }
    }

    return MOD_OK;
}

// =============================================================================
// Mod Lifecycle — Update (called every frame)
// =============================================================================

extern "C" MOD_EXPORT ModResult mod_update(ModError* out_error) {
    NetworkPumpCallback();

    static int framesSincePlayerLoaded = 0;

    if (g_playerSync && g_playerSync->GetPlayerActor() != nullptr &&
        g_playerSync->GetLocalStatus().maxHealth > 0 &&
        !g_playerSync->IsOnTitleScreen()) {

        framesSincePlayerLoaded++;

        if (framesSincePlayerLoaded > 120) {
            g_isPlayerReady = true;
            PlayerSyncCallback();
        }
    } else {
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

    if (g_actorService && daGhostPlayer_c::sActorHandle != 0) {
        g_actorService->unregister_actor(mod_ctx, daGhostPlayer_c::sActorHandle);
        daGhostPlayer_c::sActorHandle = 0;
        daGhostPlayer_c::sProcName = -1;
    }

    enet_deinitialize();
    return MOD_OK;
}
