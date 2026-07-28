#pragma once
#include <cstdint>
#include <cstdlib>
#include <string>

class ConnectionConfig {
public:
    static std::string GetConfiguredHost(const std::string& fallback) {
        const char* host = std::getenv("TP_MULTIPLAYER_HOST");
        if (host && host[0] != '\0') {
            return host;
        }
        return fallback;
    }

    static uint16_t GetConfiguredPort(uint16_t fallback) {
        const char* portValue = std::getenv("TP_MULTIPLAYER_PORT");
        if (portValue && portValue[0] != '\0') {
            char* end = nullptr;
            long parsed = std::strtol(portValue, &end, 10);
            if (end != portValue && parsed > 0 && parsed <= 65535) {
                return static_cast<uint16_t>(parsed);
            }
        }
        return fallback;
    }
};
