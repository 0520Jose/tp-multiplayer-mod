#pragma once
#include "../network/NetworkTypes.h"
#include <cstdint>

struct fopAc_ac_c;

class PlayerSync {
public:
    PlayerSync();
    fopAc_ac_c* GetPlayerActor();
    SyncPositionPacket GetLocalPosition();
    void ApplyRemotePosition(float x, float y, float z, float rotY);
    
    SyncStatusPacket GetLocalStatus();
    void ApplyRemoteStatus(int16_t health, int16_t maxHealth, uint32_t animationId);
private:
    fopAc_ac_c* m_player;
};
