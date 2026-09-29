#include "Client.h"

// =============================================================================
// Client — ENet Network Client for TP Multiplayer
// =============================================================================

Client::Client()
    : m_client(nullptr), m_peer(nullptr), m_connected(false), m_myPlayerID(0xFF) {
}

Client::~Client() {
    Disconnect();
}

bool Client::Connect(const std::string& hostName, uint16_t port) {
    if (m_connected || m_peer) {
        return true;
    }

    // Create the ENet host (client mode: 1 outgoing connection, 2 channels)
    // Channel 0: reliable/ordered — control packets, status
    // Channel 1: unreliable/unsequenced — high-frequency position updates
    if (!m_client) {
        m_client = enet_host_create(nullptr, 1, 2, 0, 0);
        if (!m_client) {
            return false;
        }
    }

    ENetAddress address;
    enet_address_set_host(&address, hostName.c_str());
    address.port = port;

    m_peer = enet_host_connect(m_client, &address, 2, 0);
    if (!m_peer) {
        return false;
    }

    // Increase timeout to 60 seconds to survive long map loading screens
    // without getting disconnected by ENet's default timeout.
    enet_peer_timeout(m_peer, 0, 0, 60000);
    return true;
}

void Client::Disconnect() {
    if (m_peer) {
        enet_peer_disconnect(m_peer, 0);

        // Drain pending packets during graceful disconnect
        ENetEvent event;
        while (enet_host_service(m_client, &event, 3000) > 0) {
            if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                enet_packet_destroy(event.packet);
            } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                break;
            }
        }
        m_peer = nullptr;
    }
    if (m_client) {
        enet_host_destroy(m_client);
        m_client = nullptr;
    }
    m_connected = false;
    m_myPlayerID = 0xFF;  // Reset ID on disconnect
}

std::vector<std::vector<uint8_t>> Client::Update() {
    std::vector<std::vector<uint8_t>> packets;
    if (!m_client) return packets;

    ENetEvent event;
    // Non-blocking poll (timeout = 0): process all queued events
    while (enet_host_service(m_client, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                m_connected = true;
                break;
            case ENET_EVENT_TYPE_RECEIVE: {
                std::vector<uint8_t> data(
                    event.packet->data,
                    event.packet->data + event.packet->dataLength);
                packets.push_back(data);
                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT:
                m_connected = false;
                m_peer = nullptr;
                m_myPlayerID = 0xFF;
                break;
            default:
                break;
        }
    }
    return packets;
}

void Client::Send(const std::vector<uint8_t>& data) {
    if (!m_connected || !m_peer) return;

    // Reliable, ordered channel 0 — for control messages and status updates.
    ENetPacket* packet = enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(m_peer, 0, packet);
}

void Client::SendUnreliable(const std::vector<uint8_t>& data) {
    if (!m_connected || !m_peer) return;

    // Unsequenced, unreliable channel 1 — for high-frequency position updates.
    // A dropped position frame is invisible thanks to LERP interpolation; a stalled
    // reliable retransmit queue on channel 0 would cause visible jitter spikes.
    ENetPacket* packet = enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_UNSEQUENCED);
    enet_peer_send(m_peer, 1, packet);
}

bool Client::IsConnected() const {
    return m_connected;
}

bool Client::IsConnecting() const {
    return (m_peer != nullptr && !m_connected);
}
