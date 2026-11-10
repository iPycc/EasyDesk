#include "sessions/files/session_file_sender.h"

#include "files/send_file_source.h"
#include "network/files/protocol/file_data_codec.h"
#include "network/files/protocol/file_receipt_codec.h"
#include "network/files/protocol/transfer_decision_codec.h"

using namespace easydesk::client::transfer;

SessionFileSender::SessionFileSender(const QByteArray &transferId, std::unique_ptr<SendFileSource> source,
                                     quint32 windowBytes)
    : m_transferId(transferId), m_source(std::move(source)), m_windowBytes(windowBytes) {}
SessionFileSender::~SessionFileSender() = default;

std::unique_ptr<SessionFileSender> SessionFileSender::create(const QByteArray &transferId,
                                                              std::unique_ptr<SendFileSource> source,
                                                              QString &error, quint32 windowBytes) {
    if (transferId.size() != 16 || transferId == QByteArray(16, '\0') || !source ||
        windowBytes == 0 || windowBytes > kMaximumReceiveWindow) {
        error = QStringLiteral("File sender context is invalid.");
        return {};
    }
    error.clear();
    return std::unique_ptr<SessionFileSender>(new SessionFileSender(transferId, std::move(source), windowBytes));
}

bool SessionFileSender::nextFrame(QByteArray &frame, QString &error) {
    frame.clear();
    if (!m_source || m_state == State::Failed || m_state == State::AwaitingCommit || m_state == State::Completed) {
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
    if (!canQueue()) {
        error = QStringLiteral("Waiting for file verification acknowledgement.");
        return false;
    }
    const ManifestFile &file = m_source->manifest();
    const FileMessageIdentity identity{m_transferId, file.fileId, file.sourceGeneration};
    FileDataMessage message;
    if (m_state == State::Begin) {
        message = FileBegin{identity, file.size, kMaximumChunkPayload};
    } else if (m_state == State::Chunks) {
        QByteArray bytes, sha256;
        const auto available = static_cast<quint32>(m_windowBytes - (m_queuedBytes - m_verifiedBytes));
        if (!m_source->readChunk(m_queuedBytes, qMin<quint32>(kMaximumChunkPayload, available), bytes, sha256, error)) {
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
    if (m_pending.isEmpty() || !m_source || m_state == State::Failed || m_state == State::AwaitingCommit ||
        m_state == State::Completed) {
        error = QStringLiteral("No file frame is pending.");
        return false;
    }
    if (m_state == State::Begin)
        m_state = m_source->manifest().size == 0 ? State::Commit : State::Chunks;
    else if (m_state == State::Chunks) {
        m_queuedBytes += m_pendingBytes;
        m_chunkBoundaries.insert(m_queuedBytes);
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

bool SessionFileSender::applyReceipt(const FileReceipt &receipt, QString &error) {
    QByteArray encoded;
    if (!m_source || m_state == State::Begin || m_state == State::Failed || m_state == State::Completed ||
        !encodeFileReceipt(receipt, encoded, error) || receipt.identity.transferId != m_transferId ||
        receipt.identity.fileId != m_source->manifest().fileId ||
        receipt.identity.generation != m_source->manifest().sourceGeneration ||
        receipt.verifiedOffset < m_verifiedBytes || receipt.verifiedOffset > m_queuedBytes ||
        (receipt.verifiedOffset != m_verifiedBytes && !m_chunkBoundaries.contains(receipt.verifiedOffset)) ||
        (receipt.kind == ReceiptKind::Committed &&
         (m_state != State::AwaitingCommit || receipt.verifiedOffset != m_source->manifest().size))) {
        error = QStringLiteral("File receipt does not match queued data.");
        return false;
    }
    m_verifiedBytes = receipt.verifiedOffset;
    for (auto it = m_chunkBoundaries.begin(); it != m_chunkBoundaries.end(); ) {
        if (*it <= m_verifiedBytes) it = m_chunkBoundaries.erase(it);
        else ++it;
    }
    if (receipt.kind == ReceiptKind::Committed)
        m_state = State::Completed;
    error.clear();
    return true;
}

bool SessionFileSender::canQueue() const {
    if (!m_source || m_state == State::Failed || m_state == State::AwaitingCommit || m_state == State::Completed)
        return false;
    return !m_pending.isEmpty() || m_state != State::Chunks || m_queuedBytes - m_verifiedBytes < m_windowBytes;
}

SessionFileSender::State SessionFileSender::state() const { return m_state; }
quint64 SessionFileSender::queuedBytes() const { return m_queuedBytes; }
quint64 SessionFileSender::verifiedBytes() const { return m_verifiedBytes; }

void SessionFileSender::cancel() {
    m_pending.clear();
    m_pendingBytes = 0;
    m_chunkBoundaries.clear();
    m_source.reset();
    m_state = State::Failed;
}
