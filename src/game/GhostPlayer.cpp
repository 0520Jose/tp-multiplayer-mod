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

extern daAlink_c* localLink; // Necesario para la lgica del usuario
extern PlayerSync* g_playerSync;

static void SafeModelCalc(J3DModel* model) {
    if (!model) return;
    J3DModelData* modelData = model->getModelData();
    if (!modelData) return;

    u16 jointNum = modelData->getJointNum();
    constexpr u16 MAX_JOINTS = 128;
    J3DJointCallBack savedCallbacks[MAX_JOINTS] = {};
    u16 count = (jointNum < MAX_JOINTS) ? jointNum : MAX_JOINTS;

    for (u16 i = 0; i < count; ++i) {
        J3DJoint* joint = modelData->getJointNodePointer(i);
        if (joint) {
            savedCallbacks[i] = joint->getCallBack();
            joint->setCallBack(nullptr);
        }
    }
    model->calc();
    for (u16 i = 0; i < count; ++i) {
        J3DJoint* joint = modelData->getJointNodePointer(i);
        if (joint) {
            joint->setCallBack(savedCallbacks[i]);
        }
    }
}

static void ClearJointCallbacks(J3DModel* model, J3DJointCallBack* savedCallbacks) {
    if (!model) return;
    J3DModelData* modelData = model->getModelData();
    if (!modelData) return;
    u16 count = modelData->getJointNum();
    if (count > 128) count = 128;
    for (u16 i = 0; i < count; ++i) {
        J3DJoint* joint = modelData->getJointNodePointer(i);
        if (joint) {
            savedCallbacks[i] = joint->getCallBack();
            joint->setCallBack(nullptr);
        }
    }
}

static void RestoreJointCallbacks(J3DModel* model, J3DJointCallBack* savedCallbacks) {
    if (!model) return;
    J3DModelData* modelData = model->getModelData();
    if (!modelData) return;
    u16 count = modelData->getJointNum();
    if (count > 128) count = 128;
    for (u16 i = 0; i < count; ++i) {
        J3DJoint* joint = modelData->getJointNodePointer(i);
        if (joint) {
            joint->setCallBack(savedCallbacks[i]);
        }
    }
}

static void DisableMipmapsForModel(J3DModel* model) {
    if (!model) return;
    J3DModelData* modelData = model->getModelData();
    if (!modelData) return;
    J3DTexture* tex = modelData->getTexture();
    if (!tex) return;

    for (u16 i = 0; i < tex->getNum(); ++i) {
        ResTIMG* timg = tex->getResTIMG(i);
        if (timg) {
            timg->maxLOD = 0;
            timg->minFilter = 1; 
#if TARGET_PC
            tex->loadGXTexObj(i);
#endif
        }
    }
}

daGhostPlayer_c::daGhostPlayer_c()
    : m_networkPlayerId(0), m_particleEmitter(nullptr),
      mpModel(nullptr), mpHatModel(nullptr), mpFaceModel(nullptr), mpHandModel(nullptr) {
}

daGhostPlayer_c::~daGhostPlayer_c() {
    mpModel = nullptr;
    mpHatModel = nullptr;
    mpFaceModel = nullptr;
    mpHandModel = nullptr;
    m_particleEmitter = nullptr;
}

int daGhostPlayer_c::CreateHeap() {
    daAlink_c* localPlayer = (daAlink_c*)dComIfGp_getPlayer(0);
    if (!localPlayer || !localPlayer->mpLinkModel) {
        return 0;
    }

    J3DModelData* bodyData = localPlayer->mpLinkModel->getModelData();
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
    if (localPlayer->mpLinkHatModel && localPlayer->mpLinkHatModel->getModelData()) {
        mpHatModel = mDoExt_J3DModel__create(localPlayer->mpLinkHatModel->getModelData(), 0x80000, 0x11000084);
        if (mpHatModel) {
            mpHatModel->setUserArea(0);
        }
    }

    // 3. Face / Expression model (al_face.bmd / zl_face.bmd)
    if (localPlayer->mpLinkFaceModel && localPlayer->mpLinkFaceModel->getModelData()) {
        mpFaceModel = mDoExt_J3DModel__create(localPlayer->mpLinkFaceModel->getModelData(), 0x80000, 0x11000084);
        if (mpFaceModel) {
            mpFaceModel->setUserArea(0);
        }
    }

    // 4. Hand model (al_hands.bmd / bl_hands.bmd)
    if (localPlayer->mpLinkHandModel && localPlayer->mpLinkHandModel->getModelData()) {
        mpHandModel = mDoExt_J3DModel__create(localPlayer->mpLinkHandModel->getModelData(), 0x80000, 0x11000084);
        if (mpHandModel) {
            mpHandModel->setUserArea(0);
        }
    }

    DisableMipmapsForModel(mpModel);
    DisableMipmapsForModel(mpHatModel);
    DisableMipmapsForModel(mpFaceModel);
    DisableMipmapsForModel(mpHandModel);

    return 1;
}

int daGhostPlayer_c::create() {
    m_networkPlayerId = static_cast<uint8_t>(fopAcM_GetParam(this));
    m_particleEmitter = nullptr;
    mpModel = nullptr;
    mpHatModel = nullptr;
    mpFaceModel = nullptr;
    mpHandModel = nullptr;

    scale.x = 1.0f;
    scale.y = 1.0f;
    scale.z = 1.0f;

    dKy_tevstr_init(&tevStr, dComIfGp_roomControl_getStayNo(), 0xFF);
    
    tevStr.mFogStartZ = 0.0f;
    tevStr.mFogEndZ = 0.0f;
    
    tevStr.TevColor.r = 255;
    tevStr.TevColor.g = 255;
    tevStr.TevColor.b = 255;
    tevStr.TevColor.a = 255;
    
    tevStr.TevKColor.r = 0;
    tevStr.TevKColor.g = 0;
    tevStr.TevKColor.b = 0;
    tevStr.TevKColor.a = 0;
    
    tevStr.AmbCol.r = 255;
    tevStr.AmbCol.g = 255;
    tevStr.AmbCol.b = 255;
    tevStr.AmbCol.a = 255;

    // Solid heap allocation for character submodels (192 KB)
    if (!fopAcM_entrySolidHeap(this, (heapCallbackFunc)CreateHeapCallback, 0x30000)) {
        // Models not loaded in memory yet; retry next frame
        return static_cast<int>(cPhs_INIT_e);
    }

    if (mpModel) {
        fopAcM_SetMtx(this, mpModel->getBaseTRMtx());
    }

    fopAcM_setCullSizeBox(this, -300.0f, -0.0f, -300.0f, 300.0f, 300.0f, 300.0f);

    return cPhs_COMPLEATE_e;
}

int daGhostPlayer_c::Execute() {
    m_networkPlayerId = static_cast<uint8_t>(fopAcM_GetParam(this));
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

    tevStr.room_no = dComIfGp_roomControl_getStayNo();

    if (mpModel) {
        mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
        mDoMtx_stack_c::YrotM(shape_angle.y);
        mDoMtx_stack_c::scaleM(scale);
        mpModel->setBaseTRMtx(mDoMtx_stack_c::get());

        // 1. Calculate body joints
        SafeModelCalc(mpModel);

        // 2. Hat / Hair attached to Head joint (4)
        if (mpHatModel) {
            mpHatModel->setBaseTRMtx(mpModel->getAnmMtx(4));
            SafeModelCalc(mpHatModel);
        }

        // 3. Face attached to Head joint (4)
        if (mpFaceModel) {
            mpFaceModel->setBaseTRMtx(mpModel->getAnmMtx(4));
            SafeModelCalc(mpFaceModel);
        }

        // 4. Hands attached to Left Wrist (9) and Right Wrist (14 / 0xE)
        if (mpHandModel) {
            mpHandModel->setBaseTRMtx(mpModel->getBaseTRMtx());
            SafeModelCalc(mpHandModel);
            mpHandModel->setAnmMtx(1, mpModel->getAnmMtx(9));
            mpHandModel->setAnmMtx(2, mpModel->getAnmMtx(0xE));
        }

        fopAcM_SetMtx(this, mpModel->getBaseTRMtx());
    } else {
        mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
        mDoMtx_stack_c::YrotM(shape_angle.y);
        mDoMtx_stack_c::scaleM(scale);
        fopAcM_SetMtx(this, mDoMtx_stack_c::get());
    }

    return 1;
}

int daGhostPlayer_c::Draw() {
    if (mpModel) {
        daAlink_c* localPlayer = (daAlink_c*)dComIfGp_getPlayer(0);
        if (localPlayer) {
            tevStr = localPlayer->tevStr;
        } else {
            g_env_light.settingTevStruct(0, &current.pos, &tevStr);
        }

        tevStr.mFogStartZ = 0.0f;
        tevStr.mFogEndZ = 0.0f;

        DisableMipmapsForModel(mpModel);
        DisableMipmapsForModel(mpHandModel);
        DisableMipmapsForModel(mpHatModel);
        DisableMipmapsForModel(mpFaceModel);

        auto DrawSubModel = [&](J3DModel* subModel) {
            if (!subModel) return;
            g_env_light.setLightTevColorType_MAJI(subModel, &tevStr);
            mDoExt_modelEntryDL(subModel);
        };

        DrawSubModel(mpModel);
        DrawSubModel(mpHandModel);
        DrawSubModel(mpHatModel);
        DrawSubModel(mpFaceModel);
    }

    return 1;
}

int daGhostPlayer_c::Delete() {
    mpModel = nullptr;
    mpHatModel = nullptr;
    mpFaceModel = nullptr;
    mpHandModel = nullptr;
    m_particleEmitter = nullptr;
    fopAcM_DeleteHeap(this);
    return 1;
}

// ---------------------------------------------------------------------------
// Static Callbacks & Metadata Required by the Engine
// ---------------------------------------------------------------------------

daAlink_c* localLink = nullptr; // Defined here

s16 daGhostPlayer_c::sProcName = 0;
ActorHandle daGhostPlayer_c::sActorHandle = 0;

int daGhostPlayer_c::CreateHeapCallback(fopAc_ac_c* i_this) {
    return static_cast<daGhostPlayer_c*>(i_this)->CreateHeap();
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

int daGhostPlayer_c::IsDeleteCallback(void* i_this) {
    return 1;
}

const ActorProfileDesc daGhostPlayer_c::sProfile = {
    .name = DA_GHOST_PLAYER_NAME,
    .priority_group = 7,  // Executed alongside general world actors, after Link
    .process_size = sizeof(daGhostPlayer_c),
    .draw_priority = fpcDwPi_ALINK_e,
    .status = fopAcStts_UNK_0x40000_e | fopAcStts_NOPAUSE_e | fopAcStts_FREEZE_e,
    .create_function = daGhostPlayer_c::CreateCallback,
    .delete_function = daGhostPlayer_c::DeleteCallback,
    .execute_function = daGhostPlayer_c::ExecuteCallback,
    .is_delete_function = daGhostPlayer_c::IsDeleteCallback,
    .draw_function = daGhostPlayer_c::DrawCallback,
};