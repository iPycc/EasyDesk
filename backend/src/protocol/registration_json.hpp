#pragma once

#include "protocol/registration.hpp"

#include <boost/json/fwd.hpp>

#include <string>
#include <variant>

namespace easydesk::protocol {

struct ProtocolError {
    std::string code;
    std::string message;
};

using RegistrationParseResult = std::variant<RegistrationEnvelope, ProtocolError>;

RegistrationParseResult parseRegistrationResponse(const boost::json::object& object);

}  // namespace easydesk::protocol
