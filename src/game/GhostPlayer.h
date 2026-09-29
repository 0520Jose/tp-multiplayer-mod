#pragma once

#include <f_op/f_op_actor.h>
#include <mods/svc/actor.h>
#include <SSystem/SComponent/c_phase.h>
#include <cstdint>

class JPABaseEmitter;
class J3DModel;

#define DA_GHOST_PLAYER_NAME "m_ghost"

class daGhostPlayer_c : public fopAc_ac_c {
public:
    uint8_t m_networkPlayerId;
    JPABaseEmitter* m_particleEmitter;
    J3DModel* mpModel;
    J3DModel* mpHatModel;
    J3DModel* mpFaceModel;
    J3DModel* mpHandModel;

    daGhostPlayer_c();
    virtual ~daGhostPlayer_c();

    int create();
    int Delete();
    int Execute();
    int Draw();
    int CreateHeap();

    static int CreateHeapCallback(fopAc_ac_c* i_this);
    static int CreateCallback(void* i_this);
    static int DeleteCallback(void* i_this);
    static int ExecuteCallback(void* i_this);
    static int DrawCallback(void* i_this);
    static int IsDeleteCallback(void* i_this);

    static s16 sProcName;
    static ActorHandle sActorHandle;
    static const ActorProfileDesc sProfile;
};