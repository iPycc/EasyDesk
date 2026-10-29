#pragma once

#include "protocol/registration_json.hpp"
#include "protocol/signed_envelope.hpp"

#include <boost/json/fwd.hpp>

#include <string>
#include <variant>

namespace easydesk::protocol {

using EnvelopeParseResult = std::variant<SignedEnvelope, ProtocolError>;

EnvelopeParseResult parseSignedEnvelope(const boost::json::object& object);
boost::json::object signedEnvelopeToJson(const SignedEnvelope& envelope,
                                         const std::string& sourceDeviceId);

}  // namespace easydesk::protocol
