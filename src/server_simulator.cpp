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

                    // --- SIMULATION HACK FOR TESTING ---
                    // If the user is alone, echo their packet back with an offset to simulate a second player
                    if (server->connectedPeers == 1 && event.packet->dataLength > 0) {
                        uint8_t type = event.packet->data[0];
                        // Position packet: 1 byte type + 16 bytes SyncPositionPacket
                        if (type == 0 && event.packet->dataLength == 17) {
                            std::vector<uint8_t> payload(event.packet->data + 1, event.packet->data + event.packet->dataLength);
                            SyncPositionPacket posData = PacketSerializer::DeserializeSyncPosition(payload);
                            
                            // Offset position to make the "friend" appear slightly ahead/aside
                            posData.x += 150.0f;
                            posData.z += 150.0f;
                            
                            std::vector<uint8_t> newPayload = PacketSerializer::SerializeSyncPosition(posData);
                            newPayload.insert(newPayload.begin(), 0);
                            
                            ENetPacket* echoPacket = enet_packet_create(newPayload.data(), newPayload.size(), event.packet->flags);
                            enet_peer_send(event.peer, 0, echoPacket);
                        }
                        // Status packet: 1 byte type + 8 bytes SyncStatusPacket
                        else if (type == 1 && event.packet->dataLength == 9) {
                            // We can just echo status verbatim so the fake player has the same animation/health
                            ENetPacket* echoPacket = enet_packet_create(event.packet->data, event.packet->dataLength, event.packet->flags);
                            enet_peer_send(event.peer, 0, echoPacket);
                        }
                    }
                    // -----------------------------------

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
