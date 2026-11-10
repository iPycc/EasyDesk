#include "sessions/files/session_file_receiver.h"

#include "files/receive_directory.h"
#include "files/receive_file.h"
#include "network/files/protocol/file_data_codec.h"
#include "network/files/protocol/file_receipt_codec.h"

#include <type_traits>

using namespace easydesk::client::transfer;

SessionFileReceiver::SessionFileReceiver(const TransferManifest &manifest,
                                         std::shared_ptr<ReceiveDirectory> directory)
    : m_manifest(manifest), m_directory(std::move(directory)) {}
SessionFileReceiver::~SessionFileReceiver() = default;

std::unique_ptr<SessionFileReceiver> SessionFileReceiver::create(
    const TransferManifest &manifest, std::shared_ptr<ReceiveDirectory> directory, QString &error) {
    quint64 total = 0;
    if (!directory || !validateManifest(manifest, kDefaultMaximumFileBytes, total, error)) {
        if (error.isEmpty()) error = QStringLiteral("Receive directory is unavailable.");
        return {};
    }
    error.clear();
    return std::unique_ptr<SessionFileReceiver>(new SessionFileReceiver(manifest, std::move(directory)));
}

void SessionFileReceiver::cancel() {
    m_file.reset();
    m_directory.reset();
    m_failed = true;
}

SessionFileReceiver::Result SessionFileReceiver::fail(QString &error, const QString &reason) {
    cancel();
    error = reason;
    return Result::Failed;
}

SessionFileReceiver::Result SessionFileReceiver::receive(const QByteArray &message,
                                                          QString &committedName, QString &error,
                                                          FileReceipt *receipt) {
    committedName.clear();
    if (receipt) *receipt = {};
    FileDataMessage decoded;
    if (m_failed || !m_directory || m_completed.size() == m_manifest.files.size() ||
        !decodeFileDataMessage(message, decoded, error))
        return fail(error, QStringLiteral("Invalid or interrupted file transfer."));

    return std::visit([&](const auto &value) -> Result {
        using T = std::decay_t<decltype(value)>;
        const auto &id = value.identity;
        if (id.transferId != m_manifest.transferId)
            return fail(error, QStringLiteral("File does not belong to the approved transfer."));
        if constexpr (std::is_same_v<T, FileBegin>) {
            if (m_file || m_completed.contains(id.fileId))
                return fail(error, QStringLiteral("File was already started or completed."));
            int index = -1;
            for (int i = 0; i < m_manifest.files.size(); ++i)
                if (m_manifest.files[i].fileId == id.fileId) { index = i; break; }
            if (index < 0 || m_manifest.files[index].sourceGeneration != id.generation ||
                m_manifest.files[index].size != value.size)
                return fail(error, QStringLiteral("File does not match the approved manifest."));
            m_file = m_directory->createFile(m_manifest.files[index], error);
            if (!m_file)
                return fail(error, QStringLiteral("Could not create the receive file."));
            m_current = index;
            m_chunkSize = value.chunkSize;
            m_offset = 0;
            if (receipt) { receipt->identity = id; receipt->verifiedOffset = 0; }
            error.clear();
            return Result::Began;
        } else {
            if (!m_file || m_current < 0 || id.fileId != m_manifest.files[m_current].fileId ||
                id.generation != m_manifest.files[m_current].sourceGeneration)
                return fail(error, QStringLiteral("File message does not match the active file."));
            if constexpr (std::is_same_v<T, FileChunk>) {
                if (value.offset != m_offset || value.bytes.size() > m_chunkSize ||
                    !m_file->append(value.offset, value.bytes, value.sha256, error))
                    return fail(error, QStringLiteral("File chunk failed verification."));
                m_offset += static_cast<quint64>(value.bytes.size());
                if (receipt) { receipt->identity = id; receipt->verifiedOffset = m_offset; }
                error.clear();
                return Result::ChunkAccepted;
            } else {
                const auto &file = m_manifest.files[m_current];
                if (value.size != file.size || value.sha256 != file.sha256 || m_offset != file.size ||
                    !m_file->commit(committedName, error))
                    return fail(error, QStringLiteral("File commit failed verification."));
                m_completed.insert(id.fileId);
                if (receipt) {
                    receipt->identity = id;
                    receipt->verifiedOffset = m_offset;
                    receipt->kind = ReceiptKind::Committed;
                    receipt->durability = Durability::OsFlushed;
                    receipt->finalName = committedName;
                }
                m_file.reset();
                m_current = -1;
                error.clear();
                return m_completed.size() == m_manifest.files.size() ? Result::BatchCompleted
                                                                      : Result::FileCommitted;
            }
        }
    }, decoded);
}
