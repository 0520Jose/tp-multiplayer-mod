cmake_minimum_required(VERSION 3.25)
project(tp_multiplayer_mod LANGUAGES C CXX)

# Habilitar soporte para mods en dusklight
set(DUSK_ENABLE_CODE_MODS ON CACHE BOOL "" FORCE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/Zc:__cplusplus)
endif()

# Añadir el SDK de dusklight
set(DUSK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../dusklight" CACHE PATH "Path to dusk source root")
if(WIN32 AND NOT DUSK_GAME_EXE)
    set(DUSK_GAME_EXE "${CMAKE_CURRENT_SOURCE_DIR}/dusklight/sdk/windows-amd64.lib" CACHE FILEPATH "Game import library" FORCE)
endif()
add_subdirectory("${DUSK_DIR}/sdk" dusk-sdk EXCLUDE_FROM_ALL)

# Crear el mod
add_mod(tp_multiplayer_mod
    MOD_JSON mod.json
    FEATURES game
    SOURCES 
        src/main.cpp
        src/network/PacketSerializer.cpp
        src/network/Client.cpp
        src/game/ActorInterface.cpp
        src/game/GhostPlayer.cpp
        src/game/WorldSync.cpp
        src/game/MapTracker.cpp
        src/game/DebugController.cpp
)

add_subdirectory(extern/enet)
target_include_directories(enet PUBLIC extern/enet/include)

# Executable for unit testing
add_executable(test_main src/test_main.cpp src/network/PacketSerializer.cpp)
target_include_directories(test_main PRIVATE src)
if(WIN32)
    target_link_libraries(test_main PRIVATE ws2_32)
endif()

# Executable for testing the server (dummy player simulation)
add_executable(server_simulator src/server_simulator.cpp src/network/PacketSerializer.cpp)
target_include_directories(server_simulator PRIVATE src)
target_link_libraries(server_simulator PRIVATE enet)

target_link_libraries(tp_multiplayer_mod PRIVATE enet)
if(WIN32)
    target_link_libraries(tp_multiplayer_mod PRIVATE user32)
endif()
target_include_directories(tp_multiplayer_mod PRIVATE ${CMAKE_BINARY_DIR}/_deps/tracy-src/public)
