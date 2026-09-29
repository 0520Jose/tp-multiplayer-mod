#include "ActorInterface.h"
#include "GhostPlayer.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <d/d_com_inf_game.h>
#undef IN
#undef OUT
#include <d/actor/d_a_alink.h>
#include <dolphin/gx/GXAurora.h>
#include <m_Do/m_Do_ext.h>
#include <f_pc/f_pc_name.h>
#include <f_op/f_op_actor_mng.h>
#include "../network/Client.h"

extern Client* g_client;
extern PlayerSync* g_playerSync;
extern ModContext* mod_ctx;
extern const ActorService* g_actorService;

PlayerSync::PlayerSync() {
    m_player = nullptr;
    m_realPlayerID = 0xFFFFFFFF;
}

PlayerSync::~PlayerSync() {
    ResetAll();
}

fopAc_ac_c* PlayerSync::GetPlayerActor() {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (!player) return nullptr;
    if (fpcM_GetProfName(player) != fpcNm_ALINK_e) return nullptr;

    m_player = player;
    m_realPlayerID = fopAcM_GetID(player);
    return player;
}

bool PlayerSync::IsOnTitleScreen() {
    const char* stage = dComIfGp_getStartStageName();
    if (!stage) return true;
    if (std::strncmp(stage, "F_SP102", 7) == 0) return true; // Title Demo (Bridge of Eldin)
    if (std::strncmp(stage, "D_MN", 4) == 0) return true;    // Menus / File selection
    return false;
}

SyncPositionPacket PlayerSync::GetLocalPosition() {
    SyncPositionPacket packet;
    packet.x = packet.y = packet.z = packet.rotY = 0.0f;
    std::memset(packet.stageName, 0, 8);
    packet.roomNo = 0;

    m_player = GetPlayerActor();
    if (m_player) {
        packet.x = m_player->current.pos.x;
        packet.y = m_player->current.pos.y;
        packet.z = m_player->current.pos.z;
        packet.rotY = m_player->current.angle.y;
        
        const char* stage = dComIfGp_getStartStageName();
        if (stage) std::strncpy(packet.stageName, stage, 8);

        // BUG 1 FIX: fopAcM_GetRoomNo is already imported and used elsewhere in this file.
        // The previous hardcoded 0 caused ghost actors to appear through room boundaries.
        packet.roomNo = static_cast<uint8_t>(fopAcM_GetRoomNo(m_player));
    }
    return packet;
}

SyncStatusPacket PlayerSync::GetLocalStatus() {
    SyncStatusPacket packet;
    packet.health = 0;
    packet.maxHealth = 0;
    packet.rupees = 0;
    packet.form = 0;
    packet.actionFlags = 0;
    packet.currentAnimation = 0;

    // Static state for motion detection — lives at function scope so both branches can access it.
    static float s_prevX     = 0.0f;
    static float s_prevZ     = 0.0f;
    static bool  s_prevValid = false;

    m_player = GetPlayerActor();
    if (m_player) {
        packet.health    = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getLife();
        packet.maxHealth = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getMaxLife();
        packet.rupees    = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getRupee();
        packet.form      = g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA().getTransformStatus();

        daAlink_c* alink = (daAlink_c*)m_player;
        if (alink && alink->checkHorseRide()) {
            packet.actionFlags |= 1; // Bit 0: Horse Riding
        }

        // Capture locomotion status via XZ speed
        if (s_prevValid) {
            float dx     = m_player->current.pos.x - s_prevX;
            float dz     = m_player->current.pos.z - s_prevZ;
            float speed2 = dx * dx + dz * dz;
            packet.currentAnimation = (speed2 > 2.25f) ? 1u : 0u;
        }
        s_prevX     = m_player->current.pos.x;
        s_prevZ     = m_player->current.pos.z;
        s_prevValid = true;
    } else {
        s_prevValid = false;
    }
    return packet;
}

float PlayerSync::Lerp(float a, float b, float t) {
    return a + t * (b - a);
}

float PlayerSync::LerpAngle(float a, float b, float t) {
    float diff = std::fmod(b - a + 32768.0f, 65536.0f);
    if (diff < 0) diff += 65536.0f;
    diff -= 32768.0f;
    return a + diff * t;
}

void PlayerSync::ApplyRemotePosition(uint8_t playerID, const SyncPositionPacket& posData) {
    auto& state = m_remotePlayers[playerID];
    
    if (!state.hasData) {
        state.renderX = state.prevX = posData.x;
        state.renderY = state.prevY = posData.y;
        state.renderZ = state.prevZ = posData.z;
        state.renderRotY = state.prevRotY = posData.rotY;
        state.hasData = true;
    } else {
        state.prevX = state.renderX;
        state.prevY = state.renderY;
        state.prevZ = state.renderZ;
        state.prevRotY = state.renderRotY;
    }
    
    state.targetX = posData.x;
    state.targetY = posData.y;
    state.targetZ = posData.z;
    state.targetRotY = posData.rotY;
    std::memcpy(state.stageName, posData.stageName, 8);
    state.roomNo = posData.roomNo;
    
    state.lerpT = 0.0f;
    state.framesIdle = 0;
}

void PlayerSync::ApplyRemoteStatus(uint8_t playerID, int16_t health, int16_t maxHealth, uint16_t rupees, uint8_t form, uint8_t actionFlags, uint32_t animationId) {
    auto& state = m_remotePlayers[playerID];
    state.health = health;
    state.maxHealth = maxHealth;
    state.rupees = rupees;
    state.form = form;
    state.actionFlags = actionFlags;
    state.animationId = animationId;
}

void PlayerSync::RemoveRemotePlayer(uint8_t playerID) {
    auto it = m_remotePlayers.find(playerID);
    if (it != m_remotePlayers.end()) {
        if (it->second.actorID != 0xFFFFFFFF) {
            fopAc_ac_c* ghost = fopAcM_SearchByID(it->second.actorID);
            // BUG 5 FIX: Verify the actor is still a ghost before deleting.
            // If the engine already freed this actor and reused the ID for a different
            // actor (e.g. during a stage transition), we must NOT delete the new actor.
            if (ghost && daGhostPlayer_c::sProcName != -1 &&
                fpcM_GetProfName(ghost) == static_cast<u16>(daGhostPlayer_c::sProcName)) {
                fopAcM_delete(ghost);
            }
            it->second.actorID = 0xFFFFFFFF; // Invalidate regardless
        }
        m_remotePlayers.erase(it);
    }
}

void PlayerSync::ResetAll() {
    for (auto& pair : m_remotePlayers) {
        if (pair.second.actorID != 0xFFFFFFFF) {
            fopAc_ac_c* ghost = fopAcM_SearchByID(pair.second.actorID);
            // BUG 5 FIX: Same type guard as RemoveRemotePlayer.
            // During stage transitions the engine clears its actor list before calling
            // mod_update(); by then these IDs may be invalid or reused.
            if (ghost && daGhostPlayer_c::sProcName != -1 &&
                fpcM_GetProfName(ghost) == static_cast<u16>(daGhostPlayer_c::sProcName)) {
                fopAcM_delete(ghost);
            }
            pair.second.actorID = 0xFFFFFFFF; // Invalidate before erase
        }
    }
    m_remotePlayers.clear();
}

void PlayerSync::UpdateAllRemotePlayers() {
    const char* currentStage = dComIfGp_getStartStageName();
    fopAc_ac_c* localPlayer = GetPlayerActor();
    uint8_t localRoom = localPlayer
        ? static_cast<uint8_t>(fopAcM_GetRoomNo(localPlayer))
        : static_cast<uint8_t>(dComIfGp_roomControl_getStayNo());

    for (auto it = m_remotePlayers.begin(); it != m_remotePlayers.end();) {
        auto& state = it->second;
        state.framesIdle++;

        if (state.framesIdle > IDLE_TIMEOUT_FRAMES) {
            if (state.actorID != 0xFFFFFFFF) {
                fopAc_ac_c* ghost = fopAcM_SearchByID(state.actorID);
                // BUG 5 FIX: Type guard before timeout-delete
                if (ghost && daGhostPlayer_c::sProcName != -1 &&
                    fpcM_GetProfName(ghost) == static_cast<u16>(daGhostPlayer_c::sProcName)) {
                    fopAcM_delete(ghost);
                }
                state.actorID = 0xFFFFFFFF;
            }
            it = m_remotePlayers.erase(it);
            continue;
        }

        if (state.hasData) {
            if (state.lerpT < 1.0f) {
                state.lerpT += LERP_SPEED;
                if (state.lerpT > 1.0f) state.lerpT = 1.0f;
            }
            state.renderX    = Lerp(state.prevX,    state.targetX,    state.lerpT);
            state.renderY    = Lerp(state.prevY,     state.targetY,    state.lerpT);
            state.renderZ    = Lerp(state.prevZ,    state.targetZ,    state.lerpT);
            state.renderRotY = LerpAngle(state.prevRotY, state.targetRotY, state.lerpT);

            // Visibility: same stage check. We isolate by stage (e.g. F_SP103 for Ordon).
            // Sub-room differences within the same stage (indoor/outdoor boundaries) should
            // not cull the actor, as Twilight Princess stages share a unified world space.
            bool sameStage = currentStage && (std::strncmp(state.stageName, currentStage, 8) == 0);
            bool visible   = sameStage;

            if (visible) {
                if (state.actorID == 0xFFFFFFFF) {
                    cXyz pos(state.renderX, state.renderY, state.renderZ);
                    csXyz rot(0, (s16)state.renderRotY, 0);
                    int roomNo = static_cast<int>(localRoom);

                    if (g_actorService && daGhostPlayer_c::sActorHandle != 0) {
                        ActorSpawnParams params = {};
                        params.parameters = (uint32_t)it->first;
                        params.argument = -1;
                        params.room_num = (int8_t)roomNo;
                        params.position = {pos.x, pos.y, pos.z};
                        params.angle = {0, (int16_t)state.renderRotY, 0};
                        params.scale = {1.0f, 1.0f, 1.0f};
                        params.create_function = nullptr;

                        ActorId outId = 0;
                        if (g_actorService->create_actor_from_name(mod_ctx, DA_GHOST_PLAYER_NAME, &params, &outId) == MOD_OK) {
                            state.actorID = outId;
                        }
                    }

                    if (state.actorID == 0xFFFFFFFF && daGhostPlayer_c::sProcName != -1) {
                        cXyz scale(1.0f, 1.0f, 1.0f);
                        state.actorID = fopAcM_create(daGhostPlayer_c::sProcName, 0xFFFF, (u32)it->first, &pos, roomNo, &rot, &scale, -1, nullptr);
                    }
                } else {
                    fopAc_ac_c* ghost = fopAcM_SearchByID(state.actorID);
                    if (!ghost) {
                        state.actorID = 0xFFFFFFFF; // Engine already freed it (stage transition)
                    }
                }
            } else {
                if (state.actorID != 0xFFFFFFFF) {
                    fopAc_ac_c* ghost = fopAcM_SearchByID(state.actorID);
                    // BUG 5 FIX: Type guard before visibility-cull delete
                    if (ghost && daGhostPlayer_c::sProcName != -1 &&
                        fpcM_GetProfName(ghost) == static_cast<u16>(daGhostPlayer_c::sProcName)) {
                        fopAcM_delete(ghost);
                    }
                    state.actorID = 0xFFFFFFFF;
                }
            }
        }
        ++it;
    }
}

