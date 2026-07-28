#include "ActorInterface.h"
#include <d/d_com_inf_game.h>
#include <f_pc/f_pc_name.h>
#include <f_op/f_op_actor_mng.h>

PlayerSync::PlayerSync() {
    m_player = nullptr;
    m_remotePlayer = nullptr;
}

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
    if (m_remotePlayer) return;

    m_player = dComIfGp_getPlayer(0);
    if (!m_player) {
        return; // Wait until local player is in the game before spawning remote player
    }

    cXyz spawnPos = m_player->current.pos;
    csXyz spawnRot = m_player->current.angle;
    int roomNo = m_player->current.roomNo;

    // Temporarily disabled to prevent crashes!
    // m_remotePlayer = fopAcM_fastCreate(fpcNm_ITEM_e, 0, &spawnPos, roomNo, &spawnRot, nullptr, -1, nullptr, nullptr);
    m_remotePlayer = nullptr;
}

void PlayerSync::ApplyRemotePosition(float x, float y, float z, float rotY) {
    if (!m_remotePlayer) {
        SpawnRemotePlayer();
    }
    
    if (m_remotePlayer) {
        m_remotePlayer->current.pos.x = x;
        m_remotePlayer->current.pos.y = y;
        m_remotePlayer->current.pos.z = z;
        m_remotePlayer->current.angle.y = (s16)rotY;
    }
}

SyncStatusPacket PlayerSync::GetLocalStatus() {
    SyncStatusPacket packet;
    m_player = dComIfGp_getPlayer(0);
    
    if (m_player) {
        packet.health = m_player->health;
        // Since we don't have maxHealth in fopAc_ac_c easily accessible, let's pull it from save data if possible
        // Let's just use dComIfGs_getLife() and dComIfGs_getMaxLife() instead of m_player fields!
        packet.health = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getLife();
        packet.maxHealth = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getMaxLife();
        
        // For animation ID, it's not simply exposed on fopAc_ac_c, 
        // we'll send a dummy value for now until we cast to daPy_py_c
        packet.currentAnimation = 0; 
    } else {
        packet.health = 0;
        packet.maxHealth = 0;
        packet.currentAnimation = 0;
    }
    return packet;
}

void PlayerSync::ApplyRemoteStatus(int16_t health, int16_t maxHealth, uint32_t animationId) {
    if (m_remotePlayer) {
        m_remotePlayer->health = health;
        // Animation syncing will require casting to daPy_py_c and calling changeDemoMoveAngle or similar,
        // which we can expand on later once we verify the actor spawned successfully.
    }
}


