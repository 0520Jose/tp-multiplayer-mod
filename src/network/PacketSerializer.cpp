#include "PacketSerializer.h"
#include <cstring>
#include <cstdint>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace {
    uint32_t FloatToNet(float f) {
        uint32_t i;
        std::memcpy(&i, &f, sizeof(float));
        return htonl(i);
    }

    float NetToFloat(uint32_t i) {
        i = ntohl(i);
        float f;
        std::memcpy(&f, &i, sizeof(float));
        return f;
    }
}

std::vector<uint8_t> PacketSerializer::SerializeSyncPosition(const SyncPositionPacket& packet) {
    std::vector<uint8_t> data(25);
    uint32_t x = FloatToNet(packet.x);
    uint32_t y = FloatToNet(packet.y);
    uint32_t z = FloatToNet(packet.z);
    uint32_t rotY = FloatToNet(packet.rotY);

    std::memcpy(data.data(), &x, 4);
    std::memcpy(data.data() + 4, &y, 4);
    std::memcpy(data.data() + 8, &z, 4);
    std::memcpy(data.data() + 12, &rotY, 4);
    std::memcpy(data.data() + 16, packet.stageName, 8);
    data[24] = packet.roomNo;

    return data;
}

SyncPositionPacket PacketSerializer::DeserializeSyncPosition(const std::vector<uint8_t>& data) {
    SyncPositionPacket packet;
    if (data.size() < 25) {
        packet.x = packet.y = packet.z = packet.rotY = 0.0f;
        std::memset(packet.stageName, 0, 8);
        packet.roomNo = 0;
        return packet;
    }

    uint32_t x, y, z, rotY;
    std::memcpy(&x, data.data(), 4);
    std::memcpy(&y, data.data() + 4, 4);
    std::memcpy(&z, data.data() + 8, 4);
    std::memcpy(&rotY, data.data() + 12, 4);

    packet.x = NetToFloat(x);
    packet.y = NetToFloat(y);
    packet.z = NetToFloat(z);
    packet.rotY = NetToFloat(rotY);
    std::memcpy(packet.stageName, data.data() + 16, 8);
    packet.roomNo = data[24];

    return packet;
}

std::vector<uint8_t> PacketSerializer::SerializeSyncStatus(const SyncStatusPacket& packet) {
    std::vector<uint8_t> data(8);
    uint16_t health = htons(packet.health);
    uint16_t maxHealth = htons(packet.maxHealth);
    uint32_t anim = htonl(packet.currentAnimation);
    
    std::memcpy(data.data(), &health, 2);
    std::memcpy(data.data() + 2, &maxHealth, 2);
    std::memcpy(data.data() + 4, &anim, 4);
    
    return data;
}

SyncStatusPacket PacketSerializer::DeserializeSyncStatus(const std::vector<uint8_t>& data) {
    SyncStatusPacket packet;
    if (data.size() < 8) {
        packet.health = 0;
        packet.maxHealth = 0;
        packet.currentAnimation = 0;
        return packet;
    }
    
    uint16_t health, maxHealth;
    uint32_t anim;
    
    std::memcpy(&health, data.data(), 2);
    std::memcpy(&maxHealth, data.data() + 2, 2);
    std::memcpy(&anim, data.data() + 4, 4);
    
    packet.health = ntohs(health);
    packet.maxHealth = ntohs(maxHealth);
    packet.currentAnimation = ntohl(anim);
    
    return packet;
}
