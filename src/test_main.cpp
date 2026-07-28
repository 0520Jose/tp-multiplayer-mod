#include <iostream>
#include <cassert>
#include <cstdlib>
#include "network/PacketSerializer.h"
#include "game/ActorInterface.h"
#include "network/ConnectionConfig.h"

#ifdef _WIN32
static void SetEnvVar(const char* name, const char* value) {
    _putenv_s(name, value);
}
#else
static void SetEnvVar(const char* name, const char* value) {
    setenv(name, value, 1);
}
#endif

static void TestConnectionConfig() {
    SetEnvVar("TP_MULTIPLAYER_HOST", "192.168.1.50");
    SetEnvVar("TP_MULTIPLAYER_PORT", "4321");

    assert(ConnectionConfig::GetConfiguredHost("127.0.0.1") == "192.168.1.50");
    assert(ConnectionConfig::GetConfiguredPort(1234) == 4321);

    SetEnvVar("TP_MULTIPLAYER_HOST", "");
    SetEnvVar("TP_MULTIPLAYER_PORT", "");

    assert(ConnectionConfig::GetConfiguredHost("127.0.0.1") == "127.0.0.1");
    assert(ConnectionConfig::GetConfiguredPort(1234) == 1234);
}

int main() {
    SyncPositionPacket packet;
    packet.x = 100.5f;
    packet.y = 200.5f;
    packet.z = 300.5f;
    packet.rotY = 45.0f;
    
    std::cout << "Posicion modificada : X=" << packet.x << " Y=" << packet.y << " Z=" << packet.z << " RotY=" << packet.rotY << "\n";
    
    std::vector<uint8_t> data = PacketSerializer::SerializeSyncPosition(packet);
    std::cout << "Tamanio de serializacion: " << data.size() << " bytes\n";
    
    SyncPositionPacket deserialized = PacketSerializer::DeserializeSyncPosition(data);
    std::cout << "Posicion deserializada: X=" << deserialized.x << " Y=" << deserialized.y << " Z=" << deserialized.z << " RotY=" << deserialized.rotY << "\n";
    
    assert(deserialized.x == packet.x);
    assert(deserialized.y == packet.y);
    assert(deserialized.z == packet.z);
    assert(deserialized.rotY == packet.rotY);
    
    // --- STATUS TESTS ---
    SyncStatusPacket statusPacket;
    statusPacket.health = 12;
    statusPacket.maxHealth = 20;
    statusPacket.currentAnimation = 0x01020304;

    std::cout << "Estado modificado: HP=" << statusPacket.health << "/" << statusPacket.maxHealth << " Anim=" << statusPacket.currentAnimation << "\n";
    
    std::vector<uint8_t> statusData = PacketSerializer::SerializeSyncStatus(statusPacket);
    std::cout << "Tamanio de serializacion estado: " << statusData.size() << " bytes\n";
    
    SyncStatusPacket dStatus = PacketSerializer::DeserializeSyncStatus(statusData);
    std::cout << "Estado deserializado: HP=" << dStatus.health << "/" << dStatus.maxHealth << " Anim=" << dStatus.currentAnimation << "\n";
    
    assert(dStatus.health == statusPacket.health);
    assert(dStatus.maxHealth == statusPacket.maxHealth);
    assert(dStatus.currentAnimation == statusPacket.currentAnimation);
    
    TestConnectionConfig();

    std::cout << "Pruebas de serializacion pasaron exitosamente.\n";
    return 0;
}
