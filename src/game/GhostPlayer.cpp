#include "GhostPlayer.h"
#include "ActorInterface.h"
#include <d/d_com_inf_game.h>
#include <d/actor/d_a_alink.h>
#include <f_op/f_op_actor_mng.h>
#include <m_Do/m_Do_mtx.h>
#include <JSystem/J3DGraphAnimator/J3DModel.h>
#include <JSystem/J3DGraphAnimator/J3DJointTree.h>
#include <JSystem/J3DGraphAnimator/J3DJoint.h>
#include <JSystem/J3DGraphBase/J3DTexture.h>

extern PlayerSync* g_playerSync;

s16 daGhostPlayer_c::sProcName = -1;
ActorHandle daGhostPlayer_c::sActorHandle = 0;

static void SafeModelCalc(J3DModel* model) {
    if (!model) return;
    J3DModelData* modelData = model->getModelData();
    if (!modelData) return;

    u16 jointNum = modelData->getJointNum();
    constexpr u16 MAX_JOINTS = 128;
    J3DJointCallBack savedCallbacks[MAX_JOINTS] = {};
    u16 count = (jointNum < MAX_JOINTS) ? jointNum : MAX_JOINTS;

    // Temporarily clear callbacks on shared modelData joints to prevent
    // daAlink callbacks from attempting to dereference a null userArea
    for (u16 i = 0; i < count; ++i) {
        J3DJoint* joint = modelData->getJointNodePointer(i);
        if (joint) {
            savedCallbacks[i] = joint->getCallBack();
            joint->setCallBack(nullptr);
        }
    }

    model->calc();

    // Restore callbacks so local Link remains 100% unaffected
    for (u16 i = 0; i < count; ++i) {
        J3DJoint* joint = modelData->getJointNodePointer(i);
        if (joint) {
            joint->setCallBack(savedCallbacks[i]);
        }
    }
}

static void DisableMipmapsForModel(J3DModel*) {
    // No-op: Do not mutate shared ResTIMG structures to avoid conflicting with
    // Dusklight engine texture caches and the Cosmetics mod.
}

daGhostPlayer_c::daGhostPlayer_c() {
    m_networkPlayerId = 0xFF;
    m_particleEmitter = nullptr;
    mpModel = nullptr;
    mpHatModel = nullptr;
    mpFaceModel = nullptr;
    mpHandModel = nullptr;
    mpSwordModel = nullptr;
    mpShieldModel = nullptr;
}

daGhostPlayer_c::~daGhostPlayer_c() {
    if (m_particleEmitter) {
        m_particleEmitter->stopDrawParticle();
        m_particleEmitter = nullptr;
    }
    mpModel = nullptr;
    mpHatModel = nullptr;
    mpFaceModel = nullptr;
    mpHandModel = nullptr;
    mpSwordModel = nullptr;
    mpShieldModel = nullptr;
}

int daGhostPlayer_c::CreateHeap() {
    daAlink_c* localLink = (daAlink_c*)dComIfGp_getPlayer(0);
    if (!localLink || !localLink->mpLinkModel) {
        return 0;
    }

    J3DModelData* bodyData = localLink->mpLinkModel->getModelData();
    if (!bodyData) {
        return 0;
    }

    // 1. Body model (al.bmd / bl.bmd)
    mpModel = mDoExt_J3DModel__create(bodyData, 0x80000, 0x11000084);
    if (!mpModel) {
        return 0;
    }
    mpModel->setUserArea(0);

    // 2. Hat / Hair model (al_head.bmd / bl_head.bmd)
    if (localLink->mpLinkHatModel && localLink->mpLinkHatModel->getModelData()) {
        mpHatModel = mDoExt_J3DModel__create(localLink->mpLinkHatModel->getModelData(), 0x80000, 0x11000084);
        if (mpHatModel) {
            mpHatModel->setUserArea(0);
        }
    }

    // 3. Face / Expression model (al_face.bmd / zl_face.bmd)
    // Use 0x11000084 (matching other submodels) rather than Link's 0x11020284, as the puppet
    // does not run an mFaceBck dynamic material color/texture animator.
    if (localLink->mpLinkFaceModel && localLink->mpLinkFaceModel->getModelData()) {
        mpFaceModel = mDoExt_J3DModel__create(localLink->mpLinkFaceModel->getModelData(), 0x80000, 0x11000084);
        if (mpFaceModel) {
            mpFaceModel->setUserArea(0);
        }
    }

    // 4. Hand model (al_hands.bmd / bl_hands.bmd)
    if (localLink->mpLinkHandModel && localLink->mpLinkHandModel->getModelData()) {
        mpHandModel = mDoExt_J3DModel__create(localLink->mpLinkHandModel->getModelData(), 0x80000, 0x11000084);
        if (mpHandModel) {
            mpHandModel->setUserArea(0);
        }
    }

    // 5. Sword model (mpSwAModel / mpSwMModel / mWoodSwordModel)
    if (localLink->mSwordModel && localLink->mSwordModel->getModelData()) {
        mpSwordModel = mDoExt_J3DModel__create(localLink->mSwordModel->getModelData(), 0x80000, 0x11000084);
        if (mpSwordModel) {
            mpSwordModel->setUserArea(0);
        }
    }

    // 6. Shield model (mShieldModel)
    if (localLink->mShieldModel && localLink->mShieldModel->getModelData()) {
        mpShieldModel = mDoExt_J3DModel__create(localLink->mShieldModel->getModelData(), 0x80000, 0x11000084);
        if (mpShieldModel) {
            mpShieldModel->setUserArea(0);
        }
    }

    // Disable mipmaps on all submodels to eliminate distance blurriness & alpha-test dither
    DisableMipmapsForModel(mpModel);
    DisableMipmapsForModel(mpHatModel);
    DisableMipmapsForModel(mpFaceModel);
    DisableMipmapsForModel(mpHandModel);
    DisableMipmapsForModel(mpSwordModel);
    DisableMipmapsForModel(mpShieldModel);

    return 1;
}

int daGhostPlayer_c::CreateHeapCallback(fopAc_ac_c* i_this) {
    return static_cast<daGhostPlayer_c*>(i_this)->CreateHeap();
}

int daGhostPlayer_c::create() {
    fopAcM_ct(this, daGhostPlayer_c);

    // Network player ID is passed in the actor's spawn parameters
    m_networkPlayerId = static_cast<uint8_t>(fopAcM_GetParam(this));

    scale.x = 1.0f;
    scale.y = 1.0f;
    scale.z = 1.0f;

    // Initialize environment lighting / TEV struct for this actor
    dKy_tevstr_init(&tevStr, dComIfGp_roomControl_getStayNo(), 0xFF);

    // Allocate isolated actor solid heap (256KB) for the full character instance & equipment
    if (!fopAcM_entrySolidHeap(this, CreateHeapCallback, 0x40000)) {
        // Link model data not yet loaded into RAM; retry next frame
        return static_cast<int>(cPhs_INIT_e);
    }

    if (mpModel) {
        fopAcM_SetMtx(this, mpModel->getBaseTRMtx());
    }

    return static_cast<int>(cPhs_COMPLEATE_e);
}

int daGhostPlayer_c::Delete() {
    this->~daGhostPlayer_c();
    fopAcM_DeleteHeap(this);
    return 1;
}

int daGhostPlayer_c::Execute() {
    if (g_playerSync) {
        const auto& players = g_playerSync->GetRemotePlayers();
        auto it = players.find(m_networkPlayerId);
        if (it != players.end() && it->second.hasData) {
            current.pos.x = it->second.renderX;
            current.pos.y = it->second.renderY;
            current.pos.z = it->second.renderZ;
            current.angle.y = static_cast<s16>(it->second.renderRotY);
            shape_angle.y = static_cast<s16>(it->second.renderRotY);
        }
    }

    // Keep room number synced to local Link's current room
    fopAc_ac_c* localPlayer = dComIfGp_getPlayer(0);
    if (localPlayer) {
        fopAcM_SetRoomNo(this, fopAcM_GetRoomNo(localPlayer));
        tevStr.room_no = fopAcM_GetRoomNo(localPlayer);
    }

    if (mpModel) {
        mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
        mDoMtx_stack_c::YrotM(shape_angle.y);
        mDoMtx_stack_c::scaleM(scale);
        mpModel->setBaseTRMtx(mDoMtx_stack_c::get());

        // 1. Calculate body joints during simulation frame so Dusklight records interpolation
        SafeModelCalc(mpModel);

        // 2. Hat / Hair (al_head.bmd) attached to Head joint (4)
        if (mpHatModel) {
            mpHatModel->setBaseTRMtx(mpModel->getAnmMtx(4));
            SafeModelCalc(mpHatModel);
        }

        // 3. Face (al_face.bmd) attached to Head joint (4)
        if (mpFaceModel) {
            mpFaceModel->setBaseTRMtx(mpModel->getAnmMtx(4));
            SafeModelCalc(mpFaceModel);
        }

        // 4. Hands (al_hands.bmd) attached to Left Wrist (9) and Right Wrist (14 / 0xE)
        if (mpHandModel) {
            mpHandModel->setBaseTRMtx(mpModel->getBaseTRMtx());
            SafeModelCalc(mpHandModel);
            mpHandModel->setAnmMtx(1, mpModel->getAnmMtx(9));
            mpHandModel->setAnmMtx(2, mpModel->getAnmMtx(0xE));
        }

        // 5. Sword attached to Right Hand (Wrist joint 14 / 0xE)
        if (mpSwordModel) {
            mpSwordModel->setBaseTRMtx(mpModel->getAnmMtx(0xE));
            SafeModelCalc(mpSwordModel);
        }

        // 6. Shield attached to Left Hand (Wrist joint 9)
        if (mpShieldModel) {
            mpShieldModel->setBaseTRMtx(mpModel->getAnmMtx(9));
            SafeModelCalc(mpShieldModel);
        }

        // Keep cull matrix up to date with body transform
        fopAcM_SetMtx(this, mpModel->getBaseTRMtx());
    }

    return 1;
}

int daGhostPlayer_c::Draw() {
    if (mpModel) {
        daAlink_c* localLink = (daAlink_c*)dComIfGp_getPlayer(0);
        if (localLink) {
            tevStr = localLink->tevStr;
        } else {
            g_env_light.settingTevStruct(0, &current.pos, &tevStr);
        }

        // Disable GX fog cleanly: in GXSetFog (GXPixel.cpp line 27), when startZ == endZ,
        // the fog coefficients aCoeff and cCoeff evaluate to 0.0f, eliminating fog blending completely
        // without overflowing fixed-point registers (which happened when using 999999 / 1000000).
        tevStr.mFogStartZ = 0.0f;
        tevStr.mFogEndZ   = 0.0f;

        // Reset TevColor to neutral 0 as in daAlink_c::initTevCustomColor().
        // In Twilight Princess, TevColor is an additive flash register (setting it to 255
        // caused the blinding white glow; 0 keeps native texture colors).
        // Crucially, we do NOT overwrite AmbCol, preserving the natural ambient shading.
        tevStr.TevColor.r = 0;
        tevStr.TevColor.g = 0;
        tevStr.TevColor.b = 0;
        tevStr.TevColor.a = 0;
        tevStr.TevKColor.r = 0;
        tevStr.TevKColor.g = 0;
        tevStr.TevKColor.b = 0;
        tevStr.TevKColor.a = 0;

        // Re-enforce mipmap disabling in case textures were reloaded/reset
        DisableMipmapsForModel(mpModel);
        DisableMipmapsForModel(mpHandModel);
        DisableMipmapsForModel(mpHatModel);
        DisableMipmapsForModel(mpFaceModel);
        DisableMipmapsForModel(mpSwordModel);
        DisableMipmapsForModel(mpShieldModel);

        // Helper lambda to apply lighting and entry directly (matching daAlink_c::basicModelDraw)
        auto DrawSubModel = [&](J3DModel* subModel) {
            if (!subModel) return;
            g_env_light.setLightTevColorType_MAJI(subModel, &tevStr);
            mDoExt_modelEntryDL(subModel);
        };

        // 1. Draw Body (al.bmd / bl.bmd / wl.bmd)
        DrawSubModel(mpModel);

        // 2. Draw Hands (al_hands.bmd / bl_hands.bmd)
        DrawSubModel(mpHandModel);

        // 3. Draw Hat / Hair (al_head.bmd / bl_head.bmd)
        DrawSubModel(mpHatModel);

        // 4. Draw Face (al_face.bmd / zl_face.bmd)
        DrawSubModel(mpFaceModel);

        // 5. Draw Sword & Shield
        DrawSubModel(mpSwordModel);
        DrawSubModel(mpShieldModel);
    }
    return 1;
}

int daGhostPlayer_c::CreateCallback(void* i_this) {
    return static_cast<daGhostPlayer_c*>(i_this)->create();
}

int daGhostPlayer_c::DeleteCallback(void* i_this) {
    return static_cast<daGhostPlayer_c*>(i_this)->Delete();
}

int daGhostPlayer_c::ExecuteCallback(void* i_this) {
    return static_cast<daGhostPlayer_c*>(i_this)->Execute();
}

int daGhostPlayer_c::DrawCallback(void* i_this) {
    return static_cast<daGhostPlayer_c*>(i_this)->Draw();
}

int daGhostPlayer_c::IsDeleteCallback(void*) {
    return 1;
}

const ActorProfileDesc daGhostPlayer_c::sProfile = {
    .name = DA_GHOST_PLAYER_NAME,
    .priority_group = 7,  // Executed alongside general world actors, after Link
    .process_size = sizeof(daGhostPlayer_c),
    .draw_priority = fpcDwPi_ALINK_e,
    .status = fopAcStts_UNK_0x40000_e | fopAcStts_NOPAUSE_e | fopAcStts_FREEZE_e,
    .group = fopAc_ACTOR_e,
    .cull_type = fopAc_CULLBOX_0_e,
    .create_function = daGhostPlayer_c::CreateCallback,
    .delete_function = daGhostPlayer_c::DeleteCallback,
    .execute_function = daGhostPlayer_c::ExecuteCallback,
    .is_delete_function = daGhostPlayer_c::IsDeleteCallback,
    .draw_function = daGhostPlayer_c::DrawCallback,
};
