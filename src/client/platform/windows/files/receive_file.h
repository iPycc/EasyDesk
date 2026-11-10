#pragma once

#include <QByteArray>
#include <QString>

#include <memory>
#include <atomic>

struct ReceiveFileState;

class ReceiveFile final {
  public:
    ~ReceiveFile();
    ReceiveFile(const ReceiveFile &) = delete;
    ReceiveFile &operator=(const ReceiveFile &) = delete;

    bool append(quint64 offset, const QByteArray &bytes, const QByteArray &chunkSha256, QString &error,
                const std::atomic_bool *cancelled = nullptr);
    bool commit(QString &finalName, QString &error, const std::atomic_bool *cancelled = nullptr);
    void cancel();

  private:
    friend class ReceiveDirectory;
    explicit ReceiveFile(std::unique_ptr<ReceiveFileState> state);
    std::unique_ptr<ReceiveFileState> m_state;
};
