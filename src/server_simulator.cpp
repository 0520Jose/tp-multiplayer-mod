#include <enet/enet.h>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <cstring>
#include "network/PacketSerializer.h"
#include "network/NetworkTypes.h"
#include "network/ConnectionConfig.h"

// =============================================================================
// Twilight Princess Multiplayer — Dedicated Server
// =============================================================================
// This server acts as the authoritative relay for all multiplayer traffic.
//
// Responsibilities:
//   1. Assign a unique playerID (0-254) to each connecting client
//   2. Relay position/status packets from each client to all OTHER clients
//   3. Overwrite the playerID in relayed packets with the server-authoritative
//      ID to prevent spoofing
//   4. Notify all remaining clients when a player disconnects
//   5. Ghost Mode: If only 1 player is connected, echo their packets back
//      with a position offset (+150 X/Z) to simulate a second player
//
// Packet format: [PacketType:1][PlayerID:1][Payload:N]
// Position payload: 25 bytes (x, y, z, rotY [16b] + stageName [8b] + roomNo [1b])
// Status payload: 8 bytes (health, maxHealth, animID)
// =============================================================================

int main(int argc, char* argv[]) {
    bool enableGhostMode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--ghost") == 0) {
            enableGhostMode = true;
        }
    }

    if (enet_initialize() != 0) {
        std::cerr << "An error occurred while initializing ENet.\n";
        return 1;
    }

    const uint16_t port = ConnectionConfig::GetConfiguredPort(1234);

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

    // 32 max clients, 2 channels, no bandwidth limits
    ENetHost* server = enet_host_create(&address, 32, 2, 0, 0);
    if (server == nullptr) {
        std::cerr << "An error occurred while trying to create an ENet server host.\n";
        return 1;
    }

    std::cout << "===========================================\n";
    std::cout << " TWILIGHT PRINCESS MULTIPLAYER DEDICATED SERVER\n";
    std::cout << " Running on port " << port << "...\n";
    std::cout << " Ghost Mode: " << (enableGhostMode ? "ENABLED (--ghost)" : "DISABLED (Clean Relay)") << "\n";
    std::cout << " Waiting for heroes to connect...\n";
    std::cout << "===========================================\n";

    // =========================================================================
    // Player ID Management
    // =========================================================================
    // Each connected peer gets a unique ID from 0-254.
    // IDs are recycled when players disconnect.

    std::map<ENetPeer*, uint8_t> peerToID;  // Maps peer -> assigned ID
    std::set<uint8_t> usedIDs;               // Set of IDs currently in use
    // BUG 4 FIX: Monotonic counter incremented on every new connection.
    // Ghost Mode uses this to detect reconnects and reset its stale static state.
    uint32_t ghostSessionId = 0;

    // Assigns the lowest available player ID to a peer
    auto assignPlayerID = [&](ENetPeer* peer) -> uint8_t {
        uint8_t id = 0;
        while (usedIDs.count(id) && id < 255) {
            id++;
        }
        usedIDs.insert(id);
        peerToID[peer] = id;
        return id;
    };

    // Releases a player ID back to the pool
    auto releasePlayerID = [&](ENetPeer* peer) -> uint8_t {
        auto it = peerToID.find(peer);
        if (it == peerToID.end()) return 0xFF;
        uint8_t id = it->second;
        usedIDs.erase(id);
        peerToID.erase(it);
        return id;
    };

    // Sends a 2-byte control packet (PACKET_PLAYER_ASSIGN or PACKET_PLAYER_DISCONNECT)
    auto sendControlPacket = [](ENetPeer* peer, uint8_t type, uint8_t playerID) {
        uint8_t buf[2] = { type, playerID };
        ENetPacket* pkt = enet_packet_create(buf, 2, ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(peer, 0, pkt);
    };

    // =========================================================================
    // Main Server Loop
    // =========================================================================

    ENetEvent event;

    while (true) {
        // Service the host with 15ms timeout (balances CPU usage vs latency)
        while (enet_host_service(server, &event, 15) > 0) {
            switch (event.type) {

                // =============================================================
                // New Connection
                // =============================================================
                case ENET_EVENT_TYPE_CONNECT: {
                    uint8_t id = assignPlayerID(event.peer);
                    std::cout << "[+] Hero connected! Assigned ID=" << (int)id
                              << " (Total: " << peerToID.size() << ")\n";

                    // BUG 4 FIX: Increment session counter so Ghost Mode resets its
                    // static position state on the next position packet. Without this,
                    // a reconnecting solo player would see the ghost start at the stale
                    // position from the previous session.
                    ghostSessionId++;

                    // Tell the new client their assigned ID
                    sendControlPacket(event.peer, PACKET_PLAYER_ASSIGN, id);
                    enet_host_flush(server);
                    break;
                }

                // =============================================================
                // Received Data — Relay to All Other Clients
                // =============================================================
                case ENET_EVENT_TYPE_RECEIVE: {
                    // Validate minimum packet size (type + playerID header)
                    if (event.packet->dataLength < 2) {
                        enet_packet_destroy(event.packet);
                        break;
                    }

                    uint8_t type = event.packet->data[0];

                    // --- Server-Authoritative PlayerID ---
                    // Overwrite byte 1 with the actual server-assigned ID for this peer.
                    // This prevents clients from spoofing another player's ID.
                    auto senderIt = peerToID.find(event.peer);
                    if (senderIt != peerToID.end()) {
                        event.packet->data[1] = senderIt->second;
                    }

                    // --- Broadcast to all OTHER connected clients ---
                    for (size_t i = 0; i < server->peerCount; ++i) {
                        ENetPeer* targetPeer = &server->peers[i];
                        if (targetPeer->state == ENET_PEER_STATE_CONNECTED &&
                            targetPeer != event.peer) {
                            ENetPacket* copy = enet_packet_create(
                                event.packet->data,
                                event.packet->dataLength,
                                event.packet->flags);
                            enet_peer_send(targetPeer, event.channelID, copy);
                        }
                    }

                    if (type == PACKET_WORLD_EVENT && event.packet->dataLength == 14) {
                        std::vector<uint8_t> payload(
                            event.packet->data + 2,
                            event.packet->data + event.packet->dataLength);
                        SyncWorldEventPacket ev = PacketSerializer::DeserializeSyncWorldEvent(payload);
                        std::cout << "[*] [World Event] Hero " << (int)event.packet->data[1]
                                  << " triggered type=" << (int)ev.eventType
                                  << " id=" << ev.eventId
                                  << " param=" << (int)ev.param
                                  << " stage=" << ev.stageName << "\n";
                    }


                    // ==========================================================
                    // GHOST MODE — Solo Testing Simulation
                    // ==========================================================
                    // When only 1 player is connected, echo their packets back
                    // with a position offset to simulate a second player.
                    // The ghost uses playerID=200 (well outside normal range).

                    if (enableGhostMode && peerToID.size() == 1) {
                        // Position packet: 2-byte header + 25-byte payload = 27 bytes
                        if (type == PACKET_POSITION && event.packet->dataLength == 27) {
                            // Deserialize, offset, reserialize
                            std::vector<uint8_t> payload(
                                event.packet->data + 2,
                                event.packet->data + event.packet->dataLength);
                            SyncPositionPacket posData =
                                PacketSerializer::DeserializeSyncPosition(payload);

                            // BUG 4 FIX: Use statics keyed to ghostSessionId so that
                            // when the player disconnects and reconnects, Ghost Mode
                            // starts fresh instead of resuming a stale position.
                            static uint32_t s_knownSession = 0xFFFFFFFF;
                            static int ghostTick = 0;
                            static SyncPositionPacket ghostPos;
                            static int packetLogCounter = 0;

                            if (s_knownSession != ghostSessionId) {
                                // New session: reset all ghost state
                                s_knownSession = ghostSessionId;
                                ghostTick = 0;
                                packetLogCounter = 0;
                                ghostPos = posData;
                                ghostPos.x += 150.0f; // Start next to the player
                                ghostPos.z += 150.0f;
                            }

                            if (++packetLogCounter % 30 == 1) {
                                std::cout << "[~] [Ghost Mode] Relaying Hero (Pos: " << (int)posData.x
                                          << ", " << (int)posData.y << ", " << (int)posData.z
                                          << " | Stage: " << posData.stageName << ") -> Ghost 200\n";
                            }

                            if (ghostTick < 150) {
                                // Phase 1: Walk toward +X
                                ghostPos.x += 5.0f;
                                ghostPos.rotY = 16384.0f; // 90 degrees
                            } else if (ghostTick < 300) {
                                // Phase 2: Walk toward -Z
                                ghostPos.z -= 5.0f;
                                ghostPos.rotY = 32768.0f; // 180 degrees
                            } else if (ghostTick < 450) {
                                // Phase 3: Stand still
                            } else {
                                ghostTick = 0; // Restart cycle
                            }
                            ghostTick++;

                            // Stay at the player's Y so the ghost doesn't fall through the map
                            ghostPos.y = posData.y;
                            // Mirror room and stage so the ghost passes visibility checks
                            ghostPos.roomNo = posData.roomNo;
                            std::memcpy(ghostPos.stageName, posData.stageName, 8);

                            std::vector<uint8_t> newPayload =
                                PacketSerializer::SerializeSyncPosition(ghostPos);
                            newPayload.insert(newPayload.begin(), 200);             // Ghost ID
                            newPayload.insert(newPayload.begin(), PACKET_POSITION); // Type

                            ENetPacket* echo = enet_packet_create(
                                newPayload.data(), newPayload.size(), event.packet->flags);
                            enet_peer_send(event.peer, 0, echo);
                        }
                        // Status packet: 2-byte header + 12-byte payload = 14 bytes
                        else if (type == PACKET_STATUS && event.packet->dataLength == 14) {
                            // Echo status verbatim but with ghost ID
                            std::vector<uint8_t> echo(
                                event.packet->data,
                                event.packet->data + event.packet->dataLength);
                            echo[1] = 200;  // Ghost playerID

                            ENetPacket* pkt = enet_packet_create(
                                echo.data(), echo.size(), event.packet->flags);
                            enet_peer_send(event.peer, 0, pkt);
                        }
                    }

                    if (type == PACKET_CHAT_MESSAGE && event.packet->dataLength == 66) {
                        std::vector<uint8_t> payload(
                            event.packet->data + 2,
                            event.packet->data + event.packet->dataLength);
                        SyncChatMessagePacket chat = PacketSerializer::DeserializeSyncChatMessage(payload);
                        std::cout << "[Chat] Hero " << (int)event.packet->data[1] << ": " << chat.message << "\n";
                    }

                    enet_packet_destroy(event.packet);
                    break;
                }

                // =============================================================
                // Disconnection — Notify Remaining Clients
                // =============================================================
                case ENET_EVENT_TYPE_DISCONNECT: {
                    uint8_t id = releasePlayerID(event.peer);
                    std::cout << "[-] Hero disconnected (ID=" << (int)id
                              << "). Remaining: " << peerToID.size() << "\n";

                    // Notify all remaining clients that this player left
                    for (size_t i = 0; i < server->peerCount; ++i) {
                        ENetPeer* targetPeer = &server->peers[i];
                        if (targetPeer->state == ENET_PEER_STATE_CONNECTED) {
                            sendControlPacket(targetPeer, PACKET_PLAYER_DISCONNECT, id);
                        }
                    }
                    break;
                }

                default:
                    break;
            }
        }
    }

    // Unreachable in current loop design, but good practice
    enet_host_destroy(server);
    enet_deinitialize();
    return 0;
}
