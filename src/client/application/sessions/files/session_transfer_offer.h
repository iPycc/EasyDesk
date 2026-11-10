#pragma once

#include "network/files/protocol/transfer_offer_codec.h"
#include "network/files/protocol/transfer_decision_codec.h"

#include <QObject>
#include <QTimer>
#include <memory>

class ReceiveDirectory;
class SessionNegotiationCoordinator;
class SessionFileIncoming;

class SessionTransferOffer final : public QObject {
    Q_OBJECT

  public:
    explicit SessionTransferOffer(SessionNegotiationCoordinator &session, QObject *parent = nullptr);
    bool sendOffer(const easydesk::client::transfer::TransferManifest &manifest, QString &error);
    bool acceptIncoming(const std::shared_ptr<ReceiveDirectory> &directory, quint32 windowBytes,
                        QString &error);
    bool declineIncoming(QString &error);
    void reset();

  signals:
    void offerSent(const QByteArray &transferId);
    void offerReceived(const easydesk::client::transfer::TransferManifest &manifest);
    void decisionSent(easydesk::client::transfer::OfferDecision decision);
    void decisionReceived(const easydesk::client::transfer::TransferDecision &decision);
    void failed(const QString &error);

  private:
    friend class SessionFileIncoming;
    void finishIncoming(const QByteArray &transferId);
    void sendNext();
    void receive(const QByteArray &message);
    void receiveDecision(const QByteArray &message);
    bool respond(easydesk::client::transfer::OfferDecision decision,
                 const std::shared_ptr<ReceiveDirectory> &directory, quint32 windowBytes,
                 QString &error);
    void abort(const QString &error);

    SessionNegotiationCoordinator &m_session;
    easydesk::client::transfer::TransferOfferDecoder m_decoder;
    QTimer m_sendTimer;
    QTimer m_sendTimeout;
    QTimer m_receiveTimer;
    QTimer m_decisionTimer;
    QVector<QByteArray> m_frames;
    QByteArray m_outgoingId;
    QByteArray m_outgoingDigest;
    QByteArray m_incomingId;
    QByteArray m_incomingDigest;
    std::shared_ptr<ReceiveDirectory> m_acceptedDirectory;
    int m_nextFrame = 0;
    bool m_outgoingPending = false;
    bool m_outgoingSent = false;
    bool m_outgoingResolved = false;
    bool m_incomingPending = false;
    bool m_incomingResolved = false;
    bool m_receiving = false;
};
