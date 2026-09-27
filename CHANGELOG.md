# Changelog

All notable changes to this project are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Added
- Git Flow branching structure (`main`, `develop`, `feature/*`)
- MIT License (`LICENSE`)
- Project `README.md` with build and run instructions
- `CHANGELOG.md`
- Private `.agent/` workspace for agent context, bitacora, and workflow documentation

### Changed
- `.gitignore` updated to exclude `.agent/` directory

---

## [0.1.0] - 2026-07-29

Initial development iteration. All work done on `feature/init-multiplayer-core`.

### Added
- CMake build system with ENet dependency via `extern/enet`
- `mod.json` metadata for Dusklight mod loader
- ENet UDP network client with auto-reconnect and 60-second timeout
- Dedicated relay server (`server_simulator.cpp`) with authoritative playerID assignment
- Ghost Mode on server: single connected player receives an echoed packet with a position offset to simulate a second player
- Packet protocol: POSITION (25 bytes), STATUS (8 bytes), PLAYER_ASSIGN, PLAYER_DISCONNECT
- Packet serialization with network byte order (htonl/ntohl)
- `RemotePlayerState` with linear interpolation (LERP_SPEED = 0.15, 15 Hz network update rate)
- Angle interpolation using shortest-path for Y rotation
- Idle timeout: remote players removed after 300 frames (~10 seconds at 30 fps)
- Stage isolation: remote actors only visible when on the same stage
- Ghost actor system using native `daAlink_c` actor (`fopAcM_create(fpcNm_ALINK_e, ...)`)
- Pre/post hooks on `daAlink_c::execute` to spoof controller input for ghost actors
- Pre/post hooks on `daAlink_c::draw` to temporarily assign Player[0] pointer to ghost during draw
- Player readiness guard: 120-frame delay after actor load before activating sync
- Title screen and loading screen protection via `maxHealth > 0` check
- Send rate control: position and status sent every 2 frames (~15 Hz)
- `ConnectionConfig`: host and port configurable via environment variables (`TP_MULTIPLAYER_HOST`, `TP_MULTIPLAYER_PORT`)
- `build_and_deploy.bat` build and copy script

### Known Issues
- `dusklight/` directory must contain the Dusklight SDK source (with `cmake/ModSDK.cmake`) for CMake configuration to succeed. The binary-only distribution does not include it.
- `roomNo` field is hardcoded to 0 due to a linker error with the SDK function for current room number.
- `CreateRemoteModelIfNeeded` is disabled; calling it causes a crash at offset `0x650` inside `daAlink_c`.
- `dComIfGp_setPlayerPtr` usage must be verified against the actual SDK — it may be an alias or may not exist.

---

[Unreleased]: https://github.com/0520Jose/tp-multiplayer-mod/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/0520Jose/tp-multiplayer-mod/releases/tag/v0.1.0
