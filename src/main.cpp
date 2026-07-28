#include <enet/enet.h>
#include <mods/api.h>
#include <iostream>
#include "network/PacketSerializer.h"
#include "game/ActorInterface.h"

#include "network/Client.h"
#include "network/ConnectionConfig.h"

Client* g_client = nullptr;
PlayerSync* g_playerSync = nullptr;

void NetworkPumpCallback() {
    if (!g_client || !g_playerSync) return;

    if (!g_client->IsConnected()) {
        static int connectCooldown = 0;
        if (connectCooldown <= 0) {
            const std::string host = ConnectionConfig::GetConfiguredHost("127.0.0.1");
            const uint16_t port = ConnectionConfig::GetConfiguredPort(1234);
            g_client->Connect(host, port);
            connectCooldown = 60; 
        } else {
            connectCooldown--;
        }
        return;
    }

    auto packets = g_client->Update();
    for (const auto& data : packets) {
        if (data.empty()) continue;
        uint8_t type = data[0];
        
        std::vector<uint8_t> payload(data.begin() + 1, data.end());
        
        if (type == 0) {
            SyncPositionPacket posData = PacketSerializer::DeserializeSyncPosition(payload);
            g_playerSync->ApplyRemotePosition(posData);
        } else if (type == 1) {
            SyncStatusPacket s = PacketSerializer::DeserializeSyncStatus(payload);
            g_playerSync->ApplyRemoteStatus(s.health, s.maxHealth, s.currentAnimation);
        }
    }
}

void PlayerSyncCallback() {
    if (!g_client || !g_client->IsConnected() || !g_playerSync) return;
    
    // --- 2. SEND LOCAL STATE TO SERVER ---
    SyncPositionPacket posPacket = g_playerSync->GetLocalPosition();
    std::vector<uint8_t> posData = PacketSerializer::SerializeSyncPosition(posPacket);
    posData.insert(posData.begin(), 0); // Type 0 = Position
    g_client->Send(posData);
    
    SyncStatusPacket statusPacket = g_playerSync->GetLocalStatus();
    std::vector<uint8_t> statusData = PacketSerializer::SerializeSyncStatus(statusPacket);
    statusData.insert(statusData.begin(), 1); // Type 1 = Status
    g_client->Send(statusData);
}

ModContext* mod_ctx;

extern "C" MOD_EXPORT const ModManifest* mod_get_manifest(void) {
    static const ModManifest manifest = {
        /* struct_size */ sizeof(ModManifest),
        /* abi_version */ MOD_ABI_VERSION,
        /* imports */ nullptr,
        /* import_count */ 0,
        /* exports */ nullptr,
        /* export_count */ 0
    };
    return &manifest;
}

extern "C" MOD_EXPORT ModResult mod_initialize(ModError* out_error) {
    if (enet_initialize() != 0) {
        return MOD_ERROR;
    }

    g_client = new Client();
    g_playerSync = new PlayerSync();
    
    return MOD_OK;
}

extern "C" MOD_EXPORT ModResult mod_update(ModError* out_error) {
    NetworkPumpCallback();
    
    // Use a delay to ensure the game is completely loaded and we aren't in a loading screen
    static int framesSincePlayerLoaded = 0;
    
    // We check if the player actor is not null. It's safe to call here.
    if (g_playerSync && g_playerSync->GetPlayerActor() != nullptr) {
        framesSincePlayerLoaded++;
        // Wait 120 frames (~4 seconds at 30fps) after player becomes valid before interacting with ActorManager
        if (framesSincePlayerLoaded > 120) {
            PlayerSyncCallback();
            if (g_playerSync) {
                g_playerSync->RenderRemotePlayer3D();
            }
        }
    } else {
        framesSincePlayerLoaded = 0;
        if (g_playerSync) {
            g_playerSync->ResetEmitter();
        }
    }
    
    return MOD_OK;
}

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
