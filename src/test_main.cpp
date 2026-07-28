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
    // These tests just verify the API surface exists, 
    // real logic would test parsing if we had a config file.
    assert(ConnectionConfig::GetConfiguredHost("127.0.0.1") == "127.0.0.1");
    assert(ConnectionConfig::GetConfiguredPort(1234) == 1234);
}

int main() {
    SyncPositionPacket packet;
    packet.x = 100.5f;
    packet.y = 200.5f;
    packet.z = 300.5f;
    packet.rotY = 45.0f;

    std::cout << "Posicion local inicial: X=0 Y=0 Z=0 RotY=0\n";
    std::cout << "Posicion modificada : X=" << packet.x 
              << " Y=" << packet.y 
              << " Z=" << packet.z 
              << " RotY=" << packet.rotY << "\n";

    std::vector<uint8_t> data = PacketSerializer::SerializeSyncPosition(packet);
    std::cout << "Tamanio de serializacion: " << data.size() << " bytes\n";

    SyncPositionPacket dPacket = PacketSerializer::DeserializeSyncPosition(data);
    std::cout << "Posicion deserializada: X=" << dPacket.x 
              << " Y=" << dPacket.y 
              << " Z=" << dPacket.z 
              << " RotY=" << dPacket.rotY << "\n";
              
    assert(dPacket.x == packet.x);
    assert(dPacket.y == packet.y);
    assert(dPacket.z == packet.z);
    assert(dPacket.rotY == packet.rotY);

    SyncStatusPacket statusPacket;
    statusPacket.health = 12;
    statusPacket.maxHealth = 20;
    statusPacket.currentAnimation = 0x01020304;

    std::cout << "Estado modificado: HP=" << statusPacket.health 
              << "/" << statusPacket.maxHealth 
              << " Anim=" << statusPacket.currentAnimation << "\n";

    std::vector<uint8_t> statusData = PacketSerializer::SerializeSyncStatus(statusPacket);
    std::cout << "Tamanio de serializacion estado: " << statusData.size() << " bytes\n";

    SyncStatusPacket dStatus = PacketSerializer::DeserializeSyncStatus(statusData);
    std::cout << "Estado deserializado: HP=" << dStatus.health 
              << "/" << dStatus.maxHealth 
              << " Anim=" << dStatus.currentAnimation << "\n";

    assert(dStatus.health == statusPacket.health);
    assert(dStatus.maxHealth == statusPacket.maxHealth);
    assert(dStatus.currentAnimation == statusPacket.currentAnimation);
    
    TestConnectionConfig();

    std::cout << "Pruebas de serializacion pasaron exitosamente.\n";
    return 0;
}
