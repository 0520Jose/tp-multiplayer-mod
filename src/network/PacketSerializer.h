#pragma once
#include <vector>
#include <cstdint>
#include "NetworkTypes.h"

class PacketSerializer {
public:
    static std::vector<uint8_t> SerializeSyncPosition(const SyncPositionPacket& packet);
    static SyncPositionPacket DeserializeSyncPosition(const std::vector<uint8_t>& data);
    
    static std::vector<uint8_t> SerializeSyncStatus(const SyncStatusPacket& packet);
    static SyncStatusPacket DeserializeSyncStatus(const std::vector<uint8_t>& data);
};
