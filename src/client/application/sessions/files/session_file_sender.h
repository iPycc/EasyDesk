#pragma once

#include <QByteArray>
#include <QString>
#include <memory>

class SendFileSource;

class SessionFileSender final {
  public:
    enum class State { Begin, Chunks, Commit, AwaitingCommit, Failed };
    static std::unique_ptr<SessionFileSender> create(const QByteArray &transferId,
                                                      std::unique_ptr<SendFileSource> source,
                                                      QString &error);
    ~SessionFileSender();

    bool nextFrame(QByteArray &frame, QString &error);
    bool markQueued(QString &error);
    State state() const;
    quint64 queuedBytes() const;
    void cancel();

  private:
    SessionFileSender(const QByteArray &transferId, std::unique_ptr<SendFileSource> source);
    QByteArray m_transferId;
    std::unique_ptr<SendFileSource> m_source;
    QByteArray m_pending;
    quint64 m_queuedBytes = 0;
    quint32 m_pendingBytes = 0;
    State m_state = State::Begin;
};
