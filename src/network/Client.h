#pragma once
#include <enet/enet.h>
#include <vector>
#include <cstdint>
#include <string>

class Client {
public:
    Client();
    ~Client();
    
    bool Connect(const std::string& hostName, uint16_t port);
    void Disconnect();
    std::vector<std::vector<uint8_t>> Update();
    void Send(const std::vector<uint8_t>& data);
    bool IsConnected() const;

private:
    ENetHost* m_client;
    ENetPeer* m_peer;
    bool m_connected;
};
