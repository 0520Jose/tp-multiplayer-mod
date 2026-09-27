# Twilight Princess Multiplayer Mod

A multiplayer mod for *The Legend of Zelda: Twilight Princess* running on the **Dusklight** emulator. Players can see each other in the same world through synchronized ghost actors using the game's native Link actor (`daAlink_c`).

## Requirements

- [Dusklight](https://dusklight.dev) emulator with Mod SDK installed
- A legitimate copy of Twilight Princess (EUR/USA GC/Wii)
- CMake 3.25+
- MSVC (Visual Studio 2022+) or GCC/Mingw64
- ENet (included via `extern/enet`)

## Building

Before building, ensure the Dusklight SDK is installed at `dusklight/`. The SDK provides the CMake toolchain (`dusklight/cmake/ModSDK.cmake`) and the game headers.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The compiled mod is output to `build/mods/tp_multiplayer_mod.dusk`. Copy it to `dusklight/mods/` to load it.

Alternatively, use the provided script:

```bat
build_and_deploy.bat
```

## Running

1. Start the dedicated server:
   ```bat
   build\server_simulator.exe
   ```
2. Launch Dusklight with the mod loaded and open a save file.
3. The mod connects automatically to `127.0.0.1:1234` by default.

To connect to a remote server, set environment variables before launching:

```bat
set TP_MULTIPLAYER_HOST=192.168.1.10
set TP_MULTIPLAYER_PORT=1234
```

## Architecture

| Module | Path | Description |
|---|---|---|
| Mod lifecycle | `src/main.cpp` | initialize / update / shutdown, hooks |
| Player sync | `src/game/ActorInterface.h/.cpp` | Remote player interpolation and ghost actor management |
| Network client | `src/network/Client.h/.cpp` | ENet UDP client with auto-reconnect |
| Packet protocol | `src/network/PacketSerializer.h/.cpp` | Serialization with network byte order |
| Types | `src/network/NetworkTypes.h` | Packet types and `RemotePlayerState` |
| Server | `src/server_simulator.cpp` | Relay server with Ghost Mode for solo testing |

### Network protocol

All packets follow the format: `[PacketType: 1 byte][PlayerID: 1 byte][Payload: N bytes]`

| Type | ID | Payload | Description |
|---|---|---|---|
| POSITION | 0 | 25 bytes | x, y, z, rotY, stageName, roomNo |
| STATUS | 1 | 8 bytes | health, maxHealth, animationId |
| PLAYER_ASSIGN | 2 | — | Server assigns playerID on connect |
| PLAYER_DISCONNECT | 3 | — | Server notifies when a player leaves |

## Development

This project follows [Git Flow](https://nvie.com/posts/a-successful-git-branching-model/) with [Conventional Commits](https://www.conventionalcommits.org/) and [Semantic Versioning](https://semver.org/).

See [CHANGELOG.md](CHANGELOG.md) for the version history.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

The Dusklight SDK, emulator binaries, and all Twilight Princess game assets are the property of their respective owners and are not covered by this license.
