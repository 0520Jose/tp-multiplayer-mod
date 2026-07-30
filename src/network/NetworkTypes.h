#pragma once
#include <cstdint>

// =============================================================================
// Network Protocol — Packet Types
// =============================================================================
// All packets on the wire follow this format:
//   [PacketType : 1 byte] [PlayerID : 1 byte] [Payload : N bytes]
//
// The server assigns PlayerIDs when clients connect (via PACKET_PLAYER_ASSIGN).
// The server overwrites the playerID field in relayed packets to prevent
// spoofing — clients cannot impersonate another player.
// =============================================================================

enum PacketType : uint8_t {
    PACKET_POSITION          = 0,  // Position sync: x, y, z, rotY (16 bytes payload)
    PACKET_STATUS            = 1,  // Status sync: health, maxHealth, animID (8 bytes payload)
    PACKET_PLAYER_ASSIGN     = 2,  // Server -> Client: your assigned playerID (0 bytes payload)
    PACKET_PLAYER_DISCONNECT = 3,  // Server -> Client: a player left (0 bytes payload, playerID = who left)
};

// --- Position Synchronization Payload ---
// Wire format: 16 bytes [x:f32][y:f32][z:f32][rotY:f32], network byte order.
struct SyncPositionPacket {
    float x;
    float y;
    float z;
    float rotY;
    char stageName[8];
    uint8_t roomNo;
};

// --- Status Synchronization Payload ---
// Wire format: 8 bytes [health:i16][maxHealth:i16][animID:u32], network byte order.
struct SyncStatusPacket {
    int16_t health;
    int16_t maxHealth;
    uint32_t currentAnimation;
};

// =============================================================================
// Remote Player State (Client-side, used for interpolation)
// =============================================================================
// Holds the interpolated state of a single remote player.
// The client maintains one of these per connected remote player.
//
// Interpolation model:
//   On receiving a new position packet:
//     prevXYZ = current renderXYZ (snapshot)
//     targetXYZ = packet data
//     lerpT = 0.0
//   Each frame:
//     lerpT += LERP_SPEED
//     renderXYZ = lerp(prevXYZ, targetXYZ, lerpT)
// =============================================================================

struct RemotePlayerState {
    // --- Interpolated position (what the renderer uses) ---
    float renderX, renderY, renderZ, renderRotY;

    // --- Network target (latest from the network) ---
    float targetX, targetY, targetZ, targetRotY;

    // --- Previous position (start of current interpolation segment) ---
    float prevX, prevY, prevZ, prevRotY;

    // --- Player status ---
    int16_t health;
    int16_t maxHealth;
    uint32_t animationId;
    char stageName[8];
    uint8_t roomNo;

    // --- Interpolation state ---
    float lerpT;        // 0.0 -> 1.0, progress between prev and target
    bool hasData;       // true after first position packet received
    int framesIdle;     // frames since last network update (for timeout/cleanup)
    
    // --- Actor ID for visual representation ---
    uint32_t actorID;

    RemotePlayerState()
        : renderX(0), renderY(0), renderZ(0), renderRotY(0),
          targetX(0), targetY(0), targetZ(0), targetRotY(0),
          prevX(0), prevY(0), prevZ(0), prevRotY(0),
          health(0), maxHealth(0), animationId(0), roomNo(0),
          lerpT(0), hasData(false), framesIdle(0), actorID(0xFFFFFFFF) {
              for (int i=0; i<8; i++) stageName[i] = 0;
          }
};
