#define NOMINMAX
#include <enet/enet.h>
#include <mods/api.h>
#include <iostream>
#include "network/PacketSerializer.h"
#include "game/ActorInterface.h"
#include <mods/hook.hpp>
#include <mods/svc/log.h>
#include <cstdio>
#include <fstream>
#include <m_Do/m_Do_ext.h>

#include "network/Client.h"
#include "network/ConnectionConfig.h"

Client* g_client = nullptr;
PlayerSync* g_playerSync = nullptr;

void NetworkPumpCallback() {
    if (!g_client || !g_playerSync) return;

    if (!g_client->IsConnected()) {
        static int connectCooldown = 0;
        if (connectCooldown <= 0) {
            const std::string host = ConnectionConfig::GetConfiguredHost("127.0.0.1");
            const uint16_t port = ConnectionConfig::GetConfiguredPort(1234);
            g_client->Connect(host, port);
            connectCooldown = 60; 
        } else {
            connectCooldown--;
        }
        return;
    }

    auto packets = g_client->Update();
    for (const auto& data : packets) {
        if (data.empty()) continue;
        uint8_t type = data[0];
        
        std::vector<uint8_t> payload(data.begin() + 1, data.end());
        
        if (type == 0) {
            SyncPositionPacket posData = PacketSerializer::DeserializeSyncPosition(payload);
            g_playerSync->ApplyRemotePosition(posData);
        } else if (type == 1) {
            SyncStatusPacket s = PacketSerializer::DeserializeSyncStatus(payload);
            g_playerSync->ApplyRemoteStatus(s.health, s.maxHealth, s.currentAnimation);
        }
    }
}

void PlayerSyncCallback() {
    if (!g_client || !g_client->IsConnected() || !g_playerSync) return;
    
    // --- 2. SEND LOCAL STATE TO SERVER ---
    SyncPositionPacket posPacket = g_playerSync->GetLocalPosition();
    std::vector<uint8_t> posData = PacketSerializer::SerializeSyncPosition(posPacket);
    posData.insert(posData.begin(), 0); // Type 0 = Position
    g_client->Send(posData);
    
    SyncStatusPacket statusPacket = g_playerSync->GetLocalStatus();
    std::vector<uint8_t> statusData = PacketSerializer::SerializeSyncStatus(statusPacket);
    statusData.insert(statusData.begin(), 1); // Type 1 = Status
    g_client->Send(statusData);
    
    // Process the fairy particles (safe alternative to 3D models)
    g_playerSync->UpdateParticles();
}

bool g_isPlayerReady = false;

void ModelUpdateDLPost(J3DModel* i_model) {
    if (!g_playerSync || !g_isPlayerReady) return;
    
    // We only inject our model immediately after Link's model has been drawn
    fopAc_ac_c* alink = g_playerSync->GetPlayerActor();
    if (!alink) return;
    
    J3DModel* mpLinkModel = *(J3DModel**)((char*)alink + 0x650);
    
    if (i_model == mpLinkModel) {
        g_playerSync->CreateRemoteModelIfNeeded(alink);
        
        J3DModel* remoteModel = g_playerSync->GetRemoteModel();
        if (remoteModel) {
            // Update the matrix based on remote position before drawing
            g_playerSync->RenderRemotePlayer3D();
            
            // Render it using the current view projection
            mDoExt_modelUpdateDL(remoteModel);
        }
    }
}

void ModelUpdateDLPost_Wrapper(ModContext* ctx, void* args, void* retval) {
    J3DModel* i_model = *(J3DModel**)args;
    ModelUpdateDLPost(i_model);
}

#include <mods/service.hpp>

DEFINE_MOD()
IMPORT_SERVICE(HookService, g_hooks);
IMPORT_SERVICE(LogService, g_log);

extern "C" MOD_EXPORT ModResult mod_initialize(ModError* out_error) {
    if (enet_initialize() != 0) {
        return MOD_ERROR;
    }

    g_client = new Client();
    g_playerSync = new PlayerSync();
    
    // We removed the hooks because they crashed the J3D pipeline.
    // Wait, we are restoring them using the direct dusk API
    if (g_hooks) {
        // g_hooks->add_post(mod_ctx, (void*)0x80014a70, (HookPostFn)ModelUpdateDLPost_Wrapper, nullptr);
    }
    
    return MOD_OK;
}

extern "C" MOD_EXPORT ModResult mod_update(ModError* out_error) {
    NetworkPumpCallback();
    
    // Use a delay to ensure the game is completely loaded and we aren't in a loading screen
    static int framesSincePlayerLoaded = 0;
    
    // We check if the player actor is not null AND the save file is actually loaded (max health > 0)
    if (g_playerSync && g_playerSync->GetPlayerActor() != nullptr &&
        g_playerSync->GetLocalStatus().maxHealth > 0) {
        framesSincePlayerLoaded++;
        // Wait 120 frames (~4 seconds at 30fps) after player becomes valid before interacting with ActorManager
        if (framesSincePlayerLoaded > 120) {
            g_isPlayerReady = true;
            PlayerSyncCallback();
        }
    } else {
        framesSincePlayerLoaded = 0;
        g_isPlayerReady = false;
        if (g_playerSync) {
            g_playerSync->ResetEmitter();
        }
    }
    
    return MOD_OK;
}

extern "C" MOD_EXPORT ModResult mod_shutdown(ModError* out_error) {
    if (g_client) {
        g_client->Disconnect();
        delete g_client;
        g_client = nullptr;
    }
    if (g_playerSync) {
        delete g_playerSync;
        g_playerSync = nullptr;
    }
    enet_deinitialize();
    return MOD_OK;
}
