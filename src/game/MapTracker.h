#pragma once
#include <cstdint>
#include <string>
#include <d/d_com_inf_game.h>
#include "../network/NetworkTypes.h"

// =============================================================================
// MapTracker — Player Beacon, Compass & Map Screen Synchronization
// =============================================================================
// Provides visual beacon particles in 3D space, world map warp/companion markers,
// and distance/compass bearing calculation for remote players.
// =============================================================================

struct PlayerRadarInfo {
    uint8_t playerID;
    float distance;
    float relativeAngleDeg;
    const char* cardinalDirection; // "N", "NE", "E", "SE", "S", "SW", "W", "NW"
    bool isSameStage;
    bool isSameRoom;
};

class MapTracker {
public:
    MapTracker();
    ~MapTracker();

    // Reset markers upon scene transition or disconnect
    void Reset();

    // Called every frame tick from PlayerSyncCallback
    void Update();

    // Query radar bearing and distance for a remote player
    bool GetRadarInfo(uint8_t playerID, PlayerRadarInfo& outInfo) const;

private:
    uint32_t m_beaconTimer;
    int m_lastMarkedPlayer;
};
