#include "application/turn/turn_credential_issuer.hpp"

#include "protocol/encoding/base64url.hpp"

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace easydesk::turn {
namespace {

constexpr std::size_t kMaximumTurnUrls = 8;
constexpr std::size_t kMaximumTurnUrlBytes = 512;

bool isDeviceId(std::string_view value) {
    return value.size() == 9 &&
           std::all_of(value.begin(), value.end(), [](unsigned char character) {
               return std::isdigit(character) != 0;
           });
}

bool validUrl(std::string_view value) {
    return !value.empty() && value.size() <= kMaximumTurnUrlBytes &&
           (value.starts_with("turn:") || value.starts_with("turns:")) &&
           std::none_of(value.begin(), value.end(), [](unsigned char character) {
               return std::isspace(character) != 0 || character == '\0';
           });
}

std::string standardBase64(std::span<const std::uint8_t> bytes) {
    std::string encoded(4 * ((bytes.size() + 2) / 3), '\0');
    const int length = EVP_EncodeBlock(
        reinterpret_cast<unsigned char*>(encoded.data()), bytes.data(),
        static_cast<int>(bytes.size()));
    if (length < 0)
        throw std::runtime_error("TURN credential base64 encoding failed");
    encoded.resize(static_cast<std::size_t>(length));
    return encoded;
}

std::string passwordFor(std::string_view secret, std::string_view username) {
    std::array<std::uint8_t, EVP_MAX_MD_SIZE> digest{};
    unsigned int digestSize = 0;
    if (HMAC(EVP_sha1(), secret.data(), static_cast<int>(secret.size()),
             reinterpret_cast<const unsigned char*>(username.data()), username.size(),
             digest.data(), &digestSize) == nullptr) {
        throw std::runtime_error("TURN credential HMAC failed");
    }
    return standardBase64({digest.data(), digestSize});
}

}  // namespace

TurnCredentialIssuer::TurnCredentialIssuer(std::string sharedSecret,
                                           std::vector<std::string> urls,
                                           std::chrono::seconds lifetime)
    : sharedSecret_(std::move(sharedSecret)),
      urls_(std::move(urls)),
      lifetime_(lifetime) {
    if (sharedSecret_.size() < 32 || sharedSecret_.size() > 1024)
        throw std::invalid_argument("TURN shared secret length is invalid");
    if (urls_.empty() || urls_.size() > kMaximumTurnUrls ||
        !std::all_of(urls_.begin(), urls_.end(), validUrl)) {
        throw std::invalid_argument("TURN URL list is invalid");
    }
    if (lifetime_ < std::chrono::seconds(60) || lifetime_ > std::chrono::minutes(30))
        throw std::invalid_argument("TURN credential lifetime is invalid");
}

std::optional<TurnCredentials> TurnCredentialIssuer::issue(
    const std::string& deviceId,
    const protocol::SessionIdentifier& sessionId,
    std::chrono::system_clock::time_point now) const {
    if (!isDeviceId(deviceId))
        return std::nullopt;
    const auto nowSeconds = std::chrono::duration_cast<std::chrono::seconds>(
                                now.time_since_epoch())
                                .count();
    if (nowSeconds <= 0 ||
        nowSeconds > std::numeric_limits<std::int64_t>::max() - lifetime_.count()) {
        return std::nullopt;
    }

    const std::int64_t expiresAt = nowSeconds + lifetime_.count();
    std::string username = std::to_string(expiresAt);
    username.push_back(':');
    username += deviceId;
    username.push_back(':');
    username += protocol::encodeBase64Url(sessionId);
    return TurnCredentials{
        .urls = urls_,
        .username = username,
        .password = passwordFor(sharedSecret_, username),
        .expiresAt = expiresAt,
    };
}

}  // namespace easydesk::turn
