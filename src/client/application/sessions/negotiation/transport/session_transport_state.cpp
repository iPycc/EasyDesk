#include "sessions/negotiation/transport/session_transport_state.h"

#include "easydesk_media_abi.h"

SessionTransportState::SessionTransportState(QObject *parent)
    : QObject(parent) {}

SessionTransportState::State SessionTransportState::state() const {
    return m_state;
}

quint32 SessionTransportState::iceGeneration() const {
    return m_iceGeneration;
}

qint32 SessionTransportState::peerConnectionState() const {
    return m_peerConnectionState;
}

qint32 SessionTransportState::iceConnectionState() const {
    return m_iceConnectionState;
}

bool SessionTransportState::start(quint32 iceGeneration) {
    if (m_active || iceGeneration == 0)
        return false;
    m_active = true;
    m_iceGeneration = iceGeneration;
    m_peerConnectionState = EASYDESK_MEDIA_CONNECTION_NEW;
    m_iceConnectionState = EASYDESK_MEDIA_ICE_NEW;
    setState(State::Connecting);
    return true;
}

void SessionTransportState::stop() {
    m_active = false;
    m_iceGeneration = 0;
    m_peerConnectionState = EASYDESK_MEDIA_CONNECTION_NEW;
    m_iceConnectionState = EASYDESK_MEDIA_ICE_NEW;
    setState(State::Idle);
}

void SessionTransportState::updatePeerConnectionState(qint32 state) {
    if (!m_active)
        return;
    if (state < EASYDESK_MEDIA_CONNECTION_NEW ||
        state > EASYDESK_MEDIA_CONNECTION_CLOSED) {
        reject(QStringLiteral("Peer connection state is invalid."));
        return;
    }
    m_peerConnectionState = state;
    evaluate();
}

void SessionTransportState::updateIceConnectionState(quint32 iceGeneration,
                                                     qint32 state) {
    if (!m_active)
        return;
    if (iceGeneration != m_iceGeneration) {
        reject(QStringLiteral("ICE connection generation is invalid."));
        return;
    }
    if (state < EASYDESK_MEDIA_ICE_NEW ||
        state > EASYDESK_MEDIA_ICE_CLOSED) {
        reject(QStringLiteral("ICE connection state is invalid."));
        return;
    }
    m_iceConnectionState = state;
    evaluate();
}

void SessionTransportState::evaluate() {
    if (m_peerConnectionState == EASYDESK_MEDIA_CONNECTION_FAILED ||
        m_iceConnectionState == EASYDESK_MEDIA_ICE_FAILED) {
        setState(State::Failed);
        emit failed(QStringLiteral("E_MEDIA_CONNECTION_FAILED"),
                    QStringLiteral("The WebRTC transport failed."));
        return;
    }
    if (m_peerConnectionState == EASYDESK_MEDIA_CONNECTION_CLOSED ||
        m_iceConnectionState == EASYDESK_MEDIA_ICE_CLOSED) {
        setState(State::Closed);
        emit failed(QStringLiteral("E_MEDIA_CONNECTION_CLOSED"),
                    QStringLiteral("The WebRTC transport closed."));
        return;
    }
    if (m_peerConnectionState == EASYDESK_MEDIA_CONNECTION_DISCONNECTED ||
        m_iceConnectionState == EASYDESK_MEDIA_ICE_DISCONNECTED) {
        setState(State::Disconnected);
        return;
    }
    if (m_peerConnectionState == EASYDESK_MEDIA_CONNECTION_CONNECTED &&
        (m_iceConnectionState == EASYDESK_MEDIA_ICE_CONNECTED ||
         m_iceConnectionState == EASYDESK_MEDIA_ICE_COMPLETED)) {
        const bool becameConnected = m_state != State::Connected;
        setState(State::Connected);
        if (becameConnected)
            emit connected();
        return;
    }
    setState(State::Connecting);
}

void SessionTransportState::setState(State state) {
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}

void SessionTransportState::reject(const QString &message) {
    m_active = false;
    setState(State::Failed);
    emit failed(QStringLiteral("E_MEDIA_CONNECTION_EVENT"), message);
}
