#include "GhostPlayer.h"
#include "ActorInterface.h"
#include <d/d_com_inf_game.h>
#include <d/actor/d_a_alink.h>
#include <f_op/f_op_actor_mng.h>
#include <m_Do/m_Do_mtx.h>
#include <m_Do/m_Do_ext.h>
#include <JSystem/J3DGraphAnimator/J3DModel.h>
#include <JSystem/J3DGraphAnimator/J3DJointTree.h>
#include <JSystem/J3DGraphAnimator/J3DJoint.h>
#include <JSystem/J3DGraphAnimator/J3DMtxBuffer.h>

extern PlayerSync* g_playerSync;

s16 daGhostPlayer_c::sProcName = -1;
ActorHandle daGhostPlayer_c::sActorHandle = 0;

static void TransferModelPose(J3DModel* src, J3DModel* dst, const Mtx& worldMtx, const Mtx& srcInvBase) {
    if (!src || !dst) return;
    J3DModelData* srcData = src->getModelData();
    J3DModelData* dstData = dst->getModelData();
    if (!srcData || !dstData) return;

    u16 numJoints = (srcData->getJointNum() < dstData->getJointNum()) 
                    ? srcData->getJointNum() 
                    : dstData->getJointNum();

    for (u16 i = 0; i < numJoints; ++i) {
        MtxP srcMtx = src->getAnmMtx(i);
        if (!srcMtx) continue;

        Mtx localJoint, finalJoint;
        MTXConcat(srcInvBase, srcMtx, localJoint);
        MTXConcat(worldMtx, localJoint, finalJoint);

        dst->setAnmMtx(i, finalJoint);
    }
}

daGhostPlayer_c::daGhostPlayer_c() {
    m_networkPlayerId = 0xFF;
    m_particleEmitter = nullptr;
    mpModel = nullptr;
    mpHatModel = nullptr;
    mpFaceModel = nullptr;
    mpHandModel = nullptr;
}

daGhostPlayer_c::~daGhostPlayer_c() {
    if (m_particleEmitter) {
        m_particleEmitter->stopDrawParticle();
        m_particleEmitter = nullptr;
    }
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

    mpModel = mDoExt_J3DModel__create(bodyData, 0x80000, 0x11000084);
    if (!mpModel) {
        return 0;
    }
    mpModel->setUserArea(0);

    if (localLink->mpLinkHatModel && localLink->mpLinkHatModel->getModelData()) {
        mpHatModel = mDoExt_J3DModel__create(localLink->mpLinkHatModel->getModelData(), 0x80000, 0x11000084);
        if (mpHatModel) {
            mpHatModel->setUserArea(0);
        }
    }

    if (localLink->mpLinkFaceModel && localLink->mpLinkFaceModel->getModelData()) {
        mpFaceModel = mDoExt_J3DModel__create(localLink->mpLinkFaceModel->getModelData(), 0x80000, 0x11000084);
        if (mpFaceModel) {
            mpFaceModel->setUserArea(0);
        }
    }

    if (localLink->mpLinkHandModel && localLink->mpLinkHandModel->getModelData()) {
        mpHandModel = mDoExt_J3DModel__create(localLink->mpLinkHandModel->getModelData(), 0x80000, 0x11000084);
        if (mpHandModel) {
            mpHandModel->setUserArea(0);
        }
    }

    return 1;
}

int daGhostPlayer_c::CreateHeapCallback(fopAc_ac_c* i_this) {
    return static_cast<daGhostPlayer_c*>(i_this)->CreateHeap();
}

int daGhostPlayer_c::create() {
    fopAcM_ct(this, daGhostPlayer_c);

    m_networkPlayerId = static_cast<uint8_t>(fopAcM_GetParam(this));
    scale.x = 1.0f;
    scale.y = 1.0f;
    scale.z = 1.0f;

    dKy_tevstr_init(&tevStr, dComIfGp_roomControl_getStayNo(), 0xFF);

    if (!fopAcM_entrySolidHeap(this, CreateHeapCallback, 0x60000)) {
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

    daAlink_c* localLink = (daAlink_c*)dComIfGp_getPlayer(0);
    if (localLink) {
        fopAcM_SetRoomNo(this, fopAcM_GetRoomNo(localLink));
        tevStr.room_no = fopAcM_GetRoomNo(localLink);
    }

    if (mpModel) {
        mDoMtx_stack_c::push();
        mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
        mDoMtx_stack_c::YrotM(shape_angle.y);
        mDoMtx_stack_c::scaleM(scale);
        mpModel->setBaseTRMtx(mDoMtx_stack_c::get());
        mDoMtx_stack_c::pop();

        if (localLink && localLink->mpLinkModel) {
            Mtx invBase;
            MTXInverse(localLink->mpLinkModel->getBaseTRMtx(), invBase);

            TransferModelPose(localLink->mpLinkModel, mpModel, mpModel->getBaseTRMtx(), invBase);

            mpModel->calcWeightEnvelopeMtx();

            MtxP headMtx = mpModel->getAnmMtx(4);
            if (!headMtx) {
                headMtx = mpModel->getBaseTRMtx();
            }

            if (mpHatModel) {
                mpHatModel->setBaseTRMtx(headMtx);
            }

            if (mpFaceModel) {
                mpFaceModel->setBaseTRMtx(headMtx);
            }

            if (mpHandModel) {
                mpHandModel->setBaseTRMtx(mpModel->getBaseTRMtx());
                MtxP leftWrist = mpModel->getAnmMtx(9);
                MtxP rightWrist = mpModel->getAnmMtx(14);
                if (leftWrist) {
                    mpHandModel->setAnmMtx(1, leftWrist);
                }
                if (rightWrist) {
                    mpHandModel->setAnmMtx(2, rightWrist);
                }
                mpHandModel->calcWeightEnvelopeMtx();
            }
        }

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

        tevStr.mFogStartZ = 0.0f;
        tevStr.mFogEndZ   = 0.0f;

        tevStr.TevColor.r = 0;
        tevStr.TevColor.g = 0;
        tevStr.TevColor.b = 0;
        tevStr.TevColor.a = 0;
        tevStr.TevKColor.r = 0;
        tevStr.TevKColor.g = 0;
        tevStr.TevKColor.b = 0;
        tevStr.TevKColor.a = 0;

        Mtx currentView;
        MTXCopy(dComIfGd_getViewMtx(), currentView);

        g_env_light.setLightTevColorType_MAJI(mpModel, &tevStr);
        mDoExt_modelEntryDL(mpModel);

        if (mpHatModel) {
            g_env_light.setLightTevColorType_MAJI(mpHatModel, &tevStr);
            mDoExt_modelEntryDL(mpHatModel);
        }

        if (mpFaceModel) {
            g_env_light.setLightTevColorType_MAJI(mpFaceModel, &tevStr);
            mDoExt_modelEntryDL(mpFaceModel);
        }

        if (mpHandModel) {
            g_env_light.setLightTevColorType_MAJI(mpHandModel, &tevStr);
            mDoExt_modelEntryDL(mpHandModel);
        }

        MTXCopy(currentView, dComIfGd_getViewMtx());
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
    .priority_group = 7,
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