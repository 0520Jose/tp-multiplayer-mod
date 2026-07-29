#include "ActorInterface.h"
#include <algorithm>
#include <cmath>
#include <d/d_com_inf_game.h>
#include <dolphin/gx/GXAurora.h>
#include <m_Do/m_Do_ext.h>
#include <f_pc/f_pc_name.h>
#include <f_op/f_op_actor_mng.h>

extern PlayerSync* g_playerSync;

PlayerSync::PlayerSync() {
    m_player = nullptr;
    m_remoteEmitter = nullptr;
    m_directionEmitter = nullptr;
    m_remotePosition = {0.0f, 0.0f, 0.0f, 0.0f};
    m_hasRemotePosition = false;
    m_remoteModel = nullptr;
    m_remoteHealth = 0;
    m_remoteMaxHealth = 1;
}

PlayerSync::~PlayerSync() {}

fopAc_ac_c* PlayerSync::GetPlayerActor() {
    return dComIfGp_getPlayer(0);
}

SyncPositionPacket PlayerSync::GetLocalPosition() {
    SyncPositionPacket packet;
    m_player = dComIfGp_getPlayer(0);
    
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

void PlayerSync::CreateRemoteModelIfNeeded(fopAc_ac_c* alink) {
    if (!alink) return;
    J3DModel* mpLinkModel = *(J3DModel**)((char*)alink + 0x650);
    
    if (!m_remoteModel && mpLinkModel) {
        // Allocate a new J3DModel using the game heap during update phase.
        // Using typical flags for characters: 0x80000 (differed DL) and 0x11000084.
        m_remoteModel = mDoExt_J3DModel__create(mpLinkModel->getModelData(), 0x80000, 0x11000084);
    }
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
    if (!m_hasRemotePosition) return;
    
    // We update the cloned model's matrix here during the update phase
    if (m_remoteModel) {
        Mtx newMtx;
        // Construct Translation + Y-Rotation matrix manually to avoid missing DLL exports
        float rad = m_remotePosition.rotY * (3.14159265f / 32768.0f);
        float s = std::sin(rad);
        float c = std::cos(rad);

        newMtx[0][0] = c;    newMtx[0][1] = 0.0f; newMtx[0][2] = s;    newMtx[0][3] = m_remotePosition.x;
        newMtx[1][0] = 0.0f; newMtx[1][1] = 1.0f; newMtx[1][2] = 0.0f; newMtx[1][3] = m_remotePosition.y;
        newMtx[2][0] = -s;   newMtx[2][1] = 0.0f; newMtx[2][2] = c;    newMtx[2][3] = m_remotePosition.z;

        m_remoteModel->setBaseTRMtx(newMtx);
        m_remoteModel->calc();
    }
}

void PlayerSync::UpdateParticles() {
    if (!m_hasRemotePosition) return;
    
    // Posición del hada principal (cuerpo del jugador) + 100 Y para que esté al nivel del pecho
    cXyz pos(m_remotePosition.x, m_remotePosition.y + 100.0f, m_remotePosition.z);
    
    // Hada direccional para indicar hacia donde mira
    cXyz dirPos = pos;
    // Convertir rotY (0-65535) a radianes para calcular el offset direccional
    float angleRad = (float)m_remotePosition.rotY * (3.14159265f / 32768.0f);
    dirPos.x += std::sin(angleRad) * 50.0f;
    dirPos.z += std::cos(angleRad) * 50.0f;
    dirPos.y += 30.0f;

    // Hada Luminosa principal (ID 0x072F según context.md)
    if (m_remoteEmitter == nullptr) {
        m_remoteEmitter = dComIfGp_particle_set(0x072F, &pos, nullptr, nullptr);
    } else {
        m_remoteEmitter->setGlobalTranslation(pos.x, pos.y, pos.z);
    }
    
    // Hada direccional (ID 0x01A según context.md)
    if (m_directionEmitter == nullptr) {
        m_directionEmitter = dComIfGp_particle_set(0x001A, &dirPos, nullptr, nullptr);
    } else {
        m_directionEmitter->setGlobalTranslation(dirPos.x, dirPos.y, dirPos.z);
    }
}

void PlayerSync::ResetEmitter() {
    if (m_remoteEmitter) {
        m_remoteEmitter->becomeInvalidEmitter();
        m_remoteEmitter = nullptr;
    }
    if (m_directionEmitter) {
        m_directionEmitter->becomeInvalidEmitter();
        m_directionEmitter = nullptr;
    }
    m_hasRemotePosition = false;
}
