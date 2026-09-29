# Twilight Princess Multiplayer Mod — Architecture & Technical Design

This document details the architectural design of the Twilight Princess Multiplayer Mod for the **Dusklight** PC port. It covers the technical obstacles inherent to the original GameCube/Wii engine design, why naive multiplayer implementations fail, and the architectural solutions adopted by this project.

---

## 1. Background & The Twilight Princess Engine Architecture

*The Legend of Zelda: Twilight Princess* was developed in C++ by Nintendo EAD for the GameCube and Wii. Like many single-player console titles of its era, its runtime was engineered under the strict assumption of having **exactly one player** present in the game world at any time.

### 1.1 The Player Singleton Architecture

At the heart of the engine lies the global game information structure:
```cpp
// SSystem / d_com_inf_game.h
dComIfG_inf_c g_dComIfG_gameInfo;
```

Access to the player character (`daAlink_c`, Link) is exposed globally through inline accessors:
```cpp
inline daAlink_c* dComIfGp_getPlayer(int i) {
    return (daAlink_c*)g_dComIfG_gameInfo.play.mPlayer[i];
}

inline void dComIfGp_setPlayer(int i, daAlink_c* player) {
    g_dComIfG_gameInfo.play.mPlayer[i] = (fopAc_ac_c*)player;
}
```
Although `mPlayer` is technically an array of size 2 (a remnant of *The Wind Waker* / early prototypes), the entire Twilight Princess codebase hardcodes index `0` across hundreds of files:
- **Camera Controller (`dCam_c`)**: Hardcoded to lock onto and track `dComIfGp_getPlayer(0)`.
- **Enemy AI & Targeting (`fopEn_en_c`)**: Pathfinding, line-of-sight, and aggression routines exclusively check distance and line-of-sight against `dComIfGp_getPlayer(0)`.
- **Room Triggers & Zone Exits (`dStage_c`)**: Transition zones test intersection against `dComIfGp_getPlayer(0)`.
- **Input System (`mDoCPd_c`)**: `daAlink_c::proc` directly reads controller port 0:
  ```cpp
  mDoCPd_c::m_cpadInfo[0]
  ```

---

## 2. The Singleton Conflict: Why Naive Approaches Fail

A common first instinct when developing a multiplayer mod is to spawn a second instance of the native player actor (`daAlink_c`) for each remote player:
```cpp
// NAIVE APPROACH - DO NOT DO THIS
fopAcM_create(fpcNm_ALINK_e, 0, &pos, roomNo, &rot, ...);
```

This causes severe, cascading engine failures:

### 2.1 The Constructor / Destructor Hijack
In `daAlink_c::create()`:
```cpp
int daAlink_c::create() {
    dComIfGp_setPlayer(0, this); // Overwrites the local player pointer!
    ...
}
```
When a remote Link actor is spawned, it forcefully overwrites `g_dComIfG_gameInfo.play.mPlayer[0]`. Consequently:
1. The game camera immediately detaches from the local player and attaches to the remote player.
2. Enemy AI begins ignoring the local player and targets the remote ghost.

Even worse, in `daAlink_c::Delete()`:
```cpp
int daAlink_c::Delete() {
    dComIfGp_setPlayer(0, NULL); // Sets player pointer to nullptr!
    ...
}
```
When the remote player disconnects or transitions to another room and its actor is deleted, the destructor clears `mPlayer[0]` to `nullptr`. On the very next frame, the camera update or local input update attempts to dereference `mPlayer[0]`, resulting in an instant `ACCESS_VIOLATION` crash.

### 2.2 Controller Contention
`daAlink_c::execute()` runs input polling against `mDoCPd_c::m_cpadInfo[0]`. If two `daAlink_c` instances exist:
- Both execute state machines that poll the local controller port 0.
- The remote player's state is overwritten by the local player's analog stick and button presses, resulting in both actors performing identical actions locally.

### 2.3 Pointer Swap Hacks & Fragility
Previous attempts tried to intercept execution hooks (`OnLinkExecutePre` / `OnLinkExecutePost`) to swap pointers back and forth:
```cpp
// FRAGILE HACK
void OnLinkExecutePre(daAlink_c* actor) {
    if (actor == remoteLink) {
        dComIfGp_setPlayer(0, remoteLink);
    }
}
void OnLinkExecutePost(daAlink_c* actor) {
    dComIfGp_setPlayer(0, localPlayer);
}
```
This approach is fundamentally unstable:
- Asynchronous events, sound triggers, and camera calculations occurring between hooks frequently catch `mPlayer[0]` pointing to the wrong actor.
- Camera matrices stutter violently between positions.
- In multi-threaded emulators or modern ports like Dusklight, thread race conditions make pointer flipping hazardous.

---

## 3. The Puppet Actor Solution (`daGhostPlayer_c`)

To cleanly decouple remote players from the game's singleton constraints, we implement the **Puppet Actor pattern**.

```
+-------------------------------------------------------------------------+
|                        Twilight Princess / Dusklight                    |
|                                                                         |
|  +---------------------------+             +--------------------------+ |
|  |       daAlink_c           |             |     daGhostPlayer_c      | |
|  |     (Local Player)        |             |   (Remote Puppet Actor)  | |
|  +---------------------------+             +--------------------------+ |
|  | - player[0] pointer       |             | - Independent fopAc_ac_c | |
|  | - Reads Pad[0] controller |             | - NO player[0] access    | |
|  | - Drives Game Camera      |             | - NO controller polling  | |
|  | - Drives Game Progression |             | - Smooth LERP transform  | |
|  +-------------+-------------+             +------------^-------------+ |
+----------------|----------------------------------------|---------------+
                 |                                        |
                 v                                        | (Updates)
    +---------------------------+            +------------+-------------+
    |   Position & Status       |            |    Interpolation Engine  |
    |       Packets             |            |    (Hermite / Linear)    |
    +-------------+-------------+            +------------^-------------+
                  |                                       |
                  v                                       |
    +-----------------------------------------------------+-------------+
    |                    Network Client (ENet)                          |
    +-------------------------------------------------------------------+
```

### 3.1 Architecture of `daGhostPlayer_c`

Instead of creating a `daAlink_c`, remote players are instantiated as `daGhostPlayer_c`, an actor inheriting directly from `fopAc_ac_c`:

1. **Independent Actor Registration**:
   Registered dynamically with Dusklight's `ActorService`:
   ```cpp
   ActorService::get().register_actor(
       GHOST_PLAYER_ACTOR_TYPE, // Custom actor type (e.g. 0x02FF)
       fopAc_ACTOR_e,           // Actor group (standard game actor)
       7                        // Execute priority
   );
   ```

2. **Zero Interaction with `player[0]`**:
   `daGhostPlayer_c` neither reads nor sets `dComIfGp_getPlayer(0)`. The local player remains the undisputed owner of `player[0]`, ensuring:
   - The game camera never glitches or hijacks.
   - Scene transitions, door triggers, and story flags function as designed.
   - Deletion of remote players has zero effect on the local player pointer.

3. **Autonomous Lifecycle**:
   - `Create()`: Initializes transformation matrices, sets actor parameters (`playerID`), and registers the actor in the scene.
   - `Execute()`: Fetches interpolated position and rotation from the network state table and updates `current.pos` and `shape_angle`.
   - `Draw()`: Renders the visual representation (3D model, debug indicators, nametags) via the GameCube J3D display list pipeline.
   - `Delete()`: Cleans up model resources.

---

## 4. Networking Architecture

### 4.1 Topology

The mod employs a **Client-Server Relay Topology** built on top of **ENet** (UDP with optional reliability):

```
                     +---------------------------+
                     |      Dedicated Relay      |
                     |  (server_simulator.exe)   |
                     +-------------+-------------+
                                   |
          +------------------------+------------------------+
          |                                                 |
          v                                                 v
+--------------------+                            +--------------------+
|  Client A (Host)   |                            |     Client B       |
|  Player ID: 0      |                            |  Player ID: 1      |
+--------------------+                            +--------------------+
```

### 4.2 Packet Protocol

All packets sent over the wire adhere to a compact binary structure with big-endian (network byte order) integers:

```
[ 1 Byte: PacketType ] [ 1 Byte: PlayerID ] [ N Bytes: Payload ]
```

#### Packet Types:

| PacketType | Value | Payload Size | Contents | Delivery |
|---|---|---|---|---|
| `POSITION` | `0` | 25 bytes | `pos.x` (f32), `pos.y` (f32), `pos.z` (f32), `rotY` (s16), `stageName` (8 chars), `roomNo` (u8) | Unreliable sequenced (Channel 0) |
| `STATUS` | `1` | 8 bytes | `health` (s16), `maxHealth` (s16), `animationId` (s32) | Reliable (Channel 1) |
| `PLAYER_ASSIGN` | `2` | 0 bytes | Server-to-Client assignment of authoritative PlayerID | Reliable (Channel 1) |
| `PLAYER_DISCONNECT` | `3` | 0 bytes | Notification that a player has left the session | Reliable (Channel 1) |

### 4.3 Position Interpolation (LERP)

Because network packets arrive at discrete intervals (~20-60 Hz) while Dusklight renders at 60 Hz or higher, directly teleporting actors would cause severe visual jitter.

The interpolation pipeline:
1. **Timestamped State Buffer**: Each client maintains a small history buffer of received position snapshots for each remote player.
2. **Linear / Spherical Interpolation**:
   ```cpp
   currentPos = prevPos + (targetPos - prevPos) * t;
   currentAngleY = LerpAngle(prevAngleY, targetAngleY, t);
   ```
3. **Stage & Room Filtering**:
   If a remote player is located on a different stage (e.g. `F_SP108` vs `R_SP107`) or a non-adjacent room, their puppet actor is hidden or despawned locally, saving memory and eliminating cross-scene artifacts.

---

## 5. Dusklight Mod Integration

Dusklight exposes a native C++ Modding SDK (`dusklight/sdk`):

- **Mod Package (`.dusk`)**: A zip-compatible archive containing:
  - `mod.json`: Manifest metadata (name, author, version, entry points).
  - `lib/windows-amd64/mod.dll`: The compiled 64-bit DLL.
- **Entry Points**:
  - `MOD_EXPORT extern constinit const ModMeta mod_meta`: Defines the mod identifier and features (`DUSK_MOD_FEATURE_GAME`).
  - `DUSK_EXPORT int mod_init()`: Invoked upon game startup.
  - `DUSK_EXPORT void mod_update()`: Invoked once per game frame tick.
  - `DUSK_EXPORT void mod_shutdown()`: Invoked when the game closes or unloads mods.
