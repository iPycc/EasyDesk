#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace easydesk::config {

struct ApplicationConfig {
    std::string bindAddress;
    std::uint16_t port;
    std::size_t workerThreads;

    static ApplicationConfig fromEnvironment();
};

}  // namespace easydesk::config
