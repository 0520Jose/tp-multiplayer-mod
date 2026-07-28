#include "ActorInterface.h"
#include <algorithm>
#include <cmath>
#include <d/d_com_inf_game.h>
#include <dolphin/gx/GXAurora.h>
#include <f_pc/f_pc_name.h>
#include <f_op/f_op_actor_mng.h>
#include <m_Do/m_Do_mtx.h>
#include <JSystem/JParticle/JPAEmitter.h>

PlayerSync::PlayerSync() {
    m_player = nullptr;
    m_remotePlayer = nullptr;
    m_remotePosition = {0.0f, 0.0f, 0.0f, 0.0f};
    m_hasRemotePosition = false;
    m_remoteHealth = 0;
    m_remoteMaxHealth = 1;
}

PlayerSync::~PlayerSync() {}

fopAc_ac_c* PlayerSync::GetPlayerActor() {
    return dComIfGp_getPlayer(0);
}

SyncPositionPacket PlayerSync::GetLocalPosition() {
    SyncPositionPacket packet;
    m_player = dComIfGp_getPlayer(0); // Update pointer every frame
    
    if (m_player) {
        packet.x = m_player->current.pos.x;
        packet.y = m_player->current.pos.y;
        packet.z = m_player->current.pos.z;
        packet.rotY = (float)m_player->current.angle.y;
    } else {
        packet.x = 0.0f;
        packet.y = 0.0f;
        packet.z = 0.0f;
        packet.rotY = 0.0f;
    }
    return packet;
}

void PlayerSync::SpawnRemotePlayer() {
    // Disabled to prevent actor manager crashes
}

void PlayerSync::ApplyRemotePosition(const SyncPositionPacket& posData) {
    m_remotePosition = posData;
    m_hasRemotePosition = true;
}

SyncStatusPacket PlayerSync::GetLocalStatus() {
    SyncStatusPacket packet;
    m_player = dComIfGp_getPlayer(0);
    
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

void PlayerSync::ApplyRemoteStatus(int16_t health, int16_t maxHealth, uint32_t animationId) {
    m_remoteHealth = health;
    m_remoteMaxHealth = (maxHealth > 0) ? maxHealth : 1;
}

void PlayerSync::RenderRemotePlayer3D() {
    if (m_hasRemotePosition) {
        cXyz pos;
        pos.x = m_remotePosition.x;
        pos.y = m_remotePosition.y + 100.0f; // Raise it slightly so it doesn't clip into the ground
        pos.z = m_remotePosition.z;

        if (!m_remoteEmitter) {
            // ID_ZF_J_FAIRY00_GLOW is 0x72F
            m_remoteEmitter = dComIfGp_particle_set(0x72F, &pos, nullptr, nullptr);
            if (!m_remoteEmitter) {
                // Try a different fairy particle if first fails
                m_remoteEmitter = dComIfGp_particle_set(0x01A, &pos, nullptr, nullptr); // ID_AK_JN_CUREFAIRY00
            }
            if (m_remoteEmitter) {
                m_remoteEmitter->becomeImmortalEmitter();
            }
        }

        if (m_remoteEmitter) {
            m_remoteEmitter->setGlobalTranslation(pos.x, pos.y, pos.z);
        }
    }
}

