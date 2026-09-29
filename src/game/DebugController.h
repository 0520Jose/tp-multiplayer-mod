#pragma once

#include <cstdint>

// =============================================================================
// DebugController — In-Game Keyboard Testing, UI & Diagnostics Suite
// =============================================================================
// Provides hotkey verification and interactive UI for all mod features:
//   [F1]  Help / Controls OSD Toast
//   [F2]  Status Diagnostics (Connection, Stage, Room, HP, Rupees, Heroes)
//   [F5]  Test Co-op World Sync (Chests, Keys, Story Bits)
//   [F6]  Open In-Game Chat Dialog (Type custom message & send)
//   [F7]  Companion Radar & Spirit Beacon Ping
//   [F8]  Toggle Wolf / Human Transformation
//   [F9]  Network Quick Reconnect
//   [F10] Force Respawn 3D Puppet Actors
//   [F11] Open Server IP & Port Connection Dialog (GUI)
// =============================================================================

class DebugController {
public:
    DebugController();
    ~DebugController();

    void Update();

    static void RegisterModsPanel();
    static void OpenChatDialog();
    static void OpenConnectionDialog();

private:
    bool JustPressed(int vKey);
    bool IsKeyDown(int vKey);

    void ShowHelpToast();
    void ShowStatusToast();
    void ToggleDummyPlayer();
    void ToggleDummyMotion();
    void TestWorldSync();
    void PingRadarAndBeacon();
    void ToggleTransformForm();
    void ReconnectNetwork();
    void ReloadPuppetActors();

    void UpdateDummySimulation();

    bool m_keyStates[256];

    // Dummy test player state (Hero 200)
    bool m_dummyActive;
    int m_dummyMotionMode; // 0 = Idle, 1 = Orbit Link, 2 = Patrol line
    float m_dummyAngle;
    float m_dummyPatrolDist;
    int m_dummyPatrolDir;
};
