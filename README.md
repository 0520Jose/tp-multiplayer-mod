# Twilight Princess Multiplayer Mod (Dusklight)

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](#building)
[![Target](https://img.shields.io/badge/target-Dusklight%20PC-blue)](#requirements)
[![C++](https://img.shields.io/badge/standard-C%2B%2B20-darkblue)](#requirements)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

An online multiplayer mod for *The Legend of Zelda: Twilight Princess* running on the **Dusklight** native PC port. It allows multiple players to explore the world together in real-time through decoupled puppet actors with interpolated physics, state replication, and low-latency UDP networking.

---

## Key Features

- **Decoupled Puppet Actor System (`daGhostPlayer_c`)**: Eliminates the engine's single-player singleton crash and camera hijacking by rendering remote players as independent game actors (`fopAc_ac_c`).
- **Smooth Interpolation**: Client-side linear and angle interpolation (LERP) prevents visual jitter across network updates.
- **Low-Latency Networking**: Powered by [ENet](http://enet.bespin.org/), featuring non-blocking UDP packets, delta synchronization, and automatic reconnection.
- **Scene & Room Isolation**: Players in different stages or distant rooms are automatically filtered to preserve performance and prevent cross-map glitches.
- **Built-in Ghost Simulator**: The dedicated relay server includes a Ghost Mode that reflects single-player movements to simulate a second player for hassle-free solo testing.

---

## Architecture Overview

In the original Twilight Princess engine, `daAlink_c` is tightly coupled with `g_dComIfG_gameInfo.play.mPlayer[0]`, controller input polling (`mDoCPd_c`), and camera focus (`dCam_c`). Spawning multiple instances of `daAlink_c` causes camera lockups and immediate crashes on actor deletion.

This mod solves the singleton conflict via a **Puppet Actor pattern**:

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

For complete technical details, see [ARCHITECTURE.md](ARCHITECTURE.md).

---

## Project Structure

```
tp-multiplayer-mod/
├── ARCHITECTURE.md          # In-depth engine design & singleton analysis
├── CONTRIBUTING.md          # Setup, build, testing & PR guidelines
├── CHANGELOG.md             # Version history
├── CMakeLists.txt           # Build definitions (MSVC / C++20 / Ninja)
├── build_and_deploy.bat     # One-click build and package script
├── mod.json                 # Dusklight mod manifest
├── extern/
│   └── enet/                # ENet UDP networking submodule
└── src/
    ├── main.cpp             # Mod lifecycle (init, update, shutdown, actor registration)
    ├── game/
    │   ├── GhostPlayer.h    # Puppet actor class (daGhostPlayer_c)
    │   ├── GhostPlayer.cpp  # Lifecycle, interpolation, and rendering
    │   ├── ActorInterface.h # Local Link telemetry extraction
    │   └── ActorInterface.cpp
    ├── network/
    │   ├── Client.h         # ENet client network loop
    │   ├── Client.cpp
    │   ├── NetworkTypes.h   # Wire structs and packet enums
    │   ├── PacketSerializer.h
    │   ├── PacketSerializer.cpp # Big-endian bitstream serialization
    │   └── ConnectionConfig.h   # Environment variable overrides
    ├── test_main.cpp        # Serialization & protocol unit tests
    └── server_simulator.cpp # Dedicated relay server with Ghost Mode
```

---

## Requirements

1. **Operating System**: Windows 10/11 (64-bit).
2. **Compiler & Toolchain**: Visual Studio 2022+ (MSVC 64-bit) with C++20 support.
3. **Build System**: CMake 3.25+ and Ninja.
4. **Dusklight Source**: Located in the sibling directory `../dusklight`.
5. **Git Submodules**: ENet initialized inside `extern/enet/`.

---

## Building

### Quick Build Script
Run the automated build script:
```cmd
build_and_deploy.bat
```

### Manual Build (x64 Native Tools Command Prompt)
```cmd
# 1. Update submodules
git submodule update --init --recursive

# 2. Configure with Ninja
cmake -G Ninja -B build-msvc -DCMAKE_BUILD_TYPE=RelWithDebInfo

# 3. Build mod, server, and test suite
cmake --build build-msvc
```

The compiled mod package will be located at:
`build-msvc/mods/tp_multiplayer_mod.dusk`

---

## Testing & Running

### 1. Run Unit Tests
```cmd
.\build-msvc\test_main.exe
```

### 2. Start the Dedicated Relay Server
```cmd
.\build-msvc\server_simulator.exe
```
By default, the server listens on port `1234`.

### 3. Deploy the Mod to Dusklight
Copy the generated `.dusk` package into Dusklight's mod directory:
```cmd
copy build-msvc\mods\tp_multiplayer_mod.dusk ..\dusklight\mods\
```

### 4. Custom Server Connection
The mod connects to `127.0.0.1:1234` by default. You can override this using environment variables before launching Dusklight:
```cmd
set TP_MULTIPLAYER_HOST=192.168.1.50
set TP_MULTIPLAYER_PORT=1234
```

---

## Contributing

We welcome contributions! Please review [CONTRIBUTING.md](CONTRIBUTING.md) for environment setup, code conventions, and guidelines on synchronizing animations, equipment, and items.

---

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

*All Zelda: Twilight Princess assets, engine code, and trademarks belong to Nintendo. The Dusklight PC port is the work of its respective authors.*
