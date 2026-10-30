#include "media/webrtc/media_session.h"

#include "media/webrtc/media_runtime_state.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace {

uint64_t capabilityMask(SessionCapabilities capabilities) {
    uint64_t mask = 0;
    if (capabilities.testFlag(SessionCapability::ViewDesktop))
        mask |= EASYDESK_MEDIA_CAPABILITY_VIEW_SCREEN;
    if (capabilities.testFlag(SessionCapability::ControlInput))
        mask |= EASYDESK_MEDIA_CAPABILITY_SEND_INPUT;
    if (capabilities.testFlag(SessionCapability::TransferFiles))
        mask |= EASYDESK_MEDIA_CAPABILITY_TRANSFER_FILES;
    return mask;
}

EasyDeskMediaError emptyError() {
    EasyDeskMediaError error{};
    error.size = sizeof(error);
    return error;
}

}  // namespace

MediaSession::MediaSession(std::shared_ptr<MediaRuntimeState> state,
                           EasyDeskMediaSession *session)
    : m_state(std::move(state)), m_session(session) {}

MediaSession::~MediaSession() {
    close();
}

bool MediaSession::createOffer() {
    if (!isActive())
        return false;
    EasyDeskMediaError error = emptyError();
    return acceptStatus(m_state->api.sessionCreateOffer(m_session, &error), error);
}

bool MediaSession::createAnswer() {
    if (!isActive())
        return false;
    EasyDeskMediaError error = emptyError();
    return acceptStatus(m_state->api.sessionCreateAnswer(m_session, &error), error);
}

bool MediaSession::setRemoteDescription(quint32 iceGeneration,
                                        quint32 sdpType,
                                        const QByteArray &sdp) {
    if (!isActive())
        return false;
    EasyDeskMediaError error = emptyError();
    return acceptStatus(m_state->api.sessionSetRemoteDescription(
                            m_session, iceGeneration, sdpType,
                            reinterpret_cast<const uint8_t *>(sdp.constData()),
                            static_cast<uint32_t>(sdp.size()), &error),
                        error);
}

bool MediaSession::addIceCandidate(quint32 iceGeneration,
                                   const QByteArray &sdpMid,
                                   qint32 sdpMlineIndex,
                                   const QByteArray &candidate) {
    if (!isActive())
        return false;
    EasyDeskMediaError error = emptyError();
    return acceptStatus(m_state->api.sessionAddIceCandidate(
                            m_session, iceGeneration,
                            reinterpret_cast<const uint8_t *>(sdpMid.constData()),
                            static_cast<uint32_t>(sdpMid.size()), sdpMlineIndex,
                            reinterpret_cast<const uint8_t *>(candidate.constData()),
                            static_cast<uint32_t>(candidate.size()), &error),
                        error);
}

bool MediaSession::setPermissions(quint64 permissionEpoch,
                                  SessionCapabilities capabilities) {
    if (!isActive())
        return false;
    EasyDeskMediaError error = emptyError();
    const bool accepted = acceptStatus(m_state->api.sessionSetPermissions(
                                           m_session, permissionEpoch,
                                           capabilityMask(capabilities), &error),
                                       error);
    if (accepted && capabilities == SessionCapabilities())
        m_closed = true;
    return accepted;
}

bool MediaSession::pollEvent(MediaSessionEvent &event, quint32 timeoutMs) {
    if (m_session == nullptr || !m_state)
        return false;

    QByteArray storage;
    storage.resize(64 * 1024);
    EasyDeskMediaEvent native{};
    native.size = sizeof(native);
    EasyDeskMediaError error = emptyError();
    uint32_t needed = 0;
    EasyDeskMediaStatus status = m_state->api.sessionPollEvent(
        m_session, &native, reinterpret_cast<uint8_t *>(storage.data()),
        static_cast<uint32_t>(storage.size()), &needed, timeoutMs, &error);
    if (status == EASYDESK_MEDIA_STATUS_BUFFER_TOO_SMALL &&
        needed <= EASYDESK_MEDIA_MAX_EVENT_PAYLOAD_SIZE) {
        storage.resize(static_cast<int>(needed));
        native = EasyDeskMediaEvent{};
        native.size = sizeof(native);
        error = emptyError();
        status = m_state->api.sessionPollEvent(
            m_session, &native, reinterpret_cast<uint8_t *>(storage.data()),
            static_cast<uint32_t>(storage.size()), &needed, 0, &error);
    }
    if (status == EASYDESK_MEDIA_STATUS_NO_EVENT) {
        m_error.clear();
        return false;
    }
    if (status == EASYDESK_MEDIA_STATUS_SESSION_CLOSED) {
        m_closed = true;
        m_error.clear();
        return false;
    }
    if (!acceptStatus(status, error))
        return false;

    storage.resize(static_cast<int>(native.payload_size));
    event.type = native.type;
    event.sequence = native.sequence;
    event.code = native.code;
    event.iceGeneration = native.ice_generation;
    event.sdpMlineIndex = native.sdp_mline_index;
    const int labelSize = std::min<int>(static_cast<int>(native.label_size), storage.size());
    event.label = storage.left(labelSize);
    event.payload = storage.mid(labelSize);
    return true;
}

void MediaSession::close() {
    if (m_session == nullptr || !m_state)
        return;
    EasyDeskMediaError error = emptyError();
    m_state->api.sessionClose(m_session, &error);
    m_state->api.sessionDestroy(m_session);
    m_session = nullptr;
    m_closed = true;
}

bool MediaSession::isActive() const {
    return m_session != nullptr && m_state && !m_closed;
}

QString MediaSession::errorString() const {
    return m_error;
}

bool MediaSession::acceptStatus(EasyDeskMediaStatus status,
                                const EasyDeskMediaError &error) {
    if (status == EASYDESK_MEDIA_STATUS_OK) {
        m_error.clear();
        return true;
    }
    m_error = QString::fromUtf8(error.message).trimmed();
    if (m_error.isEmpty())
        m_error = QStringLiteral("Media operation failed with status %1.")
                      .arg(static_cast<int>(status));
    return false;
}
