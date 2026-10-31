#if defined(_WIN32)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "application/config/application_config.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace easydesk::config {
namespace {

std::string readString(const char* name, std::string fallback) {
    const char* value = std::getenv(name);
    return value == nullptr || *value == '\0' ? std::move(fallback) : value;
}

std::optional<std::string> readOptionalString(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0')
        return std::nullopt;
    return std::string(value);
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

std::string trim(std::string_view value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
                          return std::isspace(character) != 0;
                      }).base();
    return first < last ? std::string(first, last) : std::string{};
}

std::vector<std::string> parseTurnUrls(std::string_view value) {
    std::vector<std::string> urls;
    std::size_t offset = 0;
    while (offset <= value.size()) {
        const std::size_t separator = value.find(',', offset);
        auto url = trim(value.substr(
            offset, separator == std::string_view::npos ? value.size() - offset
                                                        : separator - offset));
        if (url.empty())
            throw std::runtime_error("EASYDESK_TURN_URLS contains an empty URL");
        urls.push_back(std::move(url));
        if (separator == std::string_view::npos)
            break;
        offset = separator + 1;
    }
    return urls;
}

std::string readTurnSecret(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        throw std::runtime_error("EASYDESK_TURN_SECRET_FILE cannot be opened");
    std::string secret((std::istreambuf_iterator<char>(stream)),
                       std::istreambuf_iterator<char>());
    secret.erase(std::remove_if(secret.begin(), secret.end(), [](char character) {
                     return character == '\r' || character == '\n';
                 }),
                 secret.end());
    if (secret.size() != 64 ||
        !std::all_of(secret.begin(), secret.end(), [](unsigned char character) {
            return std::isxdigit(character) != 0;
        })) {
        throw std::runtime_error("TURN shared secret must contain 64 hexadecimal characters");
    }
    return secret;
}

std::optional<TurnConfig> readTurnConfig() {
    const auto secretFile = readOptionalString("EASYDESK_TURN_SECRET_FILE");
    const auto urls = readOptionalString("EASYDESK_TURN_URLS");
    if (!secretFile && !urls)
        return std::nullopt;
    if (!secretFile || !urls)
        throw std::runtime_error("TURN secret file and URLs must be configured together");
    const auto lifetime = readNumber<std::int64_t>(
        "EASYDESK_TURN_CREDENTIAL_LIFETIME_SECONDS", 600, 60, 1800);
    return TurnConfig{
        .sharedSecret = readTurnSecret(*secretFile),
        .urls = parseTurnUrls(*urls),
        .credentialLifetime = std::chrono::seconds(lifetime),
    };
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
        .turn = readTurnConfig(),
    };
}

}  // namespace easydesk::config
