#include <enet/enet.h>
#include <dusk/mod_api.h>
#include <iostream>

extern "C" DUSK_EXPORT void dusk_mod_init() {
    if (enet_initialize() != 0) {
        std::cerr << "An error occurred while initializing ENet.\n";
        return;
    }
    std::cout << "ENet initialized successfully!\n";
}

extern "C" DUSK_EXPORT void dusk_mod_fini() {
    enet_deinitialize();
}
