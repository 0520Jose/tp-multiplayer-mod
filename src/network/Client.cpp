#include "Client.h"
#include <iostream>

Client::Client() : m_client(nullptr), m_peer(nullptr), m_connected(false) {
}

Client::~Client() {
    Disconnect();
}

bool Client::Connect(const std::string& hostName, uint16_t port) {
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
    enet_peer_timeout(m_peer, 0, 0, 60000);

    ENetEvent event;
    if (enet_host_service(m_client, &event, 100) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
        m_connected = true;
        return true;
    }

    enet_peer_reset(m_peer);
    m_peer = nullptr;
    return false;
}

void Client::Disconnect() {
    if (m_peer) {
        enet_peer_disconnect(m_peer, 0);
        
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
}

std::vector<std::vector<uint8_t>> Client::Update() {
    std::vector<std::vector<uint8_t>> packets;
    if (!m_client) return packets;

    ENetEvent event;
    while (enet_host_service(m_client, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE: {
                std::vector<uint8_t> data(event.packet->data, event.packet->data + event.packet->dataLength);
                packets.push_back(data);
                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT:
                m_connected = false;
                m_peer = nullptr;
                break;
            default:
                break;
        }
    }
    return packets;
}

void Client::Send(const std::vector<uint8_t>& data) {
    if (!m_connected || !m_peer) return;
    
    ENetPacket* packet = enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(m_peer, 0, packet);
}

bool Client::IsConnected() const {
    return m_connected;
}
