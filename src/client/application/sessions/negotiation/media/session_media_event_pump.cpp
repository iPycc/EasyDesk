#include "sessions/negotiation/media/session_media_event_pump.h"

#include "easydesk_media_abi.h"
#include "media/webrtc/media_session.h"

#include <cstring>
#include <limits>

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
    m_captureGeometryGeneration = 0;
    m_timer.start();
    return true;
}

void SessionMediaEventPump::stop() {
    m_timer.stop();
    m_session = nullptr;
    m_lastSequence = 0;
    m_captureGeometryGeneration = 0;
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
    case EASYDESK_MEDIA_EVENT_DATA_MESSAGE:
        if (event.iceGeneration == 0 || event.label.isEmpty() || event.payload.isEmpty()) {
            fail(QStringLiteral("Media data channel message is invalid."));
            return false;
        }
        if (m_session->permitsDataChannel(event.label))
            emit dataChannelMessage(event.label, event.payload);
        return true;
    case EASYDESK_MEDIA_EVENT_CAPTURE_GEOMETRY: {
        if (event.iceGeneration == 0 ||
            event.payload.size() != static_cast<int>(sizeof(EasyDeskMediaCaptureGeometry))) {
            fail(QStringLiteral("Capture geometry event is invalid."));
            return false;
        }
        EasyDeskMediaCaptureGeometry geometry{};
        std::memcpy(&geometry, event.payload.constData(), sizeof(geometry));
        const qint64 right = static_cast<qint64>(geometry.x) + geometry.width - 1;
        const qint64 bottom = static_cast<qint64>(geometry.y) + geometry.height - 1;
        if (geometry.size != sizeof(geometry) || geometry.generation <= m_captureGeometryGeneration ||
            geometry.valid > 1 || geometry.reserved[0] != 0 || geometry.reserved[1] != 0 ||
            (geometry.valid == 0 && (geometry.x != 0 || geometry.y != 0 ||
                                     geometry.width != 0 || geometry.height != 0)) ||
            (geometry.valid == 1 &&
             (geometry.width == 0 || geometry.height == 0 ||
              geometry.width > static_cast<quint32>(std::numeric_limits<int>::max()) ||
              geometry.height > static_cast<quint32>(std::numeric_limits<int>::max()) ||
              right > std::numeric_limits<int>::max() || bottom > std::numeric_limits<int>::max()))) {
            fail(QStringLiteral("Capture geometry fields are invalid."));
            return false;
        }
        m_captureGeometryGeneration = geometry.generation;
        const QRect region = geometry.valid ? QRect(geometry.x, geometry.y,
                                                    static_cast<int>(geometry.width),
                                                    static_cast<int>(geometry.height)) : QRect();
        emit captureGeometryChanged(region, geometry.generation, geometry.valid != 0);
        return true;
    }
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
