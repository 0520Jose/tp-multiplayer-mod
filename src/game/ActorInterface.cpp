#include "ActorInterface.h"

static fopAc_ac_c g_localPlayerMock = {0.0f, 0.0f, 0.0f, 0.0f};

PlayerSync::PlayerSync() {
    m_player = &g_localPlayerMock;
}

fopAc_ac_c* PlayerSync::GetPlayerActor() {
    return m_player;
}

SyncPositionPacket PlayerSync::GetLocalPosition() {
    SyncPositionPacket packet;
    if (m_player) {
        packet.x = m_player->x;
        packet.y = m_player->y;
        packet.z = m_player->z;
        packet.rotY = m_player->rotY;
    } else {
        packet.x = 0.0f;
        packet.y = 0.0f;
        packet.z = 0.0f;
        packet.rotY = 0.0f;
    }
    return packet;
}

void PlayerSync::ApplyRemotePosition(float x, float y, float z, float rotY) {
    if (m_player) {
        m_player->x = x;
        m_player->y = y;
        m_player->z = z;
        m_player->rotY = rotY;
    }
}
