#pragma once
#include "../network/NetworkTypes.h"
#include <cstdint>
#include <map>

class fopAc_ac_c;
class JPABaseEmitter;
class J3DModel;

// =============================================================================
// PlayerSync — Multi-Player Synchronization & Rendering
// =============================================================================
// Manages the state and visual representation of all remote players.
//
// Architecture:
//   - m_remotePlayers: map of playerID -> RemotePlayerState (with interpolation)
//   - Rendering: Each remote player is shown as a glowing fairy particle
//     positioned at their interpolated coordinates. This is 100% crash-safe
//     because particle resources (0x01A) are always loaded in memory.
//   - Optional 3D model rendering can be enabled in the future via hooks.
//
// Coordinate system:
//   Twilight Princess uses Y-up. Angles are int16 (0-65535 maps to 0-360 deg).
//   Network packets transmit rotY as float (preserving the int16 range).
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
    // Apply a position update from the network for a specific remote player.
    void ApplyRemotePosition(uint8_t playerID, const SyncPositionPacket& posData);

    // Apply a status update (health, animation) for a specific remote player.
    void ApplyRemoteStatus(uint8_t playerID, int16_t health, int16_t maxHealth, uint32_t animationId);

    // Render a single remote player's particle representation
    void RenderPlayerParticle(uint8_t playerID, RemotePlayerState& state);

    // Remove a remote player (called when server notifies of disconnect).
    void RemoveRemotePlayer(uint8_t playerID);

    // --- Per-Frame Update ---
    // Advances interpolation for all remote players and renders their particles.
    // Called once per frame from mod_update when the player is ready.
    void UpdateAllRemotePlayers();

    // --- Cleanup ---
    // Resets all remote player state. Called on map changes, disconnects, etc.
    void ResetAll();

    // --- 3D Model (experimental, for future use) ---
    void CreateRemoteModelIfNeeded(fopAc_ac_c* alink);
    J3DModel* GetRemoteModel() const { return m_remoteModel; }

    const std::map<uint8_t, RemotePlayerState>& GetRemotePlayers() const { return m_remotePlayers; }
    std::map<uint8_t, RemotePlayerState>& GetRemotePlayers() { return m_remotePlayers; }
    fopAc_ac_c* GetRealPlayer() const { return m_realPlayer; }
    uint32_t GetRealPlayerID() const { return m_realPlayerID; }



private:
    // Interpolation helpers
    static float Lerp(float a, float b, float t);
    static float LerpAngle(float a, float b, float t);

    // --- Members ---
    fopAc_ac_c* m_player;
    fopAc_ac_c* m_realPlayer; // Keep track of the real player to prevent ghosts from hijacking the camera
    uint32_t m_realPlayerID = 0xFFFFFFFF;

    // All remote players keyed by server-assigned playerID
    std::map<uint8_t, RemotePlayerState> m_remotePlayers;

private:
    // Optional: cloned J3DModel for 3D rendering (shared across remote players for now)
    J3DModel* m_remoteModel;

    // --- Constants ---
    // How quickly interpolation converges per frame.
    // At 30fps with 0.15, full convergence takes ~20 frames (~0.67s),
    // which gives smooth motion at 15 Hz network update rate.
    static constexpr float LERP_SPEED = 0.15f;

    // Remove a remote player after this many idle frames (~10s at 30fps).
    static constexpr int IDLE_TIMEOUT_FRAMES = 300;

    // 0x8221 is the Z-target cursor/fairy, which is highly visible and always loaded.
    static constexpr uint16_t PARTICLE_FAIRY = 0x8221;

    // How high above the ground position to render the fairy (in game units).
    static constexpr float FAIRY_Y_OFFSET = 80.0f;

    // How far ahead of the player to render the direction indicator.
    static constexpr float DIRECTION_INDICATOR_DISTANCE = 45.0f;
};
