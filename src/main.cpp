#include <enet/enet.h>
#include <dusk/mod_api.h>
#include <iostream>
#include "network/PacketSerializer.h"
#include "game/ActorInterface.h"

#include "network/Client.h"

Client* g_client = nullptr;
PlayerSync* g_playerSync = nullptr;

void NetworkUpdateCallback() {
    if (g_client && g_client->IsConnected() && g_playerSync) {
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
                // TODO: Apply this to a spawned dummy/remote actor!
                // For now we just call it on the local player (which will make you teleport if you receive it)
                // In a real scenario, you'd call this on g_remotePlayerSync->ApplyRemotePosition(...)
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
}

extern "C" DUSK_EXPORT void dusk_mod_init() {
    if (enet_initialize() != 0) {
        std::cerr << "An error occurred while initializing ENet.\n";
        return;
    }
    std::cout << "ENet initialized successfully!\n";

    g_client = new Client();
    g_playerSync = new PlayerSync();

    dusk_register_update_callback(NetworkUpdateCallback);
}

extern "C" DUSK_EXPORT void dusk_mod_fini() {
    if (g_client) {
        delete g_client;
        g_client = nullptr;
    }
    if (g_playerSync) {
        delete g_playerSync;
        g_playerSync = nullptr;
    }
    enet_deinitialize();
}
