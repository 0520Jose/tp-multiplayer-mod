#include <enet/enet.h>
#include <iostream>
#include <vector>
#include <map>
#include <thread>
#include <chrono>
#include "network/PacketSerializer.h"
#include "network/NetworkTypes.h"
#include "network/ConnectionConfig.h"

int main() {
    if (enet_initialize() != 0) {
        std::cerr << "An error occurred while initializing ENet.\n";
        return 1;
    }

    const uint16_t port = ConnectionConfig::GetConfiguredPort(1234);

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

    ENetHost* server = enet_host_create(&address, 32, 2, 0, 0);
    if (server == nullptr) {
        std::cerr << "An error occurred while trying to create an ENet server host.\n";
        return 1;
    }

    std::cout << "===========================================\n";
    std::cout << " TWILIGHT PRINCESS MULTIPLAYER DEDICATED SERVER\n";
    std::cout << " Running on port " << port << "...\n";
    std::cout << " Waiting for heroes to connect...\n";
    std::cout << "===========================================\n";

    ENetEvent event;

    while (true) {
        while (enet_host_service(server, &event, 15) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    std::cout << "[+] A new hero connected! (Total: " << server->connectedPeers << ")\n";
                    break;
                case ENET_EVENT_TYPE_RECEIVE: {
                    // Broadcast this packet to ALL other connected clients
                    for (size_t i = 0; i < server->peerCount; ++i) {
                        ENetPeer* targetPeer = &server->peers[i];
                        if (targetPeer->state == ENET_PEER_STATE_CONNECTED && targetPeer != event.peer) {
                            ENetPacket* packetCopy = enet_packet_create(event.packet->data, event.packet->dataLength, event.packet->flags);
                            enet_peer_send(targetPeer, 0, packetCopy);
                        }
                    }
                    enet_packet_destroy(event.packet);
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT:
                    std::cout << "[-] A hero disconnected. (Remaining: " << server->connectedPeers << ")\n";
                    break;
                default:
                    break;
            }
        }
    }

    enet_host_destroy(server);
    enet_deinitialize();
    return 0;
}
