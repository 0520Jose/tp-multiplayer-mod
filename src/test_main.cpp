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
    SyncStatusPacket statusPacket;
    statusPacket.health = 12;
    statusPacket.maxHealth = 20;
    statusPacket.currentAnimation = 0x01020304;

    std::vector<uint8_t> statusData = PacketSerializer::SerializeSyncStatus(statusPacket);
    assert(statusData.size() == 8);  // 2 + 2 + 4 bytes

    SyncStatusPacket dStatus = PacketSerializer::DeserializeSyncStatus(statusData);
    assert(dStatus.health == statusPacket.health);
    assert(dStatus.maxHealth == statusPacket.maxHealth);
    assert(dStatus.currentAnimation == statusPacket.currentAnimation);

    std::cout << "[PASS] Status serialization round-trip\n";
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

    std::cout << "[PASS] PacketType enum values\n";
}

int main() {
    std::cout << "===========================================\n";
    std::cout << " TP Multiplayer Mod — Unit Tests\n";
    std::cout << "===========================================\n";

    TestPositionSerialization();
    TestStatusSerialization();
    TestProtocolFormat();
    TestRemotePlayerState();
    TestConnectionConfig();
    TestPacketTypes();

    std::cout << "===========================================\n";
    std::cout << " All tests passed!\n";
    std::cout << "===========================================\n";
    return 0;
}
