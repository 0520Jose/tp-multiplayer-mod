#pragma once
#include <cstdint>
#include <cstdlib>
#include <string>
#include <fstream>

#ifdef s_host
#undef s_host
#endif

// =============================================================================
// ConnectionConfig — Host & Port Persistence and Management
// =============================================================================
// Handles reading/writing server settings to 'multiplayer_config.txt',
// supporting in-game UI configuration, environment variables, and defaults.
// =============================================================================

class ConnectionConfig {
public:
    static std::string& HostRef() {
        static std::string s_host = "";
        return s_host;
    }

    static uint16_t& PortRef() {
        static uint16_t s_port = 0;
        return s_port;
    }

    static bool& LoadedRef() {
        static bool s_loaded = false;
        return s_loaded;
    }

    static void Reset() {
        HostRef().clear();
        PortRef() = 0;
        LoadedRef() = false;
    }

    static std::string GetConfiguredHost(const std::string& fallback = "127.0.0.1") {
        EnsureLoaded();

        const char* host = std::getenv("TP_MULTIPLAYER_HOST");
        if (host && host[0] != '\0') {
            return host;
        }

        if (!HostRef().empty()) {
            return HostRef();
        }

        return fallback;
    }

    static uint16_t GetConfiguredPort(uint16_t fallback = 1234) {
        EnsureLoaded();

        const char* portValue = std::getenv("TP_MULTIPLAYER_PORT");
        if (portValue && portValue[0] != '\0') {
            char* end = nullptr;
            long parsed = std::strtol(portValue, &end, 10);
            if (end != portValue && parsed > 0 && parsed <= 65535) {
                return static_cast<uint16_t>(parsed);
            }
        }

        if (PortRef() != 0) {
            return PortRef();
        }

        return fallback;
    }

    static void SetHost(const std::string& host) {
        HostRef() = host;
    }

    static void SetPort(uint16_t port) {
        PortRef() = port;
    }

    static void Save() {
        std::ofstream out("multiplayer_config.txt");
        if (out.is_open()) {
            out << HostRef() << "\n";
            out << PortRef() << "\n";
        }
    }

    static void EnsureLoaded() {
        if (LoadedRef()) return;
        LoadedRef() = true;

        std::ifstream in("multiplayer_config.txt");
        if (in.is_open()) {
            std::string line;
            if (std::getline(in, line)) {
                while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
                    line.pop_back();
                }
                if (!line.empty()) HostRef() = line;
            }
            if (std::getline(in, line)) {
                try {
                    int p = std::stoi(line);
                    if (p > 0 && p <= 65535) PortRef() = static_cast<uint16_t>(p);
                } catch (...) {}
            }
        }
    }
};
