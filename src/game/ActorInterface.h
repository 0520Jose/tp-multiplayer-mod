#pragma once
#include "../network/NetworkTypes.h"
#include <cstdint>
#include <map>

class fopAc_ac_c;
class J3DModel;

// =============================================================================
// PlayerSync — Multi-Player Synchronization Manager
// =============================================================================
// Manages the network state, local Link tracking, LERP interpolation, and
// spawning/despawning of puppet actors (daGhostPlayer_c).
// =============================================================================

class PlayerSync {
public:
    PlayerSync();
    ~PlayerSync();

    // --- Local Player ---
    fopAc_ac_c* GetPlayerActor();
    SyncPositionPacket GetLocalPosition();
    SyncStatusPacket GetLocalStatus();
    bool IsOnTitleScreen();

    // --- Remote Players ---
    void ApplyRemotePosition(uint8_t playerID, const SyncPositionPacket& posData);
    void ApplyRemoteStatus(uint8_t playerID, int16_t health, int16_t maxHealth, uint32_t animationId);
    void RemoveRemotePlayer(uint8_t playerID);

    // --- Per-Frame Update ---
    void UpdateAllRemotePlayers();

    // --- Cleanup ---
    void ResetAll();

    const std::map<uint8_t, RemotePlayerState>& GetRemotePlayers() const { return m_remotePlayers; }
    std::map<uint8_t, RemotePlayerState>& GetRemotePlayers() { return m_remotePlayers; }

    fopAc_ac_c* GetRealPlayer() const { return m_player; }
    uint32_t GetRealPlayerID() const { return m_realPlayerID; }

private:
    static float Lerp(float a, float b, float t);
    static float LerpAngle(float a, float b, float t);

    fopAc_ac_c* m_player;
    uint32_t m_realPlayerID;
    std::map<uint8_t, RemotePlayerState> m_remotePlayers;

    static constexpr float LERP_SPEED = 0.15f;
    static constexpr int IDLE_TIMEOUT_FRAMES = 300;
};
