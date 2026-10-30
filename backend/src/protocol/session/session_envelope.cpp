#include "protocol/session/session_envelope.hpp"

namespace easydesk::protocol {

std::optional<SessionMessageType> parseSessionMessageType(std::string_view value) {
    if (value == "session.request")
        return SessionMessageType::request;
    if (value == "session.accept")
        return SessionMessageType::accept;
    if (value == "session.reject")
        return SessionMessageType::reject;
    if (value == "session.cancel")
        return SessionMessageType::cancel;
    if (value == "session.offer")
        return SessionMessageType::offer;
    if (value == "session.answer")
        return SessionMessageType::answer;
    if (value == "session.candidates")
        return SessionMessageType::candidates;
    if (value == "session.gathering_complete")
        return SessionMessageType::gatheringComplete;
    if (value == "session.end")
        return SessionMessageType::end;
    if (value == "session.revoke")
        return SessionMessageType::revoke;
    return std::nullopt;
}

std::string_view sessionMessageTypeName(SessionMessageType type) {
    switch (type) {
    case SessionMessageType::request:
        return "session.request";
    case SessionMessageType::accept:
        return "session.accept";
    case SessionMessageType::reject:
        return "session.reject";
    case SessionMessageType::cancel:
        return "session.cancel";
    case SessionMessageType::offer:
        return "session.offer";
    case SessionMessageType::answer:
        return "session.answer";
    case SessionMessageType::candidates:
        return "session.candidates";
    case SessionMessageType::gatheringComplete:
        return "session.gathering_complete";
    case SessionMessageType::end:
        return "session.end";
    case SessionMessageType::revoke:
        return "session.revoke";
    }
    return {};
}

bool sessionMessageRequiresIceGeneration(SessionMessageType type) {
    return type == SessionMessageType::offer || type == SessionMessageType::answer ||
           type == SessionMessageType::candidates ||
           type == SessionMessageType::gatheringComplete;
}

bool sessionMessageRequiresSessionEpoch(SessionMessageType type) {
    return type != SessionMessageType::request && type != SessionMessageType::reject &&
           type != SessionMessageType::cancel;
}

}  // namespace easydesk::protocol
