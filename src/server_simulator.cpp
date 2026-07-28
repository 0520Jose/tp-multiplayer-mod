#include <enet/enet.h>
#include <iostream>
#include <vector>
#include <cmath>
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

    std::cout << "Test Server running on port " << port << "...\n";
    std::cout << "Waiting for connections...\n";

    ENetEvent event;
    ENetPeer* connectedPeer = nullptr;

    float t = 0.0f;
    SyncPositionPacket lastPos = {0.0f, 0.0f, 0.0f, 0.0f};

    while (true) {
        while (enet_host_service(server, &event, 10) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    std::cout << "A new client connected from " << event.peer->address.host << ":" << event.peer->address.port << ".\n";
                    connectedPeer = event.peer;
                    break;
                case ENET_EVENT_TYPE_RECEIVE: {
                    // Update base position based on client
                    if (event.packet->dataLength > 0 && event.packet->data[0] == 0) {
                        std::vector<uint8_t> payload(event.packet->data + 1, event.packet->data + event.packet->dataLength);
                        lastPos = PacketSerializer::DeserializeSyncPosition(payload);
                        std::cout << "Received player position: X=" << lastPos.x << ", Y=" << lastPos.y << ", Z=" << lastPos.z << "\n";
                    }
                    enet_packet_destroy(event.packet);
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT:
                    std::cout << "Client disconnected.\n";
                    connectedPeer = nullptr;
                    break;
                default:
                    break;
            }
        }

        if (connectedPeer) {
            // Generate dummy movement: Circle around the player
            t += 0.05f;
            float dummyX = lastPos.x + std::cos(t) * 500.0f; // 500 units radius
            float dummyZ = lastPos.z + std::sin(t) * 500.0f;
            float dummyY = lastPos.y; // Same height
            float dummyRot = t * 10000.0f; // Spin

            SyncPositionPacket p;
            p.x = dummyX;
            p.y = dummyY;
            p.z = dummyZ;
            p.rotY = dummyRot;

            std::vector<uint8_t> payload = PacketSerializer::SerializeSyncPosition(p);
            payload.insert(payload.begin(), 0); // Type 0 = Position

            ENetPacket* packet = enet_packet_create(payload.data(), payload.size(), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(connectedPeer, 0, packet);

            // Also send health status
            SyncStatusPacket s;
            s.health = 12; // 3 hearts
            s.maxHealth = 12;
            s.currentAnimation = 0;
            
            std::vector<uint8_t> sPayload = PacketSerializer::SerializeSyncStatus(s);
            sPayload.insert(sPayload.begin(), 1); // Type 1 = Status

            ENetPacket* sPacket = enet_packet_create(sPayload.data(), sPayload.size(), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(connectedPeer, 0, sPacket);
        }

        // Run at ~60fps
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    enet_host_destroy(server);
    enet_deinitialize();
    return 0;
}
