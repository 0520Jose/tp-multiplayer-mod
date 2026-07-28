cmake_minimum_required(VERSION 3.25)
project(tp_multiplayer_mod LANGUAGES C CXX)

# Habilitar soporte para mods en dusklight
set(DUSK_ENABLE_CODE_MODS ON CACHE BOOL "" FORCE)

# Añadir el repositorio de dusklight (ignorado en git)
add_subdirectory(dusklight EXCLUDE_FROM_ALL)

# Incluir Mod SDK
include(dusklight/cmake/ModSDK.cmake)

# Crear el mod
add_mod(tp_multiplayer_mod
    MOD_JSON mod.json
    FEATURES game
    SOURCES src/main.cpp
)

# ENet ya está expuesto gracias a tu modificación en dusklight/CMakeLists.txt
target_link_libraries(tp_multiplayer_mod PRIVATE enet)
