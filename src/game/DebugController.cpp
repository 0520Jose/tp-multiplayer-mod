#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <enet/enet.h>
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

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
    // Buffers for interactive UI controls
    char s_uiHostBuffer[128] = "127.0.0.1";
    int64_t s_uiPortValue = 1234;
    char s_uiChatMessage[64] = "Hello from Hyrule!";

    UiDialogHandle s_activeChatDialog = 0;
    UiDialogHandle s_activeConnDialog = 0;

    void PushNotification(const char* title, const char* body, uint32_t duration_ms = 4000) {
        if (!g_ui) return;
        UiToastDesc toast = UI_TOAST_DESC_INIT;
        toast.title_rml = title;
        toast.body_rml = body;
        toast.duration_ms = duration_ms;
        g_ui->push_toast(mod_ctx, &toast);
    }

    void SendChatMessage(const char* text) {
        if (!text || text[0] == '\0') return;

        SyncChatMessagePacket chat = {};
        std::strncpy(chat.message, text, sizeof(chat.message) - 1);

        uint8_t myId = (g_client && g_client->IsConnected()) ? g_client->GetPlayerID() : 0;

        // Broadcast over network
        if (g_client && g_client->IsConnected()) {
            std::vector<uint8_t> chatData = PacketSerializer::SerializeSyncChatMessage(chat);
            chatData.insert(chatData.begin(), myId);
            chatData.insert(chatData.begin(), PACKET_CHAT_MESSAGE);
            g_client->Send(chatData);
        }

        // Show local toast
        char title[32];
        std::snprintf(title, sizeof(title), "Chat (Hero %d)", (int)myId);
        PushNotification(title, chat.message, 4500);

        if (g_log) {
            char logBuf[128];
            std::snprintf(logBuf, sizeof(logBuf), "[Chat Sent] Hero %d: %s", (int)myId, chat.message);
            g_log->info(mod_ctx, logBuf);
        }
    }

    void ConnectUsingUiConfig() {
        if (!g_client) return;

        ConnectionConfig::SetHost(s_uiHostBuffer);
        ConnectionConfig::SetPort(static_cast<uint16_t>(s_uiPortValue));
        ConnectionConfig::Save();

        if (g_client->IsConnected() || g_client->IsConnecting()) {
            g_client->Disconnect();
        }

        g_client->Connect(s_uiHostBuffer, static_cast<uint16_t>(s_uiPortValue));

        char msg[128];
        std::snprintf(msg, sizeof(msg), "Connecting to %s:%u...", s_uiHostBuffer, (unsigned)s_uiPortValue);
        PushNotification("Multiplayer Connection", msg, 4000);
    }

    // --- UI Callbacks for Host IP ---
    void UiGetHost(ModContext*, void*, UiControlValue* out_val) {
        out_val->string_value = s_uiHostBuffer;
    }
    void UiSetHost(ModContext*, void*, const UiControlValue* val) {
        if (val && val->string_value) {
            std::strncpy(s_uiHostBuffer, val->string_value, sizeof(s_uiHostBuffer) - 1);
            s_uiHostBuffer[sizeof(s_uiHostBuffer) - 1] = '\0';
        }
    }

    // --- UI Callbacks for Port ---
    void UiGetPort(ModContext*, void*, UiControlValue* out_val) {
        out_val->int_value = s_uiPortValue;
    }
    void UiSetPort(ModContext*, void*, const UiControlValue* val) {
        if (val) {
            s_uiPortValue = val->int_value;
        }
    }

    // --- UI Callbacks for Chat Input ---
    void UiGetChat(ModContext*, void*, UiControlValue* out_val) {
        out_val->string_value = s_uiChatMessage;
    }
    void UiSetChat(ModContext*, void*, const UiControlValue* val) {
        if (val && val->string_value) {
            std::strncpy(s_uiChatMessage, val->string_value, sizeof(s_uiChatMessage) - 1);
            s_uiChatMessage[sizeof(s_uiChatMessage) - 1] = '\0';
        }
    }

    // --- Dialog Builders ---
    ModResult BuildChatDialogPane(ModContext* ctx, UiElementHandle pane, void*, ModError*) {
        if (!g_ui) return MOD_OK;

        UiControlDesc desc = UI_CONTROL_DESC_INIT;
        desc.kind = UI_CONTROL_STRING;
        desc.label = "Message";
        desc.get = UiGetChat;
        desc.set = UiSetChat;
        desc.string_set_mode = UI_STRING_SET_ON_CHANGE;
        desc.max_length = 62;
        desc.tooltip = "Enter chat message to broadcast";
        g_ui->pane_add_control(ctx, pane, &desc, nullptr);

        return MOD_OK;
    }

    void OnSendChatAction(ModContext*, UiDialogHandle, void*) {
        SendChatMessage(s_uiChatMessage);
        s_activeChatDialog = 0;
    }

    ModResult BuildConnDialogPane(ModContext* ctx, UiElementHandle pane, void*, ModError*) {
        if (!g_ui) return MOD_OK;

        // Initialize from current config
        std::string curHost = ConnectionConfig::GetConfiguredHost("127.0.0.1");
        std::strncpy(s_uiHostBuffer, curHost.c_str(), sizeof(s_uiHostBuffer) - 1);
        s_uiPortValue = ConnectionConfig::GetConfiguredPort(1234);

        UiControlDesc hostDesc = UI_CONTROL_DESC_INIT;
        hostDesc.kind = UI_CONTROL_STRING;
        hostDesc.label = "Server Host / IP";
        hostDesc.get = UiGetHost;
        hostDesc.set = UiSetHost;
        hostDesc.string_set_mode = UI_STRING_SET_ON_CHANGE;
        hostDesc.max_length = 120;
        hostDesc.tooltip = "Enter host IP address or domain";
        g_ui->pane_add_control(ctx, pane, &hostDesc, nullptr);

        UiControlDesc portDesc = UI_CONTROL_DESC_INIT;
        portDesc.kind = UI_CONTROL_NUMBER;
        portDesc.label = "Server Port";
        portDesc.get = UiGetPort;
        portDesc.set = UiSetPort;
        portDesc.min = 1;
        portDesc.max = 65535;
        portDesc.step = 1;
        portDesc.tooltip = "Default multiplayer port is 1234";
        g_ui->pane_add_control(ctx, pane, &portDesc, nullptr);

        return MOD_OK;
    }

    void OnConnectDialogAction(ModContext*, UiDialogHandle, void*) {
        ConnectUsingUiConfig();
        s_activeConnDialog = 0;
    }

    // --- Mods Panel Builder (Dusklight host Mods window) ---
    ModResult BuildModsPanel(ModContext* ctx, UiElementHandle pane, void*, ModError*) {
        if (!g_ui) return MOD_OK;

        std::string curHost = ConnectionConfig::GetConfiguredHost("127.0.0.1");
        std::strncpy(s_uiHostBuffer, curHost.c_str(), sizeof(s_uiHostBuffer) - 1);
        s_uiPortValue = ConnectionConfig::GetConfiguredPort(1234);

        bool conn = g_client && g_client->IsConnected();
        bool connecting = g_client && g_client->IsConnecting();
        size_t peers = g_playerSync ? g_playerSync->GetRemotePlayers().size() : 0;
        uint8_t myId = (g_client && conn) ? g_client->GetPlayerID() : 0;

        char statusRml[512];
        if (conn) {
            std::snprintf(statusRml, sizeof(statusRml),
                "<p><b>Status:</b> Connected (Hero #%u) | <b>Session:</b> %zu player(s) online | <b>v0.1.1</b></p>",
                (unsigned int)myId, peers);
        } else if (connecting) {
            std::snprintf(statusRml, sizeof(statusRml),
                "<p><b>Status:</b> Connecting to %s:%u ... | <b>v0.1.1</b></p>",
                curHost.c_str(), (unsigned int)s_uiPortValue);
        } else {
            std::snprintf(statusRml, sizeof(statusRml),
                "<p><b>Status:</b> Offline | <b>Target:</b> %s:%u | <b>v0.1.1</b></p>",
                curHost.c_str(), (unsigned int)s_uiPortValue);
        }
        g_ui->pane_add_rml(ctx, pane, statusRml, nullptr);

        g_ui->pane_add_section(ctx, pane, "Multiplayer Server Connection");

        UiControlDesc hostDesc = UI_CONTROL_DESC_INIT;
        hostDesc.kind = UI_CONTROL_STRING;
        hostDesc.label = "Server Host / IP";
        hostDesc.get = UiGetHost;
        hostDesc.set = UiSetHost;
        hostDesc.string_set_mode = UI_STRING_SET_ON_CHANGE;
        hostDesc.max_length = 120;
        g_ui->pane_add_control(ctx, pane, &hostDesc, nullptr);

        UiControlDesc portDesc = UI_CONTROL_DESC_INIT;
        portDesc.kind = UI_CONTROL_NUMBER;
        portDesc.label = "Port";
        portDesc.get = UiGetPort;
        portDesc.set = UiSetPort;
        portDesc.min = 1;
        portDesc.max = 65535;
        g_ui->pane_add_control(ctx, pane, &portDesc, nullptr);

        UiControlDesc connBtn = UI_CONTROL_DESC_INIT;
        connBtn.kind = UI_CONTROL_BUTTON;
        connBtn.label = "Connect to Server";
        connBtn.on_pressed = [](ModContext*, void*) {
            ConnectUsingUiConfig();
        };
        g_ui->pane_add_control(ctx, pane, &connBtn, nullptr);

        UiControlDesc discBtn = UI_CONTROL_DESC_INIT;
        discBtn.kind = UI_CONTROL_BUTTON;
        discBtn.label = "Disconnect";
        discBtn.on_pressed = [](ModContext*, void*) {
            if (g_client) {
                g_client->Disconnect();
                PushNotification("Multiplayer", "Disconnected from server.");
            }
        };
        g_ui->pane_add_control(ctx, pane, &discBtn, nullptr);

        g_ui->pane_add_section(ctx, pane, "In-Game Chat");

        UiControlDesc chatInput = UI_CONTROL_DESC_INIT;
        chatInput.kind = UI_CONTROL_STRING;
        chatInput.label = "Chat Message";
        chatInput.get = UiGetChat;
        chatInput.set = UiSetChat;
        chatInput.string_set_mode = UI_STRING_SET_ON_CHANGE;
        g_ui->pane_add_control(ctx, pane, &chatInput, nullptr);

        UiControlDesc sendBtn = UI_CONTROL_DESC_INIT;
        sendBtn.kind = UI_CONTROL_BUTTON;
        sendBtn.label = "Send Message";
        sendBtn.on_pressed = [](ModContext*, void*) {
            SendChatMessage(s_uiChatMessage);
        };
        g_ui->pane_add_control(ctx, pane, &sendBtn, nullptr);

        g_ui->pane_add_section(ctx, pane, "Keyboard Shortcuts");
        g_ui->pane_add_text(ctx, pane,
            "F1: Help Guide | F2: Status | F5: Sync Test\n"
            "F6: Chat Dialog | F7: Radar Beacon | F8: Wolf/Human\n"
            "F9: Reconnect | F10: Reload 3D | F11: IP & Port Dialog", nullptr);

        return MOD_OK;
    }
}

DebugController::DebugController() {
    std::memset(m_keyStates, 0, sizeof(m_keyStates));

    // Load saved connection settings from disk
    ConnectionConfig::EnsureLoaded();
    std::string h = ConnectionConfig::GetConfiguredHost("127.0.0.1");
    std::strncpy(s_uiHostBuffer, h.c_str(), sizeof(s_uiHostBuffer) - 1);
    s_uiPortValue = ConnectionConfig::GetConfiguredPort(1234);
}

DebugController::~DebugController() {
}

void DebugController::RegisterModsPanel() {
    if (!g_ui) return;
    UiModsPanelDesc desc = UI_MODS_PANEL_DESC_INIT;
    desc.build = BuildModsPanel;
    g_ui->register_mods_panel(mod_ctx, &desc);
}

void DebugController::OpenChatDialog() {
    if (!g_ui) return;
    if (s_activeChatDialog != 0) return; // Already open

    static UiDialogAction actions[2];
    actions[0] = UI_DIALOG_ACTION_INIT;
    actions[0].label = "Send";
    actions[0].on_pressed = OnSendChatAction;
    actions[0].keep_open = false;

    actions[1] = UI_DIALOG_ACTION_INIT;
    actions[1].label = "Cancel";
    actions[1].on_pressed = [](ModContext*, UiDialogHandle, void*) {
        s_activeChatDialog = 0;
    };
    actions[1].keep_open = false;

    UiDialogDesc desc = UI_DIALOG_DESC_INIT;
    desc.title = "Multiplayer Chat";
    desc.body_rml = "Type a message to send to all heroes in the session:";
    desc.variant = UI_DIALOG_NORMAL;
    desc.actions = actions;
    desc.action_count = 2;
    desc.build = BuildChatDialogPane;
    desc.on_dismiss = [](ModContext*, UiDialogHandle, void*) {
        s_activeChatDialog = 0;
    };

    g_ui->dialog_push(mod_ctx, &desc, &s_activeChatDialog);
}

void DebugController::OpenConnectionDialog() {
    if (!g_ui) return;
    if (s_activeConnDialog != 0) return; // Already open

    static UiDialogAction actions[2];
    actions[0] = UI_DIALOG_ACTION_INIT;
    actions[0].label = "Connect";
    actions[0].on_pressed = OnConnectDialogAction;
    actions[0].keep_open = false;

    actions[1] = UI_DIALOG_ACTION_INIT;
    actions[1].label = "Cancel";
    actions[1].on_pressed = [](ModContext*, UiDialogHandle, void*) {
        s_activeConnDialog = 0;
    };
    actions[1].keep_open = false;

    UiDialogDesc desc = UI_DIALOG_DESC_INIT;
    desc.title = "Multiplayer Connection Settings";
    desc.body_rml = "Configure Server IP Address and Port to join a session:";
    desc.variant = UI_DIALOG_NORMAL;
    desc.actions = actions;
    desc.action_count = 2;
    desc.build = BuildConnDialogPane;
    desc.on_dismiss = [](ModContext*, UiDialogHandle, void*) {
        s_activeConnDialog = 0;
    };

    g_ui->dialog_push(mod_ctx, &desc, &s_activeConnDialog);
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
    // Check hotkeys
    if (JustPressed(VK_F1))  ShowHelpToast();
    if (JustPressed(VK_F2))  ShowStatusToast();
    if (JustPressed(VK_F5))  TestWorldSync();
    if (JustPressed(VK_F6))  OpenChatDialog();
    if (JustPressed(VK_F7))  PingRadarAndBeacon();
    if (JustPressed(VK_F8))  ToggleTransformForm();
    if (JustPressed(VK_F9))  ReconnectNetwork();
    if (JustPressed(VK_F10)) ReloadPuppetActors();
    if (JustPressed(VK_F11)) OpenConnectionDialog();
}

void DebugController::ShowHelpToast() {
    PushNotification(
        "Multiplayer Hotkeys Guide",
        "<b>F1</b>: Guide | <b>F2</b>: Mod Status<br/>"
        "<b>F5</b>: World Sync | <b>F6</b>: Chat Dialog (Type)<br/>"
        "<b>F7</b>: Radar Ping | <b>F8</b>: Wolf/Human Form<br/>"
        "<b>F9</b>: Reconnect | <b>F10</b>: Reload 3D<br/>"
        "<b>F11</b>: IP & Port Connection Settings Dialog",
        8000
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
        "Host: %s:%u<br/>"
        "Stage: %s (Rm:%d) | HP:%d/%d | Rup:%d<br/>"
        "Form: %s",
        conn ? "Connected" : (connecting ? "Connecting..." : "Offline"),
        (int)myId, remoteCount,
        ConnectionConfig::GetConfiguredHost("127.0.0.1").c_str(),
        ConnectionConfig::GetConfiguredPort(1234),
        stage, room, hp, maxHp, rupees,
        (form == 1) ? "Wolf" : "Human"
    );

    PushNotification("Multiplayer Diagnostics", statusBuf, 5500);
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

    if (g_worldSync) {
        g_worldSync->ApplyRemoteEvent(ev);
    }

    if (g_client && g_client->IsConnected()) {
        uint8_t myId = g_client->GetPlayerID();
        std::vector<uint8_t> evData = PacketSerializer::SerializeSyncWorldEvent(ev);
        evData.insert(evData.begin(), myId);
        evData.insert(evData.begin(), PACKET_WORLD_EVENT);
        g_client->Send(evData);
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
            return;
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
    ConnectUsingUiConfig();
}

void DebugController::ReloadPuppetActors() {
    if (!g_playerSync) return;

    g_playerSync->ResetAll();
    PushNotification("Multiplayer System", "3D Puppet Actors reloaded and re-initialized.");
}
