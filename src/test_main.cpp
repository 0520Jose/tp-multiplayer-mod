#include <iostream>
#include <cassert>
#include "network/PacketSerializer.h"
#include "game/ActorInterface.h"

int main() {
    PlayerSync sync;
    
    SyncPositionPacket packet = sync.GetLocalPosition();
    std::cout << "Posicion local inicial: X=" << packet.x << " Y=" << packet.y << " Z=" << packet.z << " RotY=" << packet.rotY << "\n";
    
    sync.ApplyRemotePosition(100.5f, 200.5f, 300.5f, 45.0f);
    packet = sync.GetLocalPosition();
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
    sync.ApplyRemoteStatus(12, 20, 0x01020304);
    SyncStatusPacket statusPacket = sync.GetLocalStatus();
    std::cout << "Estado modificado: HP=" << statusPacket.health << "/" << statusPacket.maxHealth << " Anim=" << statusPacket.currentAnimation << "\n";
    
    std::vector<uint8_t> statusData = PacketSerializer::SerializeSyncStatus(statusPacket);
    std::cout << "Tamanio de serializacion estado: " << statusData.size() << " bytes\n";
    
    SyncStatusPacket dStatus = PacketSerializer::DeserializeSyncStatus(statusData);
    std::cout << "Estado deserializado: HP=" << dStatus.health << "/" << dStatus.maxHealth << " Anim=" << dStatus.currentAnimation << "\n";
    
    assert(dStatus.health == statusPacket.health);
    assert(dStatus.maxHealth == statusPacket.maxHealth);
    assert(dStatus.currentAnimation == statusPacket.currentAnimation);
    
    std::cout << "Pruebas de serializacion e interfaz del actor pasaron exitosamente.\n";
    return 0;
}
