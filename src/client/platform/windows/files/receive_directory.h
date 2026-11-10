#pragma once

#include "features/transfers/transfer_manifest.h"

#include <memory>

class ReceiveFile;
struct ReceiveDirectoryState;

class ReceiveDirectory final {
  public:
    static std::unique_ptr<ReceiveDirectory> open(const QString &directory, QString &error);
    ~ReceiveDirectory();

    ReceiveDirectory(const ReceiveDirectory &) = delete;
    ReceiveDirectory &operator=(const ReceiveDirectory &) = delete;

    QByteArray rootId() const;
    std::unique_ptr<ReceiveFile> createFile(
        const easydesk::client::transfer::ManifestFile &file, QString &error) const;

  private:
    explicit ReceiveDirectory(std::shared_ptr<ReceiveDirectoryState> state);
    std::shared_ptr<ReceiveDirectoryState> m_state;
};
