#pragma once
#include "../network/NetworkTypes.h"
#include <cstdint>

class fopAc_ac_c;
class JPABaseEmitter;

class PlayerSync {
public:
    PlayerSync();
    ~PlayerSync();
    
    // Updates local player position
    SyncPositionPacket GetLocalPosition();
    
    // Updates remote player position
    void ApplyRemotePosition(const SyncPositionPacket& posData);
    
    // Safely render the remote player dummy
    void RenderRemotePlayer3D();
    
    // Resets the emitter pointer (used during loading screens)
    void ResetEmitter() { m_remoteEmitter = nullptr; }
    
    fopAc_ac_c* GetPlayerActor();

    SyncStatusPacket GetLocalStatus();
    void ApplyRemoteStatus(int16_t health, int16_t maxHealth, uint32_t animationId);

private:
    void SpawnRemotePlayer();

    fopAc_ac_c* m_player;
    fopAc_ac_c* m_remotePlayer;
    JPABaseEmitter* m_remoteEmitter;
    SyncPositionPacket m_remotePosition;
    bool m_hasRemotePosition;
    int16_t m_remoteHealth;
    int16_t m_remoteMaxHealth;
};
