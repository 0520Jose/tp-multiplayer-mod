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
    std::vector<uint8_t> data(16);
    uint32_t x = FloatToNet(packet.x);
    uint32_t y = FloatToNet(packet.y);
    uint32_t z = FloatToNet(packet.z);
    uint32_t rotY = FloatToNet(packet.rotY);

    std::memcpy(data.data(), &x, 4);
    std::memcpy(data.data() + 4, &y, 4);
    std::memcpy(data.data() + 8, &z, 4);
    std::memcpy(data.data() + 12, &rotY, 4);

    return data;
}

SyncPositionPacket PacketSerializer::DeserializeSyncPosition(const std::vector<uint8_t>& data) {
    SyncPositionPacket packet;
    if (data.size() < 16) {
        packet.x = 0.0f;
        packet.y = 0.0f;
        packet.z = 0.0f;
        packet.rotY = 0.0f;
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

    return packet;
}
