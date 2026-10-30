#pragma once

#include "protocol/registration/registration_json.hpp"
#include "protocol/session/session_envelope.hpp"

#include <boost/json/fwd.hpp>

#include <variant>

namespace easydesk::protocol {

using SessionEnvelopeParseResult = std::variant<SessionEnvelope, ProtocolError>;

SessionEnvelopeParseResult parseSessionEnvelope(const boost::json::object& object);
boost::json::object sessionEnvelopeToJson(const SessionEnvelope& envelope);

}  // namespace easydesk::protocol
