#pragma once

#include "sessions/negotiation/ice/local_ice_candidate_buffer.h"
#include "sessions/negotiation/session_negotiation_context.h"

#include <QObject>
#include <QTimer>

class SignalingClient;
class WindowsIdentityStore;

class SessionIceSignalSender final : public QObject {
    Q_OBJECT

  public:
    SessionIceSignalSender(WindowsIdentityStore &identity,
                           SignalingClient &signaling,
                           QObject *parent = nullptr);

    bool begin(const SessionNegotiationContext &context,
               quint32 iceGeneration,
               QString &error);
    bool bindLocalDescription(const QByteArray &signedDescriptionPayload,
                              quint64 descriptionSequence,
                              QString &error);
    void stop();

    bool isActive() const;

  public slots:
    void enqueueCandidate(quint32 iceGeneration,
                          const QByteArray &sdpMid,
                          qint32 sdpMlineIndex,
                          const QByteArray &candidate);

  signals:
    void candidatesSent(int count, quint64 sequence);
    void failed(const QString &code, const QString &message);

  private:
    void scheduleFlush();
    void flushCandidates();
    void fail(const QString &code, const QString &message);

    WindowsIdentityStore &m_identity;
    SignalingClient &m_signaling;
    QTimer m_flushTimer;
    SessionNegotiationContext m_context;
    LocalIceCandidateBuffer m_buffer;
    QByteArray m_signedDescriptionPayload;
    quint32 m_iceGeneration = 0;
    quint64 m_lastSequence = 0;
    bool m_active = false;
};
