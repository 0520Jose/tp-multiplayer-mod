#pragma once
#include <f_op/f_op_actor.h>
#include <mods/svc/actor.h>
#include <SSystem/SComponent/c_phase.h>
#include <cstdint>

class JPABaseEmitter;
class J3DModel;

#define DA_GHOST_PLAYER_NAME "m_ghost"

// =============================================================================
// daGhostPlayer_c — Multiplayer Remote Player Puppet Actor
// =============================================================================
// Unlike daAlink_c (which is hardcoded as a single-player singleton that hijacks
// dComIfGp_getPlayer(0), reads pad 0 directly, and nulls player[0] on delete),
// daGhostPlayer_c is a lightweight puppet actor registered via Dusklight's
// ActorService.
//
// Key architectural benefits:
//   1. Never touches dComIfGp_getPlayer(0) — camera remains 100% on local Link.
//   2. Never reads or writes mDoCPd_c::m_cpadInfo[0] — zero input poisoning.
//   3. Never mutates global save data, inventory, or world event flags.
//   4. Clean deletion without triggering dComIfGp_setPlayer(0, NULL) crash.
//   5. Runs in priority group 7 (alongside general objects, after Link).
// =============================================================================

class daGhostPlayer_c : public fopAc_ac_c {
public:
    uint8_t m_networkPlayerId;
    JPABaseEmitter* m_particleEmitter;
    J3DModel* mpModel;      // Body (al.bmd)
    J3DModel* mpHatModel;   // Hat / Hair (al_head.bmd)
    J3DModel* mpFaceModel;  // Face / Eyes (al_face.bmd)
    J3DModel* mpHandModel;  // Hands / Gloves (al_hands.bmd)

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
