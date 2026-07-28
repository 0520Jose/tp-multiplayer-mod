#pragma once
#include <vector>
#include <cstdint>
#include "NetworkTypes.h"

class PacketSerializer {
public:
    static std::vector<uint8_t> SerializeSyncPosition(const SyncPositionPacket& packet);
    static SyncPositionPacket DeserializeSyncPosition(const std::vector<uint8_t>& data);
};
