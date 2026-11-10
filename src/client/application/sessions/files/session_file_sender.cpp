#include "sessions/files/session_file_sender.h"

#include "files/send_file_source.h"
#include "network/files/protocol/file_data_codec.h"

using namespace easydesk::client::transfer;

SessionFileSender::SessionFileSender(const QByteArray &transferId, std::unique_ptr<SendFileSource> source)
    : m_transferId(transferId), m_source(std::move(source)) {}
SessionFileSender::~SessionFileSender() = default;

std::unique_ptr<SessionFileSender> SessionFileSender::create(const QByteArray &transferId,
                                                              std::unique_ptr<SendFileSource> source,
                                                              QString &error) {
    if (transferId.size() != 16 || transferId == QByteArray(16, '\0') || !source) {
        error = QStringLiteral("File sender context is invalid.");
        return {};
    }
    error.clear();
    return std::unique_ptr<SessionFileSender>(new SessionFileSender(transferId, std::move(source)));
}

bool SessionFileSender::nextFrame(QByteArray &frame, QString &error) {
    frame.clear();
    if (!m_source || m_state == State::Failed || m_state == State::AwaitingCommit) {
        error = QStringLiteral("File sender has no frame to send.");
        return false;
    }
    if (!m_source->checkUnchanged(error)) {
        m_pending.clear();
        m_state = State::Failed;
        return false;
    }
    if (!m_pending.isEmpty()) {
        frame = m_pending;
        error.clear();
        return true;
    }
    const ManifestFile &file = m_source->manifest();
    const FileMessageIdentity identity{m_transferId, file.fileId, file.sourceGeneration};
    FileDataMessage message;
    if (m_state == State::Begin) {
        message = FileBegin{identity, file.size, kMaximumChunkPayload};
    } else if (m_state == State::Chunks) {
        QByteArray bytes, sha256;
        if (!m_source->readChunk(m_queuedBytes, kMaximumChunkPayload, bytes, sha256, error)) {
            m_state = State::Failed;
            return false;
        }
        m_pendingBytes = static_cast<quint32>(bytes.size());
        message = FileChunk{identity, m_queuedBytes, std::move(sha256), std::move(bytes)};
    } else {
        message = FileCommit{identity, file.size, file.sha256};
    }
    if (!encodeFileDataMessage(message, m_pending, error)) {
        m_state = State::Failed;
        return false;
    }
    frame = m_pending;
    error.clear();
    return true;
}

bool SessionFileSender::markQueued(QString &error) {
    if (m_pending.isEmpty() || !m_source || m_state == State::Failed || m_state == State::AwaitingCommit) {
        error = QStringLiteral("No file frame is pending.");
        return false;
    }
    if (m_state == State::Begin)
        m_state = m_source->manifest().size == 0 ? State::Commit : State::Chunks;
    else if (m_state == State::Chunks) {
        m_queuedBytes += m_pendingBytes;
        if (m_queuedBytes == m_source->manifest().size)
            m_state = State::Commit;
    } else {
        m_state = State::AwaitingCommit;
    }
    m_pending.clear();
    m_pendingBytes = 0;
    error.clear();
    return true;
}

SessionFileSender::State SessionFileSender::state() const { return m_state; }
quint64 SessionFileSender::queuedBytes() const { return m_queuedBytes; }

void SessionFileSender::cancel() {
    m_pending.clear();
    m_pendingBytes = 0;
    m_source.reset();
    m_state = State::Failed;
}
