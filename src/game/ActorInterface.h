#pragma once
#include "../network/NetworkTypes.h"
#include <cstdint>

struct fopAc_ac_c {
    float x;
    float y;
    float z;
    float rotY;
};

class PlayerSync {
public:
    PlayerSync();
    fopAc_ac_c* GetPlayerActor();
    SyncPositionPacket GetLocalPosition();
    void ApplyRemotePosition(float x, float y, float z, float rotY);
private:
    fopAc_ac_c* m_player;
};
