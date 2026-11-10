#pragma once

#include "sessions/files/session_file_send_worker.h"

#include <QObject>
#include <QTimer>

class SessionNegotiationCoordinator;
class SessionTransferOffer;
class TrustedPeerStore;

class SessionFileOutgoing final : public QObject {
    Q_OBJECT

  public:
    SessionFileOutgoing(SessionNegotiationCoordinator &session, SessionTransferOffer &offer,
                        TrustedPeerStore &trustedPeers, QObject *parent = nullptr);
    ~SessionFileOutgoing() override;
    bool start(const QStringList &paths, QString &error);
    bool cancel(QString &error);
    void stop();

  signals:
    void preparing();
    void awaitingDecision();
    void progress(quint64 queuedBytes, quint64 verifiedBytes);
    void fileCommitted(const QString &name);
    void batchCompleted();
    void declined();
    void cancelled();
    void failed(const QString &error);

  private:
    enum class State { Idle, Preparing, AwaitingDecision, Sending, Cancelling };
    void requestFrame();
    void sendFrame();
    void fail(const QString &error);

    SessionNegotiationCoordinator &m_session;
    SessionTransferOffer &m_offer;
    TrustedPeerStore &m_trustedPeers;
    SessionFileSendWorker m_worker;
    QTimer m_retry;
    QTimer m_timeout;
    QByteArray m_transferId;
    QByteArray m_pendingFrame;
    int m_receiptsPending = 0;
    bool m_frameRequested = false;
    State m_state = State::Idle;
};
