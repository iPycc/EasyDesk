#include "sessions/files/session_file_transport.h"

#include "media/webrtc/media_session.h"

namespace {
constexpr int kMaximumMessageBytes = 16384;

bool validChannel(FileChannel channel) {
    return channel == FileChannel::Metadata || channel == FileChannel::Data;
}

QByteArray labelFor(FileChannel channel) {
    return channel == FileChannel::Metadata ? QByteArrayLiteral("files.meta.v1")
                                             : QByteArrayLiteral("files.data.v1");
}

bool validMessage(const QByteArray &message) {
    return !message.isEmpty() && message.size() <= kMaximumMessageBytes;
}
} // namespace

SessionFileTransport::SessionFileTransport(QObject *parent) : QObject(parent) {}

bool SessionFileTransport::start(MediaSession &session, SessionCapabilities capabilities,
                                 bool localMayReceive, bool peerMayReceive) {
    if (m_session || !session.isActive() ||
        capabilities.testFlag(SessionCapability::TransferFiles) != (localMayReceive || peerMayReceive))
        return false;
    m_session = &session;
    m_permitted = capabilities.testFlag(SessionCapability::TransferFiles);
    m_localMayReceive = localMayReceive;
    m_peerMayReceive = peerMayReceive;
    return true;
}

void SessionFileTransport::stop() {
    const bool wasAvailable = isOpen(FileChannel::Metadata) || isOpen(FileChannel::Data);
    m_session = nullptr;
    m_permitted = m_ready = m_metadataOpen = m_dataOpen = false;
    m_localMayReceive = m_peerMayReceive = false;
    if (wasAvailable)
        emit availabilityChanged();
}

void SessionFileTransport::setSessionReady(bool ready) {
    const bool beforeMetadata = isOpen(FileChannel::Metadata);
    const bool beforeData = isOpen(FileChannel::Data);
    m_ready = ready && m_session && m_permitted;
    if (beforeMetadata != isOpen(FileChannel::Metadata) || beforeData != isOpen(FileChannel::Data))
        emit availabilityChanged();
}

void SessionFileTransport::setChannelOpen(const QByteArray &label, bool open) {
    if (!m_session || (label != "files.meta.v1" && label != "files.data.v1"))
        return;
    const bool beforeMetadata = isOpen(FileChannel::Metadata);
    const bool beforeData = isOpen(FileChannel::Data);
    if (label == "files.meta.v1")
        m_metadataOpen = open;
    else
        m_dataOpen = open;
    if (beforeMetadata != isOpen(FileChannel::Metadata) || beforeData != isOpen(FileChannel::Data))
        emit availabilityChanged();
}

void SessionFileTransport::receive(const QByteArray &label, const QByteArray &message) {
    if (label != "files.meta.v1" && label != "files.data.v1")
        return;
    const FileChannel channel = label == "files.meta.v1" ? FileChannel::Metadata : FileChannel::Data;
    if (isOpen(channel) && (channel != FileChannel::Data || m_localMayReceive) && validMessage(message))
        emit messageReceived(channel, message);
}

bool SessionFileTransport::isOpen(FileChannel channel) const {
    if (!validChannel(channel) || !m_session || !m_session->isActive() || !m_permitted || !m_ready ||
        !m_session->permitsDataChannel(labelFor(channel)))
        return false;
    return channel == FileChannel::Metadata ? m_metadataOpen : m_dataOpen;
}

SessionFileTransport::SendResult SessionFileTransport::send(FileChannel channel,
                                                              const QByteArray &message) {
    if (!isOpen(channel) || (channel == FileChannel::Data && !m_peerMayReceive) || !validMessage(message))
        return SendResult::Unavailable;
    switch (m_session->sendData(labelFor(channel), message)) {
    case MediaSession::SendResult::Sent: return SendResult::Sent;
    case MediaSession::SendResult::WouldBlock: return SendResult::WouldBlock;
    case MediaSession::SendResult::Failed: return SendResult::Failed;
    }
    return SendResult::Failed;
}
