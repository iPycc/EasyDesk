#if defined(_WIN32)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "config/application_config.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace easydesk::config {
namespace {

std::string readString(const char* name, std::string fallback) {
    const char* value = std::getenv(name);
    return value == nullptr || *value == '\0' ? std::move(fallback) : value;
}

template <typename Value>
Value readNumber(const char* name, Value fallback, Value minimum, Value maximum) {
    const char* raw = std::getenv(name);
    if (raw == nullptr || *raw == '\0') {
        return fallback;
    }

    Value value{};
    const std::string_view text(raw);
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value < minimum ||
        value > maximum) {
        throw std::runtime_error(std::string(name) + " has an invalid value");
    }
    return value;
}

}  // namespace

ApplicationConfig ApplicationConfig::fromEnvironment() {
    const auto hardwareThreads = std::max(1U, std::thread::hardware_concurrency());
    return {
        .bindAddress = readString("EASYDESK_BIND_ADDRESS", "0.0.0.0"),
        .databasePath = readString("EASYDESK_DATABASE_PATH", "data/easydesk.sqlite3"),
        .port = readNumber<std::uint16_t>("EASYDESK_PORT", 8080, 1, 65535),
        .workerThreads = readNumber<std::size_t>(
            "EASYDESK_WORKER_THREADS", std::min<std::size_t>(hardwareThreads, 8), 1, 64),
    };
}

}  // namespace easydesk::config
