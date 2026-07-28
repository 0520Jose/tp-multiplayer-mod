#include <enet/enet.h>
#include <mods/api.h>
#include <iostream>
#include "network/PacketSerializer.h"
#include "game/ActorInterface.h"

#include "network/Client.h"
#include "network/ConnectionConfig.h"

Client* g_client = nullptr;
PlayerSync* g_playerSync = nullptr;

void NetworkUpdateCallback() {
    if (!g_client || !g_playerSync) return;

    if (!g_client->IsConnected()) {
        static int connectCooldown = 0;
        if (connectCooldown <= 0) {
            // Try connecting (very low timeout to avoid freezing)
            const std::string host = ConnectionConfig::GetConfiguredHost("127.0.0.1");
            const uint16_t port = ConnectionConfig::GetConfiguredPort(1234);
            g_client->Connect(host, port);
            connectCooldown = 60; // Wait ~1 second before trying again (assuming 60fps)
        } else {
            connectCooldown--;
        }
        return;
    }

    // --- 1. RECEIVE AND PROCESS REMOTE PACKETS ---
    auto packets = g_client->Update();
    for (const auto& data : packets) {
        if (data.empty()) continue;
        uint8_t type = data[0];
        
        // Remove the type byte before deserialization
        std::vector<uint8_t> payload(data.begin() + 1, data.end());
        
        if (type == 0) {
            // Position packet
            SyncPositionPacket p = PacketSerializer::DeserializeSyncPosition(payload);
            // Apply this to the spawned remote actor
            g_playerSync->ApplyRemotePosition(p.x, p.y, p.z, p.rotY);
        } else if (type == 1) {
            // Status packet
            SyncStatusPacket s = PacketSerializer::DeserializeSyncStatus(payload);
            g_playerSync->ApplyRemoteStatus(s.health, s.maxHealth, s.currentAnimation);
        }
    }
    
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
    NetworkUpdateCallback();
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
