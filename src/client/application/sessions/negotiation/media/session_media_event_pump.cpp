#include "sessions/negotiation/media/session_media_event_pump.h"

#include "easydesk_media_abi.h"
#include "media/webrtc/media_session.h"

namespace {

constexpr int kPollIntervalMilliseconds = 10;
constexpr int kMaximumEventsPerTick = 32;

bool validSdpType(qint32 type) {
    return type == EASYDESK_MEDIA_SDP_OFFER ||
           type == EASYDESK_MEDIA_SDP_ANSWER;
}

}  // namespace

SessionMediaEventPump::SessionMediaEventPump(QObject *parent)
    : QObject(parent) {
    m_timer.setInterval(kPollIntervalMilliseconds);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout,
            this, &SessionMediaEventPump::drainEvents);
}

bool SessionMediaEventPump::start(MediaSession &session) {
    if (m_session != nullptr || !session.isActive())
        return false;
    m_session = &session;
    m_lastSequence = 0;
    m_timer.start();
    return true;
}

void SessionMediaEventPump::stop() {
    m_timer.stop();
    m_session = nullptr;
    m_lastSequence = 0;
}

bool SessionMediaEventPump::isRunning() const {
    return m_timer.isActive() && m_session != nullptr;
}

void SessionMediaEventPump::drainEvents() {
    for (int index = 0; index < kMaximumEventsPerTick && m_session; ++index) {
        if (!dispatchEvent())
            return;
    }
}

bool SessionMediaEventPump::dispatchEvent() {
    MediaSessionEvent event;
    if (!m_session->pollEvent(event)) {
        if (!m_session->errorString().isEmpty()) {
            fail(m_session->errorString());
        } else if (!m_session->isActive()) {
            stop();
            emit sessionClosed();
        }
        return false;
    }
    if (event.sequence != m_lastSequence + 1) {
        fail(QStringLiteral("Media event stream is out of sequence."));
        return false;
    }
    m_lastSequence = event.sequence;

    switch (event.type) {
    case EASYDESK_MEDIA_EVENT_LOCAL_DESCRIPTION:
        if (event.iceGeneration == 0 || !validSdpType(event.code) ||
            !event.payload.startsWith("v=0")) {
            fail(QStringLiteral("Local media description event is invalid."));
            return false;
        }
        emit localDescription(event.iceGeneration,
                              static_cast<quint32>(event.code),
                              event.payload);
        return true;
    case EASYDESK_MEDIA_EVENT_LOCAL_CANDIDATE:
        if (event.iceGeneration == 0 || event.label.isEmpty() ||
            event.sdpMlineIndex < 0 || !event.payload.startsWith("candidate:")) {
            fail(QStringLiteral("Local ICE candidate event is invalid."));
            return false;
        }
        emit localCandidate(event.iceGeneration,
                            event.label,
                            event.sdpMlineIndex,
                            event.payload);
        return true;
    case EASYDESK_MEDIA_EVENT_ICE_GATHERING_COMPLETE:
        if (event.iceGeneration == 0) {
            fail(QStringLiteral("ICE gathering event is invalid."));
            return false;
        }
        emit gatheringComplete(event.iceGeneration);
        return true;
    case EASYDESK_MEDIA_EVENT_CONNECTION_STATE:
        if (event.code < EASYDESK_MEDIA_CONNECTION_NEW ||
            event.code > EASYDESK_MEDIA_CONNECTION_CLOSED) {
            fail(QStringLiteral("Media connection state is invalid."));
            return false;
        }
        emit connectionStateChanged(event.code);
        return true;
    case EASYDESK_MEDIA_EVENT_ICE_CONNECTION_STATE:
        if (event.iceGeneration == 0 || event.code < EASYDESK_MEDIA_ICE_NEW ||
            event.code > EASYDESK_MEDIA_ICE_CLOSED) {
            fail(QStringLiteral("ICE connection state is invalid."));
            return false;
        }
        emit iceConnectionStateChanged(event.iceGeneration, event.code);
        return true;
    case EASYDESK_MEDIA_EVENT_REMOTE_DESCRIPTION_APPLIED:
        if (event.iceGeneration == 0 || !validSdpType(event.code)) {
            fail(QStringLiteral("Remote media description event is invalid."));
            return false;
        }
        emit remoteDescriptionApplied(event.iceGeneration,
                                      static_cast<quint32>(event.code));
        return true;
    case EASYDESK_MEDIA_EVENT_PERMISSION_CHANGED:
        if (event.payload.size() != static_cast<int>(sizeof(quint64))) {
            fail(QStringLiteral("Media permission event is invalid."));
            return false;
        }
        emit permissionChanged();
        return true;
    case EASYDESK_MEDIA_EVENT_DATA_CHANNEL:
        if (event.payload.isEmpty() || event.iceGeneration == 0 ||
            event.code < EASYDESK_MEDIA_DATA_CHANNEL_OPEN ||
            event.code > EASYDESK_MEDIA_DATA_CHANNEL_CLOSED) {
            fail(QStringLiteral("Media data channel event is invalid."));
            return false;
        }
        if (event.code == EASYDESK_MEDIA_DATA_CHANNEL_OPEN)
            emit dataChannelOpened(event.payload);
        else if (event.code == EASYDESK_MEDIA_DATA_CHANNEL_CLOSED)
            emit dataChannelClosed(event.payload);
        return true;
    case EASYDESK_MEDIA_EVENT_ERROR:
        fail(event.payload.isEmpty()
                 ? QStringLiteral("The media runtime reported an error.")
                 : QString::fromUtf8(event.payload));
        return false;
    default:
        fail(QStringLiteral("The media runtime returned an unknown event."));
        return false;
    }
}

void SessionMediaEventPump::fail(const QString &message) {
    stop();
    emit failed(message);
}
