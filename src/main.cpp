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
        g_client->Update();
        
        SyncPositionPacket posPacket = g_playerSync->GetLocalPosition();
        std::vector<uint8_t> posData = PacketSerializer::SerializeSyncPosition(posPacket);
        
        // Let's prepend a byte to identify the packet type: 0 for position, 1 for status
        posData.insert(posData.begin(), 0);
        g_client->Send(posData);
        
        SyncStatusPacket statusPacket = g_playerSync->GetLocalStatus();
        std::vector<uint8_t> statusData = PacketSerializer::SerializeSyncStatus(statusPacket);
        statusData.insert(statusData.begin(), 1);
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
