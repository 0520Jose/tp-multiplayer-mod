#include <iostream>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include "network/PacketSerializer.h"
#include "network/NetworkTypes.h"
#include "network/ConnectionConfig.h"

// =============================================================================
// Unit Tests for TP Multiplayer Mod
// =============================================================================
// Tests serialization/deserialization, the new protocol format, and config.

#ifdef _WIN32
static void SetEnvVar(const char* name, const char* value) {
    _putenv_s(name, value);
}
#else
static void SetEnvVar(const char* name, const char* value) {
    setenv(name, value, 1);
}
#endif

// --- Test: Position packet round-trip ---
static void TestPositionSerialization() {
    SyncPositionPacket packet;
    packet.x = 100.5f;
    packet.y = 200.5f;
    packet.z = 300.5f;
    packet.rotY = 45.0f;
    std::memcpy(packet.stageName, "F_SP108", 8);
    packet.roomNo = 1;

    std::vector<uint8_t> data = PacketSerializer::SerializeSyncPosition(packet);
    assert(data.size() == 25);  // 16 bytes coords + 8 bytes stage + 1 byte room

    SyncPositionPacket dPacket = PacketSerializer::DeserializeSyncPosition(data);
    assert(dPacket.x == packet.x);
    assert(dPacket.y == packet.y);
    assert(dPacket.z == packet.z);
    assert(dPacket.rotY == packet.rotY);
    assert(std::memcmp(dPacket.stageName, packet.stageName, 8) == 0);
    assert(dPacket.roomNo == packet.roomNo);

    std::cout << "[PASS] Position serialization round-trip\n";
}

// --- Test: Status packet round-trip ---
static void TestStatusSerialization() {
    SyncStatusPacket statusPacket = {};
    statusPacket.health = 12;
    statusPacket.maxHealth = 20;
    statusPacket.rupees = 250;
    statusPacket.form = 1; // Wolf form
    statusPacket.actionFlags = 1; // Horse riding
    statusPacket.currentAnimation = 0x01020304;

    std::vector<uint8_t> statusData = PacketSerializer::SerializeSyncStatus(statusPacket);
    assert(statusData.size() == 12);  // 2 + 2 + 2 + 1 + 1 + 4 bytes

    SyncStatusPacket dStatus = PacketSerializer::DeserializeSyncStatus(statusData);
    assert(dStatus.health == statusPacket.health);
    assert(dStatus.maxHealth == statusPacket.maxHealth);
    assert(dStatus.rupees == statusPacket.rupees);
    assert(dStatus.form == statusPacket.form);
    assert(dStatus.actionFlags == statusPacket.actionFlags);
    assert(dStatus.currentAnimation == statusPacket.currentAnimation);

    std::cout << "[PASS] Status serialization round-trip (health, form, rupees, actions)\n";
}

// --- Test: New protocol wire format (type + playerID + payload) ---
static void TestProtocolFormat() {
    // Simulate building a position packet as it would appear on the wire:
    // [type=0][playerID=5][...25 bytes position payload...]
    SyncPositionPacket posPacket;
    posPacket.x = 42.0f;
    posPacket.y = 100.0f;
    posPacket.z = -50.0f;
    posPacket.rotY = 16384.0f;  // 90 degrees in TP angle format
    std::memcpy(posPacket.stageName, "R_SP107", 8);
    posPacket.roomNo = 2;

    std::vector<uint8_t> posData = PacketSerializer::SerializeSyncPosition(posPacket);
    posData.insert(posData.begin(), 5);                  // playerID = 5
    posData.insert(posData.begin(), PACKET_POSITION);    // type = 0

    // Verify wire format size: 1 (type) + 1 (playerID) + 25 (payload) = 27
    assert(posData.size() == 27);
    assert(posData[0] == PACKET_POSITION);
    assert(posData[1] == 5);

    // Parse it back as the client would
    uint8_t type = posData[0];
    uint8_t playerID = posData[1];
    std::vector<uint8_t> payload(posData.begin() + 2, posData.end());

    assert(type == PACKET_POSITION);
    assert(playerID == 5);
    assert(payload.size() == 25);

    SyncPositionPacket decoded = PacketSerializer::DeserializeSyncPosition(payload);
    assert(decoded.x == 42.0f);
    assert(decoded.y == 100.0f);
    assert(decoded.z == -50.0f);
    assert(decoded.rotY == 16384.0f);
    assert(std::memcmp(decoded.stageName, posPacket.stageName, 8) == 0);
    assert(decoded.roomNo == posPacket.roomNo);

    std::cout << "[PASS] Protocol wire format (type + playerID + payload)\n";
}

// --- Test: RemotePlayerState initialization ---
static void TestRemotePlayerState() {
    RemotePlayerState state;
    assert(state.hasData == false);
    assert(state.framesIdle == 0);
    assert(state.lerpT == 0.0f);
    assert(state.health == 0);
    assert(state.maxHealth == 0);
    assert(state.actorID == 0xFFFFFFFF);

    std::cout << "[PASS] RemotePlayerState default initialization\n";
}

// --- Test: ConnectionConfig environment variable parsing ---
static void TestConnectionConfig() {
    // Default values (no env vars set)
    assert(ConnectionConfig::GetConfiguredHost("127.0.0.1") == "127.0.0.1");
    assert(ConnectionConfig::GetConfiguredPort(1234) == 1234);

    // Override with env vars
    SetEnvVar("TP_MULTIPLAYER_HOST", "192.168.1.100");
    SetEnvVar("TP_MULTIPLAYER_PORT", "5555");

    assert(ConnectionConfig::GetConfiguredHost("127.0.0.1") == "192.168.1.100");
    assert(ConnectionConfig::GetConfiguredPort(1234) == 5555);

    // Clean up env vars
    SetEnvVar("TP_MULTIPLAYER_HOST", "");
    SetEnvVar("TP_MULTIPLAYER_PORT", "");

    std::cout << "[PASS] ConnectionConfig environment variable parsing\n";
}

// --- Test: PacketType enum values ---
static void TestPacketTypes() {
    assert(PACKET_POSITION == 0);
    assert(PACKET_STATUS == 1);
    assert(PACKET_PLAYER_ASSIGN == 2);
    assert(PACKET_PLAYER_DISCONNECT == 3);
    assert(PACKET_WORLD_EVENT == 4);
    assert(PACKET_CHAT_MESSAGE == 5);

    std::cout << "[PASS] PacketType enum values\n";
}

// --- Test: World event packet round-trip ---
static void TestWorldEventSerialization() {
    SyncWorldEventPacket eventPacket = {};
    eventPacket.eventType = WORLD_EVENT_TBOX_ON;
    eventPacket.eventId = 42;
    eventPacket.param = 3;
    std::memcpy(eventPacket.stageName, "D_MN05A", 8);

    std::vector<uint8_t> eventData = PacketSerializer::SerializeSyncWorldEvent(eventPacket);
    assert(eventData.size() == 12);  // 1 + 2 + 1 + 8 bytes

    SyncWorldEventPacket dEvent = PacketSerializer::DeserializeSyncWorldEvent(eventData);
    assert(dEvent.eventType == eventPacket.eventType);
    assert(dEvent.eventId == eventPacket.eventId);
    assert(dEvent.param == eventPacket.param);
    assert(std::memcmp(dEvent.stageName, eventPacket.stageName, 8) == 0);

    // Wire format test with playerID header: [type=4][playerID=2][...12 payload bytes...]
    eventData.insert(eventData.begin(), 2);
    eventData.insert(eventData.begin(), PACKET_WORLD_EVENT);
    assert(eventData.size() == 14);
    assert(eventData[0] == PACKET_WORLD_EVENT);
    assert(eventData[1] == 2);

    std::vector<uint8_t> payload(eventData.begin() + 2, eventData.end());
    SyncWorldEventPacket decoded = PacketSerializer::DeserializeSyncWorldEvent(payload);
    assert(decoded.eventType == WORLD_EVENT_TBOX_ON);
    assert(decoded.eventId == 42);
    assert(decoded.param == 3);
    assert(std::memcmp(decoded.stageName, "D_MN05A", 8) == 0);

    std::cout << "[PASS] World event serialization & wire format round-trip\n";
}

// --- Test: Chat message packet round-trip ---
static void TestChatMessageSerialization() {
    SyncChatMessagePacket chatPacket = {};
    std::strncpy(chatPacket.message, "Hello from Ordon Village!", sizeof(chatPacket.message) - 1);

    std::vector<uint8_t> chatData = PacketSerializer::SerializeSyncChatMessage(chatPacket);
    assert(chatData.size() == 64);

    SyncChatMessagePacket dChat = PacketSerializer::DeserializeSyncChatMessage(chatData);
    assert(std::strncmp(dChat.message, "Hello from Ordon Village!", sizeof(dChat.message)) == 0);

    // Wire format test: [type=5][playerID=1][...64 bytes payload...]
    chatData.insert(chatData.begin(), 1);
    chatData.insert(chatData.begin(), PACKET_CHAT_MESSAGE);
    assert(chatData.size() == 66);
    assert(chatData[0] == PACKET_CHAT_MESSAGE);
    assert(chatData[1] == 1);

    std::vector<uint8_t> payload(chatData.begin() + 2, chatData.end());
    SyncChatMessagePacket decoded = PacketSerializer::DeserializeSyncChatMessage(payload);
    assert(std::strcmp(decoded.message, "Hello from Ordon Village!") == 0);

    std::cout << "[PASS] Chat message serialization & wire format round-trip\n";
}

int main() {
    std::cout << "===========================================\n";
    std::cout << " TP Multiplayer Mod — Unit Tests\n";
    std::cout << "===========================================\n";

    TestPositionSerialization();
    TestStatusSerialization();
    TestWorldEventSerialization();
    TestChatMessageSerialization();
    TestProtocolFormat();
    TestRemotePlayerState();
    TestConnectionConfig();
    TestPacketTypes();

    std::cout << "===========================================\n";
    std::cout << " All tests passed!\n";
    std::cout << "===========================================\n";
    return 0;
}


