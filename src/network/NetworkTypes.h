#pragma once
#include <cstdint>

struct SyncPositionPacket {
    float x;
    float y;
    float z;
    float rotY;
};

struct SyncStatusPacket {
    int16_t health;
    int16_t maxHealth;
    uint32_t currentAnimation;
};
