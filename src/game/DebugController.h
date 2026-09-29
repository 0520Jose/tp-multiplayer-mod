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
    void TestWorldSync();
    void PingRadarAndBeacon();
    void ToggleTransformForm();
    void ReconnectNetwork();
    void ReloadPuppetActors();

    bool m_keyStates[256];
};
