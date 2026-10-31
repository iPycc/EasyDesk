#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace easydesk::config {

struct TurnConfig {
    std::string sharedSecret;
    std::vector<std::string> urls;
    std::chrono::seconds credentialLifetime;
};

struct ApplicationConfig {
    std::string bindAddress;
    std::string databasePath;
    std::uint16_t port;
    std::size_t workerThreads;
    std::optional<TurnConfig> turn;

    static ApplicationConfig fromEnvironment();
};

}  // namespace easydesk::config
