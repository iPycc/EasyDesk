#pragma once

#include "features/transfers/transfer_manifest.h"

#include <memory>

struct SendFileSourceState;

class SendFileSource final {
  public:
    // Opening hashes the selected file; call from a worker thread for large files.
    static std::unique_ptr<SendFileSource> open(const QString &localPath, QString &error);
    ~SendFileSource();

    SendFileSource(const SendFileSource &) = delete;
    SendFileSource &operator=(const SendFileSource &) = delete;

    const easydesk::client::transfer::ManifestFile &manifest() const;
    bool checkUnchanged(QString &error);
    bool readChunk(quint64 offset, quint32 maximumBytes, QByteArray &bytes,
                   QByteArray &sha256, QString &error);

  private:
    explicit SendFileSource(std::unique_ptr<SendFileSourceState> state);
    std::unique_ptr<SendFileSourceState> m_state;
};
