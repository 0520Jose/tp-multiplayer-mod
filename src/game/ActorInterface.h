#pragma once
#include "../network/NetworkTypes.h"
#include <cstdint>

class fopAc_ac_c;
class JPABaseEmitter;
class J3DModel;

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
    
    // Updates the particle positions
    void UpdateParticles();
    
    // Resets the emitter pointer (used during loading screens)
    void ResetEmitter();
    
    fopAc_ac_c* GetPlayerActor();
    
    J3DModel* GetRemoteModel() { return m_remoteModel; }
    void CreateRemoteModelIfNeeded(fopAc_ac_c* alink);

    SyncStatusPacket GetLocalStatus();
    void ApplyRemoteStatus(int16_t health, int16_t maxHealth, uint32_t animationId);

private:
    fopAc_ac_c* m_player;
    JPABaseEmitter* m_remoteEmitter;
    JPABaseEmitter* m_directionEmitter;
    SyncPositionPacket m_remotePosition;
    bool m_hasRemotePosition;
    J3DModel* m_remoteModel;
    int16_t m_remoteHealth;
    int16_t m_remoteMaxHealth;
};
