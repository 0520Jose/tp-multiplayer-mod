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
#include "game/WorldSync.h"
#include "game/MapTracker.h"
#include "game/DebugController.h"
#include <mods/svc/ui.h>

// =============================================================================
// Mod Definition & Service Imports
// =============================================================================

DEFINE_MOD()
IMPORT_SERVICE(LogService, g_log);
IMPORT_SERVICE(ActorService, g_actorService);
IMPORT_SERVICE(UiService, g_ui);

// =============================================================================
// Global State
// =============================================================================

Client* g_client = nullptr;
PlayerSync* g_playerSync = nullptr;
WorldSync* g_worldSync = nullptr;
MapTracker* g_mapTracker = nullptr;
DebugController* g_debugController = nullptr;
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
                if (payload.size() < 12) break;
                SyncStatusPacket s = PacketSerializer::DeserializeSyncStatus(payload);
                g_playerSync->ApplyRemoteStatus(playerID, s.health, s.maxHealth, s.rupees, s.form, s.actionFlags, s.currentAnimation);
                break;
            }
            case PACKET_PLAYER_ASSIGN: {
                g_client->SetPlayerID(playerID);
                if (g_ui) {
                    char msg[64];
                    std::snprintf(msg, sizeof(msg), "Connected as Hero %d", (int)playerID);
                    UiToastDesc toast = UI_TOAST_DESC_INIT;
                    toast.title_rml = "Multiplayer";
                    toast.body_rml = msg;
                    toast.duration_ms = 3500;
                    g_ui->push_toast(mod_ctx, &toast);
                }
                break;
            }
            case PACKET_PLAYER_DISCONNECT: {
                g_playerSync->RemoveRemotePlayer(playerID);
                if (g_ui) {
                    char msg[64];
                    std::snprintf(msg, sizeof(msg), "Hero %d left the game", (int)playerID);
                    UiToastDesc toast = UI_TOAST_DESC_INIT;
                    toast.title_rml = "Multiplayer";
                    toast.body_rml = msg;
                    toast.duration_ms = 3500;
                    g_ui->push_toast(mod_ctx, &toast);
                }
                break;
            }
            case PACKET_WORLD_EVENT: {
                if (payload.size() < 12) break;
                SyncWorldEventPacket eventData = PacketSerializer::DeserializeSyncWorldEvent(payload);
                if (g_worldSync) {
                    g_worldSync->ApplyRemoteEvent(eventData);
                }
                if (g_ui && eventData.eventType == WORLD_EVENT_TBOX_ON) {
                    char msg[64];
                    std::snprintf(msg, sizeof(msg), "Hero %d opened a chest in %s!", (int)playerID, eventData.stageName);
                    UiToastDesc toast = UI_TOAST_DESC_INIT;
                    toast.title_rml = "Multiplayer Co-op";
                    toast.body_rml = msg;
                    toast.duration_ms = 4000;
                    g_ui->push_toast(mod_ctx, &toast);
                }
                break;
            }
            case PACKET_CHAT_MESSAGE: {
                if (payload.size() < 64) break;
                SyncChatMessagePacket chat = PacketSerializer::DeserializeSyncChatMessage(payload);
                if (g_ui) {
                    char title[32];
                    std::snprintf(title, sizeof(title), "Hero %d", (int)playerID);
                    UiToastDesc toast = UI_TOAST_DESC_INIT;
                    toast.title_rml = title;
                    toast.body_rml = chat.message;
                    toast.duration_ms = 5000;
                    g_ui->push_toast(mod_ctx, &toast);
                }
                if (g_log) {
                    char logBuf[128];
                    std::snprintf(logBuf, sizeof(logBuf), "[Chat] Hero %d: %s", (int)playerID, chat.message);
                    g_log->info(mod_ctx, logBuf);
                }
                break;
            }
        }
    }
}

// =============================================================================
// Player Sync — Send Local State & Advance Remote Interpolation
// =============================================================================

void PlayerSyncCallback() {
    if (!g_playerSync) return;

    // --- Send local state across network if connected ---
    if (g_client && g_client->IsConnected()) {
        uint8_t myID = g_client->GetPlayerID();
        if (myID != 0xFF) {
            g_sendCounter++;
            if (g_sendCounter >= SEND_INTERVAL_FRAMES) {
                g_sendCounter = 0;

                SyncPositionPacket posPacket = g_playerSync->GetLocalPosition();
                std::vector<uint8_t> posData = PacketSerializer::SerializeSyncPosition(posPacket);
                posData.insert(posData.begin(), myID);
                posData.insert(posData.begin(), PACKET_POSITION);
                g_client->SendUnreliable(posData);

                SyncStatusPacket statusPacket = g_playerSync->GetLocalStatus();
                std::vector<uint8_t> statusData = PacketSerializer::SerializeSyncStatus(statusPacket);
                statusData.insert(statusData.begin(), myID);
                statusData.insert(statusData.begin(), PACKET_STATUS);
                g_client->Send(statusData);

                // --- Co-op World & Story Event Synchronization ---
                if (g_worldSync) {
                    std::vector<SyncWorldEventPacket> events;
                    g_worldSync->PollLocalEvents(events);
                    for (const auto& ev : events) {
                        std::vector<uint8_t> evData = PacketSerializer::SerializeSyncWorldEvent(ev);
                        evData.insert(evData.begin(), myID);
                        evData.insert(evData.begin(), PACKET_WORLD_EVENT);
                        g_client->Send(evData);
                    }
                }
            }
        }
    }

    // --- Advance interpolation & update remote players ---
    g_playerSync->UpdateAllRemotePlayers();

    // --- Advance map markers, beacons, and radar ---
    if (g_mapTracker) {
        g_mapTracker->Update();
    }
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
    g_worldSync = new WorldSync();
    g_mapTracker = new MapTracker();
    g_debugController = new DebugController();

    // Register GUI panel in Dusklight Mods settings
    DebugController::RegisterModsPanel();

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

    if (g_debugController) {
        g_debugController->Update();
    }

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
        if (g_worldSync) {
            g_worldSync->Reset();
        }
        if (g_mapTracker) {
            g_mapTracker->Reset();
        }
    }

    return MOD_OK;
}

// =============================================================================
// Mod Lifecycle — Shutdown
// =============================================================================

extern "C" MOD_EXPORT ModResult mod_shutdown(ModError* out_error) {
    if (g_debugController) {
        delete g_debugController;
        g_debugController = nullptr;
    }
    if (g_client) {
        g_client->Disconnect();
        delete g_client;
        g_client = nullptr;
    }
    if (g_playerSync) {
        delete g_playerSync;
        g_playerSync = nullptr;
    }
    if (g_worldSync) {
        delete g_worldSync;
        g_worldSync = nullptr;
    }
    if (g_mapTracker) {
        delete g_mapTracker;
        g_mapTracker = nullptr;
    }

    if (g_actorService && daGhostPlayer_c::sActorHandle != 0) {
        g_actorService->unregister_actor(mod_ctx, daGhostPlayer_c::sActorHandle);
        daGhostPlayer_c::sActorHandle = 0;
        daGhostPlayer_c::sProcName = -1;
    }

    enet_deinitialize();
    return MOD_OK;
}

