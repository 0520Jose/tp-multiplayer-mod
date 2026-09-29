#include "MapTracker.h"
#include "ActorInterface.h"
#include <d/actor/d_a_alink.h>
#include <cmath>
#include <cstring>

extern PlayerSync* g_playerSync;

namespace {
    const char* AngleToCardinal(float degrees) {
        // Normalize degrees to [0, 360)
        float d = std::fmod(degrees, 360.0f);
        if (d < 0.0f) d += 360.0f;

        if (d >= 337.5f || d < 22.5f)  return "N";
        if (d >= 22.5f  && d < 67.5f)  return "NE";
        if (d >= 67.5f  && d < 112.5f) return "E";
        if (d >= 112.5f && d < 157.5f) return "SE";
        if (d >= 157.5f && d < 202.5f) return "S";
        if (d >= 202.5f && d < 247.5f) return "SW";
        if (d >= 247.5f && d < 292.5f) return "W";
        return "NW";
    }
}

MapTracker::MapTracker() {
    Reset();
}

MapTracker::~MapTracker() {
}

void MapTracker::Reset() {
    m_beaconTimer = 0;
    m_lastMarkedPlayer = -1;
}

void MapTracker::Update() {
    if (!g_playerSync) return;

    daAlink_c* localPlayer = (daAlink_c*)dComIfGp_getPlayer(0);
    if (!localPlayer) return;

    const char* localStage = dComIfGp_getStartStageName();
    if (!localStage) return;

    m_beaconTimer++;

    const auto& remotePlayers = g_playerSync->GetRemotePlayers();
    for (const auto& pair : remotePlayers) {
        const auto& state = pair.second;
        if (!state.hasData) continue;

        bool sameStage = (std::strncmp(state.stageName, localStage, 8) == 0);

        if (sameStage) {
            // 1. Visual 3D Beacon: Spawn persistent glowing fairy/light beacon above remote player
            if ((m_beaconTimer % 4) == 0) {
                cXyz beaconPos(state.renderX, state.renderY + 220.0f, state.renderZ);
                // 0x01B7: Twilight Princess glowing spirit/light beacon particle
                dComIfGp_particle_set(0x01B7, &beaconPos, nullptr, nullptr);
            }

            // 2. Full-screen Map / Stage Map Marker:
            // Register companion location in Twilight Princess's map mark system
            cXyz markerPos(state.renderX, state.renderY, state.renderZ);
            dComIfGs_setLastWarpMarkItemData(
                localStage,
                markerPos,
                static_cast<s16>(state.renderRotY),
                static_cast<s8>(state.roomNo),
                1, // Warp type icon
                1  // Active flag
            );
        }
    }
}

bool MapTracker::GetRadarInfo(uint8_t playerID, PlayerRadarInfo& outInfo) const {
    if (!g_playerSync) return false;

    daAlink_c* localPlayer = (daAlink_c*)dComIfGp_getPlayer(0);
    if (!localPlayer) return false;

    const auto& remotePlayers = g_playerSync->GetRemotePlayers();
    auto it = remotePlayers.find(playerID);
    if (it == remotePlayers.end() || !it->second.hasData) return false;

    const auto& state = it->second;
    const char* localStage = dComIfGp_getStartStageName();

    outInfo.playerID = playerID;
    outInfo.isSameStage = localStage && (std::strncmp(state.stageName, localStage, 8) == 0);
    outInfo.isSameRoom = outInfo.isSameStage && (state.roomNo == fopAcM_GetRoomNo(localPlayer));

    float dx = state.renderX - localPlayer->current.pos.x;
    float dz = state.renderZ - localPlayer->current.pos.z;

    outInfo.distance = std::sqrt(dx * dx + dz * dz);

    // World angle: 0 deg = North (+Z in TP world orientation, or standard cartesian)
    float angleRad = std::atan2(dx, dz);
    float angleDeg = angleRad * (180.0f / 3.14159265f);
    if (angleDeg < 0.0f) angleDeg += 360.0f;

    outInfo.relativeAngleDeg = angleDeg;
    outInfo.cardinalDirection = AngleToCardinal(angleDeg);

    return true;
}
