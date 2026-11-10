#pragma once

#include "features/transfers/transfer_manifest.h"

#include <QSet>
#include <memory>
#include <atomic>

class ReceiveDirectory;
class ReceiveFile;
namespace easydesk::client::transfer { struct FileReceipt; }

class SessionFileReceiver final {
  public:
    enum class Result { Began, ChunkAccepted, FileCommitted, BatchCompleted, Failed };
    static std::unique_ptr<SessionFileReceiver> create(
        const easydesk::client::transfer::TransferManifest &manifest,
        std::shared_ptr<ReceiveDirectory> directory, QString &error,
        std::shared_ptr<std::atomic_bool> cancellation = {});
    ~SessionFileReceiver();

    Result receive(const QByteArray &message, QString &committedName, QString &error,
                   easydesk::client::transfer::FileReceipt *receipt = nullptr);
    void cancel();

  private:
    SessionFileReceiver(const easydesk::client::transfer::TransferManifest &manifest,
                        std::shared_ptr<ReceiveDirectory> directory,
                        std::shared_ptr<std::atomic_bool> cancellation);
    Result fail(QString &error, const QString &reason);

    easydesk::client::transfer::TransferManifest m_manifest;
    std::shared_ptr<ReceiveDirectory> m_directory;
    std::shared_ptr<std::atomic_bool> m_cancellation;
    std::unique_ptr<ReceiveFile> m_file;
    QSet<QByteArray> m_completed;
    int m_current = -1;
    quint16 m_chunkSize = 0;
    quint64 m_offset = 0;
    bool m_failed = false;
};
