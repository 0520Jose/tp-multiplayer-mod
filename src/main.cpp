#include <enet/enet.h>
#include <dusk/mod_api.h>
#include <iostream>
#include "network/PacketSerializer.h"
#include "game/ActorInterface.h"

class Client {
public:
    bool IsConnected() const { return true; }
    void Send(const std::vector<uint8_t>& data) {}
};

Client* g_client = nullptr;
PlayerSync* g_playerSync = nullptr;

void NetworkUpdateCallback() {
    if (g_client && g_client->IsConnected() && g_playerSync) {
        SyncPositionPacket packet = g_playerSync->GetLocalPosition();
        std::vector<uint8_t> data = PacketSerializer::SerializeSyncPosition(packet);
        g_client->Send(data);
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
