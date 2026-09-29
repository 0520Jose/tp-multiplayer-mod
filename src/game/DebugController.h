#pragma once

#include <cstdint>

// =============================================================================
// DebugController — In-Game Keyboard Testing & Diagnostics Suite
// =============================================================================
// Provides hotkey verification for all TP Multiplayer Mod features:
//   [F1] Help / Controls OSD Toast
//   [F2] Status Diagnostics (Connection, Stage, Room, HP, Rupees, 3D Puppets)
//   [F3] Spawn / Despawn 3D Link Test Dummy (Hero 200)
//   [F4] Toggle Dummy Motion (Idle / Orbit / Patrol)
//   [F5] Test Co-op World Sync (Chests, Keys, Story Bits)
//   [F6] Send In-Game Quick Chat Message
//   [F7] Companion Radar & Spirit Beacon Ping
//   [F8] Toggle Wolf / Human Transformation
//   [F9] Network Reconnect / Reset
//   [F10] Force Respawn 3D Puppet Actors
// =============================================================================

class DebugController {
public:
    DebugController();
    ~DebugController();

    void Update();

    bool IsDummyActive() const { return m_dummyActive; }

private:
    bool JustPressed(int vKey);
    bool IsKeyDown(int vKey);

    void ShowHelpToast();
    void ShowStatusToast();
    void ToggleDummyPlayer();
    void ToggleDummyMotion();
    void TestWorldSync();
    void SendTestChat();
    void PingRadarAndBeacon();
    void ToggleTransformForm();
    void ReconnectNetwork();
    void ReloadPuppetActors();

    void UpdateDummySimulation();

    bool m_keyStates[256];

    // Dummy test player state
    bool m_dummyActive;
    int m_dummyMotionMode; // 0 = Idle, 1 = Orbit Link, 2 = Patrol line
    float m_dummyAngle;
    float m_dummyPatrolDist;
    int m_dummyPatrolDir;
    int m_chatMessageIndex;
};
