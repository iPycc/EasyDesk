#pragma once

#include <QByteArray>
#include <QString>
#include <QSet>
#include <memory>

class SendFileSource;
namespace easydesk::client::transfer { struct FileReceipt; }

class SessionFileSender final {
  public:
    enum class State { Begin, Chunks, Commit, AwaitingCommit, Completed, Failed };
    static std::unique_ptr<SessionFileSender> create(const QByteArray &transferId,
                                                      std::unique_ptr<SendFileSource> source,
                                                      QString &error, quint32 windowBytes = 512 * 1024);
    ~SessionFileSender();

    bool nextFrame(QByteArray &frame, QString &error);
    bool markQueued(QString &error);
    bool applyReceipt(const easydesk::client::transfer::FileReceipt &receipt, QString &error);
    bool canQueue() const;
    State state() const;
    quint64 queuedBytes() const;
    quint64 verifiedBytes() const;
    void cancel();

  private:
    SessionFileSender(const QByteArray &transferId, std::unique_ptr<SendFileSource> source, quint32 windowBytes);
    QByteArray m_transferId;
    std::unique_ptr<SendFileSource> m_source;
    QByteArray m_pending;
    quint64 m_queuedBytes = 0;
    quint64 m_verifiedBytes = 0;
    quint32 m_windowBytes = 0;
    quint32 m_pendingBytes = 0;
    QSet<quint64> m_chunkBoundaries;
    State m_state = State::Begin;
};
