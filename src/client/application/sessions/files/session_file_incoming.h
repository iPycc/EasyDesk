#pragma once

#include "features/transfers/transfer_manifest.h"
#include "sessions/files/session_file_receive_worker.h"

#include <QObject>
#include <QQueue>
#include <QTimer>
#include <memory>

class ReceiveDirectory;
class SessionNegotiationCoordinator;
class SessionTransferOffer;
class TrustedPeerStore;

class SessionFileIncoming final : public QObject {
    Q_OBJECT

  public:
    SessionFileIncoming(SessionNegotiationCoordinator &session, SessionTransferOffer &offer,
                        TrustedPeerStore &trustedPeers, QObject *parent = nullptr);
    ~SessionFileIncoming() override;
    bool accept(const std::shared_ptr<ReceiveDirectory> &directory, quint32 windowBytes, QString &error);
    void stop();

  signals:
    void fileCommitted(const QString &name);
    void batchCompleted();
    void failed(const QString &error);

  private:
    void queueReceipt(const QByteArray &receipt);
    void flushReceipts();
    void fail(const QString &error);

    SessionNegotiationCoordinator &m_session;
    SessionTransferOffer &m_offer;
    TrustedPeerStore &m_trustedPeers;
    SessionFileReceiveWorker m_worker;
    easydesk::client::transfer::TransferManifest m_manifest;
    QQueue<QByteArray> m_receipts;
    QTimer m_retry;
    QTimer m_timeout;
    qsizetype m_receiptBytes = 0;
    bool m_hasOffer = false;
    bool m_active = false;
    bool m_batchDone = false;
};
