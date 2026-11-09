#include "sessions/input/session_input_transport.h"

#include "media/webrtc/media_session.h"

namespace {
constexpr int kMaximumReliableBytes = 16384;
constexpr int kMaximumPointerBytes = 256;

bool validStream(InputStream stream) {
    return stream == InputStream::Reliable || stream == InputStream::Pointer;
}

QByteArray channelLabel(InputStream stream) {
    return stream == InputStream::Reliable ? QByteArrayLiteral("input.v1")
                                            : QByteArrayLiteral("pointer.v1");
}

bool validMessage(InputStream stream, const QByteArray &message) {
    return validStream(stream) && !message.isEmpty() &&
           message.size() <= (stream == InputStream::Reliable ? kMaximumReliableBytes
                                                              : kMaximumPointerBytes);
}
}  // namespace

SessionInputTransport::SessionInputTransport(QObject *parent) : QObject(parent) {}

bool SessionInputTransport::start(MediaSession &session, SessionCapabilities capabilities) {
    if (m_session || !session.isActive())
        return false;
    m_session = &session;
    m_permitted = capabilities.testFlag(SessionCapability::ControlInput);
    return true;
}

void SessionInputTransport::stop() {
    const bool wasAvailable = isOpen(InputStream::Reliable) || isOpen(InputStream::Pointer);
    m_session = nullptr;
    m_permitted = false;
    m_ready = false;
    m_reliableOpen = false;
    m_pointerOpen = false;
    if (wasAvailable)
        emit availabilityChanged();
}

void SessionInputTransport::setSessionReady(bool ready) {
    const bool beforeReliable = isOpen(InputStream::Reliable);
    const bool beforePointer = isOpen(InputStream::Pointer);
    m_ready = ready && m_session && m_permitted;
    if (beforeReliable != isOpen(InputStream::Reliable) ||
        beforePointer != isOpen(InputStream::Pointer))
        emit availabilityChanged();
}

void SessionInputTransport::setChannelOpen(const QByteArray &label, bool open) {
    if (!m_session || (label != "input.v1" && label != "pointer.v1"))
        return;
    const bool beforeReliable = isOpen(InputStream::Reliable);
    const bool beforePointer = isOpen(InputStream::Pointer);
    if (label == "input.v1")
        m_reliableOpen = open;
    else
        m_pointerOpen = open;
    if (beforeReliable != isOpen(InputStream::Reliable) ||
        beforePointer != isOpen(InputStream::Pointer))
        emit availabilityChanged();
}

void SessionInputTransport::receive(const QByteArray &label, const QByteArray &message) {
    if (label != "input.v1" && label != "pointer.v1")
        return;
    const InputStream stream = label == "input.v1" ? InputStream::Reliable : InputStream::Pointer;
    if (isOpen(stream) && validMessage(stream, message))
        emit messageReceived(stream, message);
}

bool SessionInputTransport::isOpen(InputStream stream) const {
    if (!validStream(stream) || !m_session || !m_session->isActive() || !m_permitted || !m_ready ||
        !m_session->permitsDataChannel(channelLabel(stream)))
        return false;
    return stream == InputStream::Reliable ? m_reliableOpen : m_pointerOpen;
}

SessionInputTransport::SendResult SessionInputTransport::send(InputStream stream,
                                                               const QByteArray &message) {
    if (!isOpen(stream) || !validMessage(stream, message))
        return SendResult::Unavailable;
    switch (m_session->sendData(channelLabel(stream), message)) {
    case MediaSession::SendResult::Sent: return SendResult::Sent;
    case MediaSession::SendResult::WouldBlock: return SendResult::WouldBlock;
    case MediaSession::SendResult::Failed: return SendResult::Failed;
    }
    return SendResult::Failed;
}
