# Contributing to the Twilight Princess Multiplayer Mod

Thank you for your interest in contributing to the Twilight Princess Multiplayer Mod! This guide provides all necessary instructions to set up your environment, build the project, run tests, and contribute new features.

---

## 1. Prerequisites & Toolchain

The mod targets the 64-bit Windows PC port of Twilight Princess (**Dusklight**). Because Dusklight's game import library (`windows-amd64.lib`) uses MSVC symbol mangling, **MSVC (Visual Studio 2022 or higher)** is required to compile code mods.

### Required Software:
1. **Visual Studio 2022+** (Community, Professional, or Insiders) with the **Desktop development with C++** workload installed.
2. **CMake 3.25+** (available in VS or standalone).
3. **Ninja** build system (recommended, included with VS or `choco install ninja` / `winget install Ninja-build.Ninja`).
4. **Git** with submodule support.
5. **Dusklight Source / SDK**: Located in a sibling directory (`../dusklight`).

---

## 2. Setting Up the Workspace

Clone the repository and initialize submodules (specifically `extern/enet`):

```bash
git clone https://github.com/0520Jose/tp-multiplayer-mod.git
cd tp-multiplayer-mod

# Initialize the ENet networking library
git submodule update --init --recursive
```

Ensure the Dusklight project is located at `../dusklight` relative to this folder:
```
Projects/
├── dusklight/
│   ├── sdk/
│   ├── include/
│   └── ...
└── tp-multiplayer-mod/
    ├── src/
    ├── extern/enet/
    └── CMakeLists.txt
```

---

## 3. Building the Project

### Using the Automated Script
You can run the build script from cmd or PowerShell:
```cmd
build_and_deploy.bat
```

### Manual Build via Command Line (Recommended)

Open **x64 Native Tools Command Prompt for VS** (or load `vcvars64.bat`), then run:

```cmd
# Configure CMake with Ninja and MSVC
cmake -G Ninja -B build-msvc -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Compile the mod DLL, unit tests, and server simulator
cmake --build build-msvc
```

### Build Artifacts:
- **`build-msvc/mods/tp_multiplayer_mod.dusk`**: The packaged mod archive ready to drop into `dusklight/mods/`.
- **`build-msvc/test_main.exe`**: Unit tests verifying packet serialization and types.
- **`build-msvc/server_simulator.exe`**: Standalone relay server and ghost bot simulator.

---

## 4. Running the Tests & Verifying

### Run Unit Tests
Always run the test suite after modifying serialization or network types:
```cmd
.\build-msvc\test_main.exe
```
Expected output:
```text
===========================================
 TP Multiplayer Mod — Unit Tests
===========================================
[PASS] Position serialization round-trip
[PASS] Status serialization round-trip
[PASS] Protocol wire format (type + playerID + payload)
[PASS] RemotePlayerState default initialization
[PASS] ConnectionConfig environment variable parsing
[PASS] PacketType enum values
===========================================
 All tests passed!
===========================================
```

### Solo Testing with Server Ghost Mode
The server simulator includes an automated **Ghost Mode**:
1. Launch `build-msvc\server_simulator.exe`.
2. When only one client connects, the server automatically reflects their position packets offset by `(+150, 0, +150)` as `playerID=200`.
3. In-game, you will see a duplicate remote puppet actor following your movements offset in 3D space, allowing you to test actor spawning, rendering, and interpolation without needing a second physical machine or player.

---

## 5. Architectural Guidelines & Best Practices

Before contributing code, please read [ARCHITECTURE.md](ARCHITECTURE.md).

### Rule 1: Never Touch `dComIfGp_getPlayer(0)` for Remote Players
The Twilight Princess engine is designed as a single-player singleton. Overwriting `g_dComIfG_gameInfo.play.mPlayer[0]` will break the camera and crash the game on deletion. All remote entities **must** be implemented via `daGhostPlayer_c` (`fopAc_ac_c`).

### Rule 2: Keep the Game Thread Responsive
- Dusklight's game loop ticks at 60 Hz. Heavy network operations (blocking sockets, DNS resolution) must run on a background thread or non-blocking polling loops (`enet_host_service` with `0` timeout).
- Never block in `mod_update()`.

### Rule 3: Endianness & Wire Safety
- All network packets must use big-endian serialization (`htons`, `htonl`, or explicit byte shifting).
- GameCube/Wii data is natively big-endian; Dusklight runs on little-endian x86-64 CPUs. Ensure conversion helpers in `PacketSerializer` are used.

---

## 6. How to Add New Features

### Example: Synchronizing New Actions / Animations
1. Update `NetworkTypes.h` to define the state or action IDs.
2. Update `PacketSerializer.cpp` and `PacketSerializer.h` to serialize/deserialize the new fields.
3. Add a test in `src/test_main.cpp` verifying the round-trip serialization.
4. In `src/game/ActorInterface.cpp`, extract the action from the local player (`daAlink_c`) and queue the packet.
5. In `src/game/GhostPlayer.cpp`, receive the state in `Execute()` or `Draw()` to trigger the corresponding animation or model modification.

---

## 7. Git Workflow & Submitting Changes

1. **Branches**: Branch off `develop` (e.g., `feature/anim-sync`, `fix/actor-cleanup`).
2. **Commit Conventions**: Use [Conventional Commits](https://www.conventionalcommits.org/):
   - `feat:` for new capabilities.
   - `fix:` for bug fixes.
   - `docs:` for documentation updates.
   - `refactor:` for code cleanups without functional change.
   - `test:` for adding or updating unit tests.
3. **Pull Request**: Open a pull request targeting `develop` with a clear description of the changes, test results, and any relevant logs or screenshots.
