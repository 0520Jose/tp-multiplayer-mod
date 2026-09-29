#pragma once
#include <cstdint>
#include <vector>
#include <cstring>
#include "../network/NetworkTypes.h"

// =============================================================================
// WorldSync — Twilight Princess Co-op World & Story State Synchronization
// =============================================================================
// Keeps story progression, opened chests, dungeon keys, and permanent switches
// synchronized between all connected players in real time without causing
// infinite feedback loops (ping-pong echo).
// =============================================================================

class WorldSync {
public:
    WorldSync();
    ~WorldSync();

    // Reset snapshots (e.g. on return to title screen or game load)
    void Reset();

    // Check for newly activated local game flags and produce delta packets
    void PollLocalEvents(std::vector<SyncWorldEventPacket>& outPackets);

    // Apply a received world event packet from a remote player
    void ApplyRemoteEvent(const SyncWorldEventPacket& packet);

private:
    bool m_initialized;
    char m_currentStage[8];

    // Local state snapshots for differential change detection
    uint8_t m_eventBitsSnapshot[256]; // 256 bytes = 2048 story/trigger event bits
    uint32_t m_tboxSnapshot[2];       // 64 chest bits in current stage
    uint32_t m_switchSnapshot[4];     // 128 permanent switch bits in current stage
    uint8_t m_keyNumSnapshot;         // Small keys count in current dungeon
    uint8_t m_dungeonItemSnapshot;    // Map, Compass, Boss Key in current dungeon

    void CaptureCurrentSnapshots(const char* stage);
};
