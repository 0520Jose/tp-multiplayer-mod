#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <enet/enet.h>
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstring>

#undef IN
#undef OUT
#undef DELETE
#undef OPAQUE
#undef TRANSPARENT

#include "DebugController.h"
#include "ActorInterface.h"
#include "GhostPlayer.h"
#include "WorldSync.h"
#include "MapTracker.h"
#include "../network/Client.h"
#include "../network/PacketSerializer.h"
#include "../network/ConnectionConfig.h"

#include <d/actor/d_a_alink.h>
#include <d/d_com_inf_game.h>
#include <f_op/f_op_actor_mng.h>
#include <mods/svc/ui.h>
#include <mods/svc/log.h>


extern ModContext* mod_ctx;
extern const UiService* g_ui;
extern const LogService* g_log;
extern Client* g_client;
extern PlayerSync* g_playerSync;
extern WorldSync* g_worldSync;
extern MapTracker* g_mapTracker;

namespace {
    void PushNotification(const char* title, const char* body, uint32_t duration_ms = 4000) {
        if (!g_ui) return;
        UiToastDesc toast = UI_TOAST_DESC_INIT;
        toast.title_rml = title;
        toast.body_rml = body;
        toast.duration_ms = duration_ms;
        g_ui->push_toast(mod_ctx, &toast);
    }
}

DebugController::DebugController() {
    std::memset(m_keyStates, 0, sizeof(m_keyStates));
    m_dummyActive = false;
    m_dummyMotionMode = 0;
    m_dummyAngle = 0.0f;
    m_dummyPatrolDist = 0.0f;
    m_dummyPatrolDir = 1;
    m_chatMessageIndex = 0;
}

DebugController::~DebugController() {
    if (m_dummyActive && g_playerSync) {
        g_playerSync->RemoveRemotePlayer(200);
        m_dummyActive = false;
    }
}

bool DebugController::IsKeyDown(int vKey) {
    if (vKey < 0 || vKey >= 256) return false;
    return (GetAsyncKeyState(vKey) & 0x8000) != 0;
}

bool DebugController::JustPressed(int vKey) {
    if (vKey < 0 || vKey >= 256) return false;
    bool down = IsKeyDown(vKey);
    bool pressed = down && !m_keyStates[vKey];
    m_keyStates[vKey] = down;
    return pressed;
}

void DebugController::Update() {
    // Check all F-keys
    if (JustPressed(VK_F1))  ShowHelpToast();
    if (JustPressed(VK_F2))  ShowStatusToast();
    if (JustPressed(VK_F3))  ToggleDummyPlayer();
    if (JustPressed(VK_F4))  ToggleDummyMotion();
    if (JustPressed(VK_F5))  TestWorldSync();
    if (JustPressed(VK_F6))  SendTestChat();
    if (JustPressed(VK_F7))  PingRadarAndBeacon();
    if (JustPressed(VK_F8))  ToggleTransformForm();
    if (JustPressed(VK_F9))  ReconnectNetwork();
    if (JustPressed(VK_F10)) ReloadPuppetActors();

    // Drive local dummy animation / position update if active
    if (m_dummyActive) {
        UpdateDummySimulation();
    }
}

void DebugController::ShowHelpToast() {
    PushNotification(
        "Multiplayer Hotkeys Guide",
        "<b>F1</b>: Guide | <b>F2</b>: Mod Status<br/>"
        "<b>F3</b>: Spawn/Despawn 3D Dummy Link<br/>"
        "<b>F4</b>: Dummy Move (Idle/Orbit/Patrol)<br/>"
        "<b>F5</b>: World Sync Test | <b>F6</b>: Send Chat<br/>"
        "<b>F7</b>: Radar/Beacon | <b>F8</b>: Wolf/Human<br/>"
        "<b>F9</b>: Reconnect | <b>F10</b>: Reload 3D Actor",
        7000
    );
}

void DebugController::ShowStatusToast() {
    char statusBuf[256];
    bool conn = g_client && g_client->IsConnected();
    bool connecting = g_client && g_client->IsConnecting();
    uint8_t myId = g_client ? g_client->GetPlayerID() : 0xFF;
    size_t remoteCount = g_playerSync ? g_playerSync->GetRemotePlayers().size() : 0;
    const char* stage = dComIfGp_getStartStageName();
    if (!stage) stage = "None";

    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    int room = player ? fopAcM_GetRoomNo(player) : 0;
    int hp = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getLife();
    int maxHp = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getMaxLife();
    int rupees = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getRupee();
    uint8_t form = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getTransformStatus();

    std::snprintf(statusBuf, sizeof(statusBuf),
        "Net: %s (ID:%d) | Heroes: %zu<br/>"
        "Stage: %s (Rm:%d) | HP:%d/%d | Rup:%d<br/>"
        "Form: %s | Dummy 3D: %s",
        conn ? "Connected" : (connecting ? "Connecting..." : "Offline"),
        (int)myId, remoteCount,
        stage, room, hp, maxHp, rupees,
        (form == 1) ? "Wolf" : "Human",
        m_dummyActive ? "Active" : "Off"
    );

    PushNotification("Multiplayer Diagnostics", statusBuf, 5500);
}

void DebugController::ToggleDummyPlayer() {
    if (!g_playerSync) return;

    daAlink_c* localPlayer = (daAlink_c*)dComIfGp_getPlayer(0);
    if (!localPlayer) {
        PushNotification("Dummy Spawn Error", "Local player not loaded into world yet.");
        return;
    }

    const char* currentStage = dComIfGp_getStartStageName();
    if (!currentStage) currentStage = "F_SP103";
    int roomNo = fopAcM_GetRoomNo(localPlayer);

    if (!m_dummyActive) {
        m_dummyActive = true;
        m_dummyMotionMode = 0;
        m_dummyAngle = 0.0f;
        m_dummyPatrolDist = 0.0f;
        m_dummyPatrolDir = 1;

        // Spawn dummy 220 units directly in front of Link
        float linkAngleRad = localPlayer->current.angle.y * (3.14159265f / 32768.0f);
        float spawnX = localPlayer->current.pos.x + 220.0f * std::sin(linkAngleRad);
        float spawnY = localPlayer->current.pos.y;
        float spawnZ = localPlayer->current.pos.z + 220.0f * std::cos(linkAngleRad);
        float spawnRotY = localPlayer->current.angle.y + 32768.0f; // Face local Link

        SyncPositionPacket posPkt = {};
        posPkt.x = spawnX;
        posPkt.y = spawnY;
        posPkt.z = spawnZ;
        posPkt.rotY = spawnRotY;
        posPkt.roomNo = static_cast<uint8_t>(roomNo);
        std::strncpy(posPkt.stageName, currentStage, 8);

        uint8_t form = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getTransformStatus();

        g_playerSync->ApplyRemotePosition(200, posPkt);
        g_playerSync->ApplyRemoteStatus(200, 12, 12, 50, form, 0, 0);

        PushNotification("3D Puppet Test Dummy", "Spawned Hero 200 directly in front of you!<br/>Press <b>F4</b> to cycle movement.");
    } else {
        m_dummyActive = false;
        g_playerSync->RemoveRemotePlayer(200);
        PushNotification("3D Puppet Test Dummy", "Despawned test dummy.");
    }
}

void DebugController::ToggleDummyMotion() {
    if (!m_dummyActive) {
        ToggleDummyPlayer();
        return;
    }

    m_dummyMotionMode = (m_dummyMotionMode + 1) % 3;

    if (m_dummyMotionMode == 0) {
        PushNotification("Dummy Movement", "Mode: <b>Standing Idle</b> (Bind / A-pose inspection)");
    } else if (m_dummyMotionMode == 1) {
        PushNotification("Dummy Movement", "Mode: <b>Orbiting Link</b> (360-degree rotation & lighting check)");
    } else {
        PushNotification("Dummy Movement", "Mode: <b>Patrol Walk</b> (Front-to-back walking motion)");
    }
}

void DebugController::UpdateDummySimulation() {
    if (!g_playerSync || !m_dummyActive) return;

    daAlink_c* localPlayer = (daAlink_c*)dComIfGp_getPlayer(0);
    if (!localPlayer) return;

    const char* currentStage = dComIfGp_getStartStageName();
    if (!currentStage) currentStage = "F_SP103";
    int roomNo = fopAcM_GetRoomNo(localPlayer);

    SyncPositionPacket posPkt = {};
    posPkt.y = localPlayer->current.pos.y;
    posPkt.roomNo = static_cast<uint8_t>(roomNo);
    std::strncpy(posPkt.stageName, currentStage, 8);

    uint32_t animId = 0;

    if (m_dummyMotionMode == 0) {
        // Idle: remain 220 units ahead of player, facing player
        float rad = localPlayer->current.angle.y * (3.14159265f / 32768.0f);
        posPkt.x = localPlayer->current.pos.x + 220.0f * std::sin(rad);
        posPkt.z = localPlayer->current.pos.z + 220.0f * std::cos(rad);
        posPkt.rotY = localPlayer->current.angle.y + 32768.0f;
        animId = 0;
    } else if (m_dummyMotionMode == 1) {
        // Orbit: revolve smoothly around Link in a 250-unit circle
        m_dummyAngle += 0.025f;
        if (m_dummyAngle > 6.2831853f) m_dummyAngle -= 6.2831853f;

        posPkt.x = localPlayer->current.pos.x + 250.0f * std::sin(m_dummyAngle);
        posPkt.z = localPlayer->current.pos.z + 250.0f * std::cos(m_dummyAngle);
        // Face tangent along circular path
        float tangentAngleRad = m_dummyAngle + 1.5707963f;
        posPkt.rotY = tangentAngleRad * (32768.0f / 3.14159265f);
        animId = 1;
    } else {
        // Patrol walk back and forth along player's forward vector
        m_dummyPatrolDist += m_dummyPatrolDir * 3.0f;
        if (m_dummyPatrolDist > 300.0f) {
            m_dummyPatrolDist = 300.0f;
            m_dummyPatrolDir = -1;
        } else if (m_dummyPatrolDist < 120.0f) {
            m_dummyPatrolDist = 120.0f;
            m_dummyPatrolDir = 1;
        }

        float rad = localPlayer->current.angle.y * (3.14159265f / 32768.0f);
        posPkt.x = localPlayer->current.pos.x + m_dummyPatrolDist * std::sin(rad);
        posPkt.z = localPlayer->current.pos.z + m_dummyPatrolDist * std::cos(rad);
        posPkt.rotY = (m_dummyPatrolDir > 0) ? localPlayer->current.angle.y : (localPlayer->current.angle.y + 32768.0f);
        animId = 1;
    }

    g_playerSync->ApplyRemotePosition(200, posPkt);

    // Keep dummy from timing out while active
    auto& rem = g_playerSync->GetRemotePlayers();
    auto it = rem.find(200);
    if (it != rem.end()) {
        it->second.framesIdle = 0;
        it->second.animationId = animId;
    }
}

void DebugController::TestWorldSync() {
    const char* currentStage = dComIfGp_getStartStageName();
    if (!currentStage) currentStage = "F_SP103";

    static int s_testSeq = 0;
    s_testSeq = (s_testSeq + 1) % 3;

    SyncWorldEventPacket ev = {};
    std::strncpy(ev.stageName, currentStage, 8);

    if (s_testSeq == 0) {
        ev.eventType = WORLD_EVENT_TBOX_ON;
        ev.eventId = 0x05;
        ev.param = 1;
        PushNotification("World Sync Test", "Triggered: <b>Chest #5 Opened</b> (Broadcasted)");
    } else if (s_testSeq == 1) {
        ev.eventType = WORLD_EVENT_KEY_SYNC;
        ev.eventId = 0;
        ev.param = 3;
        PushNotification("World Sync Test", "Triggered: <b>Dungeon Small Keys = 3</b> (Broadcasted)");
    } else {
        ev.eventType = WORLD_EVENT_BIT_ON;
        ev.eventId = 120;
        ev.param = 1;
        PushNotification("World Sync Test", "Triggered: <b>Story Event Bit #120 Set</b> (Broadcasted)");
    }

    // Apply locally to world state
    if (g_worldSync) {
        g_worldSync->ApplyRemoteEvent(ev);
    }

    // Relay over network if connected
    if (g_client && g_client->IsConnected()) {
        uint8_t myId = g_client->GetPlayerID();
        std::vector<uint8_t> evData = PacketSerializer::SerializeSyncWorldEvent(ev);
        evData.insert(evData.begin(), myId);
        evData.insert(evData.begin(), PACKET_WORLD_EVENT);
        g_client->Send(evData);
    }
}

void DebugController::SendTestChat() {
    static const char* kMessages[] = {
        "Hey! Look over here!",
        "Found a secret co-op chest!",
        "Let's enter the temple together.",
        "Watch out, monsters incoming!",
        "Multiplayer test working great!"
    };
    constexpr int kCount = sizeof(kMessages) / sizeof(kMessages[0]);

    SyncChatMessagePacket chat = {};
    std::strncpy(chat.message, kMessages[m_chatMessageIndex], sizeof(chat.message) - 1);
    m_chatMessageIndex = (m_chatMessageIndex + 1) % kCount;

    uint8_t myId = (g_client && g_client->IsConnected()) ? g_client->GetPlayerID() : 0;

    // Send across network
    if (g_client && g_client->IsConnected()) {
        std::vector<uint8_t> chatData = PacketSerializer::SerializeSyncChatMessage(chat);
        chatData.insert(chatData.begin(), myId);
        chatData.insert(chatData.begin(), PACKET_CHAT_MESSAGE);
        g_client->Send(chatData);
    }

    // Toast locally
    char title[32];
    std::snprintf(title, sizeof(title), "Quick Chat (Hero %d)", (int)myId);
    PushNotification(title, chat.message, 4500);

    if (g_log) {
        char logBuf[128];
        std::snprintf(logBuf, sizeof(logBuf), "[Chat Sent] Hero %d: %s", (int)myId, chat.message);
        g_log->info(mod_ctx, logBuf);
    }
}

void DebugController::PingRadarAndBeacon() {
    if (!g_mapTracker || !g_playerSync) return;

    const auto& rem = g_playerSync->GetRemotePlayers();
    if (rem.empty()) {
        PushNotification("Companion Radar", "No other heroes connected in world.");
        return;
    }

    for (const auto& pair : rem) {
        PlayerRadarInfo info = {};
        if (g_mapTracker->GetRadarInfo(pair.first, info)) {
            // Pulse visual light spirit beacon on target
            cXyz beaconPos(pair.second.renderX, pair.second.renderY + 220.0f, pair.second.renderZ);
            dComIfGp_particle_set(0x01B7, &beaconPos, nullptr, nullptr);

            char msg[128];
            std::snprintf(msg, sizeof(msg),
                "Hero %d is <b>%.1f m %s</b><br/>(%s, %s)",
                (int)pair.first,
                info.distance * 0.01f,
                info.cardinalDirection,
                info.isSameStage ? "Same Stage" : "Other Stage",
                info.isSameRoom ? "Same Room" : "Other Room"
            );
            PushNotification("Companion Radar Ping", msg, 4500);
            return; // Ping closest
        }
    }
}

void DebugController::ToggleTransformForm() {
    uint8_t currentForm = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getTransformStatus();
    uint8_t newForm = (currentForm == 0) ? 1 : 0;

    g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().setTransformStatus(newForm);

    if (newForm == 1) {
        PushNotification("Form Transformation", "Transformed into <b>Wolf Link</b>!");
    } else {
        PushNotification("Form Transformation", "Transformed into <b>Human Link</b>!");
    }
}

void DebugController::ReconnectNetwork() {
    if (!g_client) return;

    if (g_client->IsConnected()) {
        g_client->Disconnect();
    }

    const std::string host = ConnectionConfig::GetConfiguredHost("127.0.0.1");
    const uint16_t port = ConnectionConfig::GetConfiguredPort(1234);
    g_client->Connect(host, port);

    char msg[64];
    std::snprintf(msg, sizeof(msg), "Connecting to %s:%u...", host.c_str(), port);
    PushNotification("Multiplayer Network", msg, 3500);
}

void DebugController::ReloadPuppetActors() {
    if (!g_playerSync) return;

    g_playerSync->ResetAll();
    PushNotification("Multiplayer System", "3D Puppet Actors reloaded and re-initialized.");
}
