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
    // 3D mesh rendering of Link is explicitly marked as PENDIENTE.
    // Models are kept null to prevent graphics pipeline crashes while co-op systems run.
    mpModel = nullptr;
    mpHatModel = nullptr;
    mpFaceModel = nullptr;
    mpHandModel = nullptr;
    return 1;
}

int daGhostPlayer_c::create() {
    m_networkPlayerId = static_cast<uint8_t>(fopAcM_GetParam(this));
    m_particleEmitter = nullptr;
    mpModel = nullptr;
    mpHatModel = nullptr;
    mpFaceModel = nullptr;
    mpHandModel = nullptr;

    fopAcM_entrySolidHeap(this, (heapCallbackFunc)CreateHeapCallback, 0x1000);

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

    fopAcM_setCullSizeBox(this, -300.0f, -0.0f, -300.0f, 300.0f, 300.0f, 300.0f);

    return cPhs_COMPLEATE_e;
}

static void TransferModelPose(J3DModel* src, J3DModel* dst, MtxP dstBase, MtxP srcInvBase) {
    if (!src || !dst) return;
    
    J3DModelData* srcData = src->getModelData();
    J3DModelData* dstData = dst->getModelData();
    if (!srcData || !dstData) return;
    
    u16 count = srcData->getJointNum();
    if (dstData->getJointNum() < count) {
        count = dstData->getJointNum();
    }
    
    for (u16 i = 0; i < count; ++i) {
        MtxP srcMtx = src->getAnmMtx(i);
        if (srcMtx) {
            Mtx localMtx;
            MTXConcat(srcInvBase, srcMtx, localMtx);
            Mtx finalMtx;
            MTXConcat(dstBase, localMtx, finalMtx);
            dst->setAnmMtx(i, finalMtx);
        }
    }
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

    // Maintain actor position and rotation matrix for world/collision presence
    mDoMtx_stack_c::push();
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::YrotM(shape_angle.y);
    mDoMtx_stack_c::scaleM(scale);
    if (mpModel) {
        mpModel->setBaseTRMtx(mDoMtx_stack_c::get());
        fopAcM_SetMtx(this, mpModel->getBaseTRMtx());
    } else {
        fopAcM_SetMtx(this, mDoMtx_stack_c::get());
    }
    mDoMtx_stack_c::pop();

    SafeModelCalc(mpModel);
    SafeModelCalc(mpHatModel);
    SafeModelCalc(mpFaceModel);
    SafeModelCalc(mpHandModel);

    if (mpModel) {
        if (mpHatModel) mpHatModel->setBaseTRMtx(mpModel->getAnmMtx(4));
        if (mpFaceModel) mpFaceModel->setBaseTRMtx(mpModel->getAnmMtx(4));
        if (mpHandModel) {
            mpHandModel->setBaseTRMtx(mpModel->getBaseTRMtx());
        }
    }

    return 1;
}

int daGhostPlayer_c::Draw() {
    // Only perform 3D drawing passes when models are present
    if (mpModel) {
        DisableMipmapsForModel(mpModel);
        g_env_light.settingTevStruct(0, &current.pos, &tevStr);

        J3DJointCallBack savedCallbacks[128];
        ClearJointCallbacks(mpModel, savedCallbacks);

        g_env_light.setLightTevColorType_MAJI(mpModel, &tevStr);
        mDoExt_modelEntryDL(mpModel);

        RestoreJointCallbacks(mpModel, savedCallbacks);
    }

    if (mpHandModel) {
        DisableMipmapsForModel(mpHandModel);
        g_env_light.setLightTevColorType_MAJI(mpHandModel, &tevStr);
        mpHandModel->calcMaterial();
        mpHandModel->diff();
        mDoExt_modelEntryDL(mpHandModel);
    }

    if (mpHatModel) {
        DisableMipmapsForModel(mpHatModel);
        g_env_light.setLightTevColorType_MAJI(mpHatModel, &tevStr);
        mpHatModel->calcMaterial();
        mpHatModel->diff();
        mDoExt_modelEntryDL(mpHatModel);
    }

    if (mpFaceModel) {
        DisableMipmapsForModel(mpFaceModel);
        g_env_light.setLightTevColorType_MAJI(mpFaceModel, &tevStr);
        mpFaceModel->calcMaterial();
        mpFaceModel->diff();
        mDoExt_modelEntryDL(mpFaceModel);
    }

    return 1;
}

int daGhostPlayer_c::Delete() {
    mpModel = nullptr;
    mpHatModel = nullptr;
    mpFaceModel = nullptr;
    mpHandModel = nullptr;
    m_particleEmitter = nullptr;
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