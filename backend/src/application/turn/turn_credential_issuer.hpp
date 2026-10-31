#pragma once

#include "protocol/session/session_envelope.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace easydesk::turn {

struct TurnCredentials {
    std::vector<std::string> urls;
    std::string username;
    std::string password;
    std::int64_t expiresAt;
};

class TurnCredentialIssuer final {
public:
    TurnCredentialIssuer(std::string sharedSecret,
                         std::vector<std::string> urls,
                         std::chrono::seconds lifetime);

    std::optional<TurnCredentials> issue(
        const std::string& deviceId,
        const protocol::SessionIdentifier& sessionId,
        std::chrono::system_clock::time_point now) const;

private:
    std::string sharedSecret_;
    std::vector<std::string> urls_;
    std::chrono::seconds lifetime_;
};

}  // namespace easydesk::turn
