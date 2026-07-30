#include "ActorInterface.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <d/d_com_inf_game.h>
#include <dolphin/gx/GXAurora.h>
#include <m_Do/m_Do_ext.h>
#include <f_pc/f_pc_name.h>
#include <f_op/f_op_actor_mng.h>
#include "../network/Client.h"

extern Client* g_client;
extern PlayerSync* g_playerSync;

PlayerSync::PlayerSync() {
    m_player = nullptr;
    m_realPlayer = nullptr;
    m_realPlayerID = 0xFFFFFFFF;
    m_remoteModel = nullptr;
}

PlayerSync::~PlayerSync() {
    ResetAll();
}

fopAc_ac_c* PlayerSync::GetPlayerActor() {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);

    bool isGhost = false;
    for (auto& pair : m_remotePlayers) {
        if (player && fopAcM_GetID(player) == pair.second.actorID) {
            isGhost = true;
            break;
        }
    }

    if (isGhost) {
        if (m_realPlayer && fopAcM_SearchByID(m_realPlayerID) != nullptr) {
            dComIfGp_setPlayer(0, m_realPlayer);
            dComIfGp_setPlayerPtr(0, m_realPlayer);
            player = m_realPlayer;
        } else {
            m_realPlayer = nullptr;
            m_realPlayerID = 0xFFFFFFFF;
            player = nullptr;
        }
    } else {
        m_realPlayer = player;
        if (player) m_realPlayerID = fopAcM_GetID(player);
        else m_realPlayerID = 0xFFFFFFFF;
    }

    if (!player) return nullptr;
    if (fpcM_GetProfName(player) != fpcNm_ALINK_e) return nullptr;
    return player;
}

bool PlayerSync::IsOnTitleScreen() {
    return false;
}

SyncPositionPacket PlayerSync::GetLocalPosition() {
    SyncPositionPacket packet;
    packet.x = packet.y = packet.z = packet.rotY = 0.0f;
    std::memset(packet.stageName, 0, 8);
    packet.roomNo = 0;

    m_player = GetPlayerActor();
    if (m_player) {
        packet.x = m_player->current.pos.x;
        packet.y = m_player->current.pos.y;
        packet.z = m_player->current.pos.z;
        packet.rotY = m_player->current.angle.y;
        
        const char* stage = dComIfGp_getStartStageName();
        if (stage) std::strncpy(packet.stageName, stage, 8);
        packet.roomNo = 0; // Temporarily disabled due to linker error
    }
    return packet;
}

SyncStatusPacket PlayerSync::GetLocalStatus() {
    SyncStatusPacket packet;
    m_player = GetPlayerActor();
    if (m_player) {
        packet.health = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getLife();
        packet.maxHealth = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getMaxLife();
        packet.currentAnimation = 0; 
    } else {
        packet.health = 0;
        packet.maxHealth = 0;
        packet.currentAnimation = 0;
    }
    return packet;
}

float PlayerSync::Lerp(float a, float b, float t) {
    return a + t * (b - a);
}

float PlayerSync::LerpAngle(float a, float b, float t) {
    float diff = std::fmod(b - a + 32768.0f, 65536.0f);
    if (diff < 0) diff += 65536.0f;
    diff -= 32768.0f;
    return a + diff * t;
}

void PlayerSync::ApplyRemotePosition(uint8_t playerID, const SyncPositionPacket& posData) {
    auto& state = m_remotePlayers[playerID];
    
    if (!state.hasData) {
        state.renderX = state.prevX = posData.x;
        state.renderY = state.prevY = posData.y;
        state.renderZ = state.prevZ = posData.z;
        state.renderRotY = state.prevRotY = posData.rotY;
        state.hasData = true;
    } else {
        state.prevX = state.renderX;
        state.prevY = state.renderY;
        state.prevZ = state.renderZ;
        state.prevRotY = state.renderRotY;
    }
    
    state.targetX = posData.x;
    state.targetY = posData.y;
    state.targetZ = posData.z;
    state.targetRotY = posData.rotY;
    std::memcpy(state.stageName, posData.stageName, 8);
    state.roomNo = posData.roomNo;
    
    state.lerpT = 0.0f;
    state.framesIdle = 0;
}

void PlayerSync::ApplyRemoteStatus(uint8_t playerID, int16_t health, int16_t maxHealth, uint32_t animationId) {
    auto& state = m_remotePlayers[playerID];
    state.health = health;
    state.maxHealth = maxHealth;
    state.animationId = animationId;
}

void PlayerSync::RemoveRemotePlayer(uint8_t playerID) {
    auto it = m_remotePlayers.find(playerID);
    if (it != m_remotePlayers.end()) {
        if (it->second.actorID != 0xFFFFFFFF) {
            fopAc_ac_c* ghost = fopAcM_SearchByID(it->second.actorID);
            if (ghost) fopAcM_delete(ghost);
        }
        m_remotePlayers.erase(it);
    }
}

void PlayerSync::ResetAll() {
    for (auto& pair : m_remotePlayers) {
        if (pair.second.actorID != 0xFFFFFFFF) {
            fopAc_ac_c* ghost = fopAcM_SearchByID(pair.second.actorID);
            if (ghost) fopAcM_delete(ghost);
        }
    }
    m_remotePlayers.clear();
}

void PlayerSync::UpdateAllRemotePlayers() {
    const char* currentStage = dComIfGp_getStartStageName();

    for (auto it = m_remotePlayers.begin(); it != m_remotePlayers.end();) {
        auto& state = it->second;
        state.framesIdle++;

        if (state.framesIdle > IDLE_TIMEOUT_FRAMES) {
            if (state.actorID != 0xFFFFFFFF) {
                fopAc_ac_c* ghost = fopAcM_SearchByID(state.actorID);
                if (ghost) fopAcM_delete(ghost);
            }
            it = m_remotePlayers.erase(it);
            continue;
        }

        if (state.hasData) {
            if (state.lerpT < 1.0f) {
                state.lerpT += LERP_SPEED;
                if (state.lerpT > 1.0f) state.lerpT = 1.0f;
            }
            state.renderX = Lerp(state.prevX, state.targetX, state.lerpT);
            state.renderY = Lerp(state.prevY, state.targetY, state.lerpT);
            state.renderZ = Lerp(state.prevZ, state.targetZ, state.lerpT);
            state.renderRotY = LerpAngle(state.prevRotY, state.targetRotY, state.lerpT);

            // Stage isolation check
            bool sameStage = currentStage && (std::strncmp(state.stageName, currentStage, 8) == 0);
            bool visible = sameStage;

            if (visible) {
                if (state.actorID == 0xFFFFFFFF) {
                    cXyz pos(state.renderX, state.renderY, state.renderZ);
                    csXyz rot(0, (s16)state.renderRotY, 0);
                    state.actorID = fopAcM_create(fpcNm_ALINK_e, 0, &pos, 0, &rot, nullptr, -1);
                } else {
                    fopAc_ac_c* ghost = fopAcM_SearchByID(state.actorID);
                    if (ghost) {
                        // Position logic handled in hooks.
                    } else {
                        state.actorID = 0xFFFFFFFF; // It was deleted by the engine (e.g. stage transition)
                    }
                }
            } else {
                if (state.actorID != 0xFFFFFFFF) {
                    fopAc_ac_c* ghost = fopAcM_SearchByID(state.actorID);
                    if (ghost) fopAcM_delete(ghost);
                    state.actorID = 0xFFFFFFFF;
                }
            }
        }
        ++it;
    }
}

void PlayerSync::RenderPlayerParticle(uint8_t playerID, RemotePlayerState& state) {
    if (!state.hasData || state.actorID == 0xFFFFFFFF) return;
    
    fopAc_ac_c* ghost = fopAcM_SearchByID(state.actorID);
    if (ghost) {
        ghost->current.pos.x = state.renderX;
        ghost->current.pos.y = state.renderY;
        ghost->current.pos.z = state.renderZ;
        ghost->current.angle.y = (s16)state.renderRotY;
        ghost->shape_angle.y = (s16)state.renderRotY;
    }
}

void PlayerSync::CreateRemoteModelIfNeeded(fopAc_ac_c* alink) { }
