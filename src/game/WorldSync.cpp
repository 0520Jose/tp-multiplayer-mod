#include "WorldSync.h"
#include <d/d_com_inf_game.h>
#include <d/actor/d_a_alink.h>
#include <cstring>

WorldSync::WorldSync() {
    Reset();
}

WorldSync::~WorldSync() {
}

void WorldSync::Reset() {
    m_initialized = false;
    std::memset(m_currentStage, 0, sizeof(m_currentStage));
    std::memset(m_eventBitsSnapshot, 0, sizeof(m_eventBitsSnapshot));
    m_tboxSnapshot[0] = m_tboxSnapshot[1] = 0;
    m_switchSnapshot[0] = m_switchSnapshot[1] = m_switchSnapshot[2] = m_switchSnapshot[3] = 0;
    m_keyNumSnapshot = 0;
    m_dungeonItemSnapshot = 0;
}

void WorldSync::CaptureCurrentSnapshots(const char* stage) {
    if (stage) {
        std::strncpy(m_currentStage, stage, 8);
    } else {
        std::memset(m_currentStage, 0, sizeof(m_currentStage));
    }

    // Capture global story event bits
    uint8_t* pEventBits = static_cast<uint8_t*>(dComIfGs_getPEventBit());
    if (pEventBits) {
        std::memcpy(m_eventBitsSnapshot, pEventBits, 256);
    }

    // Capture stage memory bits
    dSv_memBit_c& memBit = g_dComIfG_gameInfo.info.getMemory().getBit();
    m_tboxSnapshot[0] = static_cast<uint32_t>(memBit.mTbox[0]);
    m_tboxSnapshot[1] = static_cast<uint32_t>(memBit.mTbox[1]);

    m_switchSnapshot[0] = static_cast<uint32_t>(memBit.mSwitch[0]);
    m_switchSnapshot[1] = static_cast<uint32_t>(memBit.mSwitch[1]);
    m_switchSnapshot[2] = static_cast<uint32_t>(memBit.mSwitch[2]);
    m_switchSnapshot[3] = static_cast<uint32_t>(memBit.mSwitch[3]);

    m_keyNumSnapshot = dComIfGs_getKeyNum();
    m_dungeonItemSnapshot = memBit.mDungeonItem;

    m_initialized = true;
}

void WorldSync::PollLocalEvents(std::vector<SyncWorldEventPacket>& outPackets) {
    daAlink_c* localPlayer = (daAlink_c*)dComIfGp_getPlayer(0);
    if (!localPlayer) {
        m_initialized = false;
        return;
    }

    const char* currentStage = dComIfGp_getStartStageName();
    if (!currentStage) return;

    // Detect stage transition: re-capture stage-specific snapshots without generating false deltas
    if (!m_initialized || std::strncmp(m_currentStage, currentStage, 8) != 0) {
        CaptureCurrentSnapshots(currentStage);
        return;
    }

    // 1. Differential scan of 2048 global Event Bits (Story, flags, cutscenes)
    uint8_t* pEventBits = static_cast<uint8_t*>(dComIfGs_getPEventBit());
    if (pEventBits) {
        for (int byteIdx = 0; byteIdx < 256; ++byteIdx) {
            uint8_t currentByte = pEventBits[byteIdx];
            uint8_t snapshotByte = m_eventBitsSnapshot[byteIdx];

            // Bits that changed from 0 to 1
            uint8_t newBits = currentByte & ~snapshotByte;
            if (newBits != 0) {
                for (int bit = 0; bit < 8; ++bit) {
                    if ((newBits >> bit) & 1) {
                        uint16_t flagId = static_cast<uint16_t>(byteIdx * 8 + bit);
                        SyncWorldEventPacket pkt = {};
                        pkt.eventType = WORLD_EVENT_BIT_ON;
                        pkt.eventId = flagId;
                        pkt.param = 0;
                        std::memcpy(pkt.stageName, currentStage, 8);
                        outPackets.push_back(pkt);
                    }
                }
                m_eventBitsSnapshot[byteIdx] = currentByte;
            }
        }
    }

    // 2. Differential scan of stage chests (64 Tbox bits)
    dSv_memBit_c& memBit = g_dComIfG_gameInfo.info.getMemory().getBit();
    for (int wordIdx = 0; wordIdx < 2; ++wordIdx) {
        uint32_t currentWord = static_cast<uint32_t>(memBit.mTbox[wordIdx]);
        uint32_t snapshotWord = m_tboxSnapshot[wordIdx];
        uint32_t newChests = currentWord & ~snapshotWord;

        if (newChests != 0) {
            for (int bit = 0; bit < 32; ++bit) {
                if ((newChests >> bit) & 1) {
                    uint16_t chestId = static_cast<uint16_t>(wordIdx * 32 + bit);
                    SyncWorldEventPacket pkt = {};
                    pkt.eventType = WORLD_EVENT_TBOX_ON;
                    pkt.eventId = chestId;
                    pkt.param = 0;
                    std::memcpy(pkt.stageName, currentStage, 8);
                    outPackets.push_back(pkt);
                }
            }
            m_tboxSnapshot[wordIdx] = currentWord;
        }
    }

    // 3. Differential scan of permanent stage switches (128 bits)
    for (int wordIdx = 0; wordIdx < 4; ++wordIdx) {
        uint32_t currentWord = static_cast<uint32_t>(memBit.mSwitch[wordIdx]);
        uint32_t snapshotWord = m_switchSnapshot[wordIdx];
        uint32_t newSwitches = currentWord & ~snapshotWord;

        if (newSwitches != 0) {
            for (int bit = 0; bit < 32; ++bit) {
                if ((newSwitches >> bit) & 1) {
                    uint16_t swId = static_cast<uint16_t>(wordIdx * 32 + bit);
                    SyncWorldEventPacket pkt = {};
                    pkt.eventType = WORLD_EVENT_SWITCH_ON;
                    pkt.eventId = swId;
                    pkt.param = 0;
                    std::memcpy(pkt.stageName, currentStage, 8);
                    outPackets.push_back(pkt);
                }
            }
            m_switchSnapshot[wordIdx] = currentWord;
        }
    }

    // 4. Dungeon small keys count
    uint8_t currentKeys = dComIfGs_getKeyNum();
    if (currentKeys != m_keyNumSnapshot) {
        SyncWorldEventPacket pkt = {};
        pkt.eventType = WORLD_EVENT_KEY_SYNC;
        pkt.eventId = 0;
        pkt.param = currentKeys;
        std::memcpy(pkt.stageName, currentStage, 8);
        outPackets.push_back(pkt);

        m_keyNumSnapshot = currentKeys;
    }

    // 5. Dungeon items (Map, Compass, Boss Key)
    uint8_t currentDungeonItem = memBit.mDungeonItem;
    uint8_t newDungeonItems = currentDungeonItem & ~m_dungeonItemSnapshot;
    if (newDungeonItems != 0) {
        for (int bit = 0; bit < 8; ++bit) {
            if ((newDungeonItems >> bit) & 1) {
                SyncWorldEventPacket pkt = {};
                pkt.eventType = WORLD_EVENT_DUNGEON_ITEM;
                pkt.eventId = static_cast<uint16_t>(bit);
                pkt.param = 0;
                std::memcpy(pkt.stageName, currentStage, 8);
                outPackets.push_back(pkt);
            }
        }
        m_dungeonItemSnapshot = currentDungeonItem;
    }
}

void WorldSync::ApplyRemoteEvent(const SyncWorldEventPacket& packet) {
    const char* currentStage = dComIfGp_getStartStageName();

    switch (packet.eventType) {
        case WORLD_EVENT_BIT_ON: {
            if (!dComIfGs_isEventBit(packet.eventId)) {
                dComIfGs_onEventBit(packet.eventId);
                // Update local snapshot so PollLocalEvents does not reflect it back
                int byteIdx = packet.eventId / 8;
                int bit = packet.eventId % 8;
                if (byteIdx < 256) {
                    m_eventBitsSnapshot[byteIdx] |= (1 << bit);
                }
            }
            break;
        }

        case WORLD_EVENT_TBOX_ON: {
            if (currentStage && std::strncmp(currentStage, packet.stageName, 8) == 0) {
                if (!dComIfGs_isTbox(packet.eventId)) {
                    dComIfGs_onTbox(packet.eventId);
                    int wordIdx = packet.eventId / 32;
                    int bit = packet.eventId % 32;
                    if (wordIdx < 2) {
                        m_tboxSnapshot[wordIdx] |= (1u << bit);
                    }
                }
            }
            break;
        }

        case WORLD_EVENT_SWITCH_ON: {
            if (currentStage && std::strncmp(currentStage, packet.stageName, 8) == 0) {
                if (!dComIfGs_isSaveSwitch(packet.eventId)) {
                    dComIfGs_onSaveSwitch(packet.eventId);
                    int wordIdx = packet.eventId / 32;
                    int bit = packet.eventId % 32;
                    if (wordIdx < 4) {
                        m_switchSnapshot[wordIdx] |= (1u << bit);
                    }
                }
            }
            break;
        }

        case WORLD_EVENT_KEY_SYNC: {
            if (currentStage && std::strncmp(currentStage, packet.stageName, 8) == 0) {
                dComIfGs_setKeyNum(packet.param);
                m_keyNumSnapshot = packet.param;
            }
            break;
        }

        case WORLD_EVENT_DUNGEON_ITEM: {
            if (currentStage && std::strncmp(currentStage, packet.stageName, 8) == 0) {
                g_dComIfG_gameInfo.info.getMemory().getBit().onDungeonItem(packet.eventId);
                m_dungeonItemSnapshot |= (1 << packet.eventId);
            }
            break;
        }
    }
}
