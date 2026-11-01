#pragma once

#include "sessions/negotiation/ice/session_ice_description_binding.h"
#include "sessions/negotiation/session_negotiation_context.h"

#include <QObject>
#include <QVector>

class MediaRuntimeLibrary;
class MediaSession;

class SessionIceSignalReceiver final : public QObject {
    Q_OBJECT

  public:
    SessionIceSignalReceiver(MediaRuntimeLibrary &media,
                             QObject *parent = nullptr);

    bool begin(const SessionNegotiationContext &context,
               MediaSession &session,
               quint32 iceGeneration,
               QString &error);
    bool bindRemoteDescription(const QByteArray &signedDescriptionPayload,
                               quint64 descriptionSequence,
                               const QByteArray &sdp,
                               QString &error);
    bool markRemoteDescriptionApplied(quint32 iceGeneration,
                                      QString &error);
    void stop();

    bool isActive() const;
    void handleCandidates(const QByteArray &message);

  signals:
    void candidateBatchApplied(int count, quint64 sequence);
    void failed(const QString &code, const QString &message);

  private:
    struct PendingBatch {
        QVector<easydesk::client::signaling::SessionIceCandidate> candidates;
        quint64 sequence = 0;
    };

    bool applyBatch(const PendingBatch &batch, QString &error);
    bool isDuplicate(
        const easydesk::client::signaling::SessionIceCandidate &candidate) const;
    void fail(const QString &code, const QString &message);

    MediaRuntimeLibrary &m_media;
    MediaSession *m_mediaSession = nullptr;
    SessionNegotiationContext m_context;
    SessionIceDescriptionBinding m_descriptionBinding;
    QVector<PendingBatch> m_pending;
    QVector<easydesk::client::signaling::SessionIceCandidate> m_seen;
    QByteArray m_signedDescriptionPayload;
    quint32 m_iceGeneration = 0;
    quint64 m_descriptionSequence = 0;
    quint64 m_lastSequence = 0;
    bool m_remoteDescriptionApplied = false;
    bool m_active = false;
};
