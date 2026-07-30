#pragma once
#include <enet/enet.h>
#include <vector>
#include <cstdint>
#include <string>

// =============================================================================
// Network Client
// =============================================================================
// Manages the ENet connection to the multiplayer server.
// After connecting, the server sends a PACKET_PLAYER_ASSIGN packet containing
// the client's unique playerID. All outgoing packets include this ID in their
// header so the server can route them to other players.
// =============================================================================

class Client {
public:
    Client();
    ~Client();

    bool Connect(const std::string& hostName, uint16_t port);
    void Disconnect();

    // Polls ENet for incoming packets. Returns raw byte vectors
    // (including the [type][playerID] header — caller parses it).
    std::vector<std::vector<uint8_t>> Update();

    // Sends raw data to the server. Caller must prepend [type][playerID].
    void Send(const std::vector<uint8_t>& data);

    bool IsConnected() const;

    // The playerID assigned by the server. 0xFF means not yet assigned.
    uint8_t GetPlayerID() const { return m_myPlayerID; }
    void SetPlayerID(uint8_t id) { m_myPlayerID = id; }

private:
    ENetHost* m_client;
    ENetPeer* m_peer;
    bool m_connected;
    uint8_t m_myPlayerID;  // Assigned by server on connection (0xFF = unassigned)
};
