#include "sessions/input/session_input_coordinator.h"

#include "input/windows_input_controller.h"
#include "network/input/protocol/lease/input_enable_codec.h"
#include "network/input/protocol/lease/input_lease_codec.h"
#include "network/input/protocol/input_codec.h"
#include "sessions/input/session_input_lease_host.h"
#include "sessions/negotiation/session_negotiation_coordinator.h"

#include <limits>

namespace {
constexpr int kRenewIntervalMilliseconds = 100;
constexpr int kResponseTimeoutMilliseconds = 750;
constexpr int kHostCheckMilliseconds = 50;
}

SessionInputCoordinator::SessionInputCoordinator(SessionNegotiationCoordinator &session, QObject *parent)
    : QObject(parent), m_session(session) {
    m_renewTimer.setSingleShot(true);
    m_renewTimer.setTimerType(Qt::PreciseTimer);
    m_responseTimer.setSingleShot(true);
    m_hostWatchdog.setInterval(kHostCheckMilliseconds);
    connect(&m_renewTimer, &QTimer::timeout, this, &SessionInputCoordinator::sendRenewal);
    connect(&m_responseTimer, &QTimer::timeout, this, &SessionInputCoordinator::suspend);
    connect(&m_hostWatchdog, &QTimer::timeout, this, [this] {
        if (m_leaseActive && (!m_input || !m_input->leaseActive()))
            suspend();
    });
    connect(&m_session, &SessionNegotiationCoordinator::stateChanged, this, [this](auto state) {
        if (state == SessionNegotiationCoordinator::State::Idle)
            reset();
        else if (state == SessionNegotiationCoordinator::State::Ending)
            suspend();
        else
            refresh();
    });
    connect(&m_session, &SessionNegotiationCoordinator::transportStateChanged, this, [this](auto state) {
        if (state != SessionTransportState::State::Connected && (m_clientStarted || m_hostStarted))
            suspend();
        else
            refresh();
    });
    connect(&m_session, &SessionNegotiationCoordinator::inputAvailabilityChanged, this, [this] {
        if (!m_session.isInputChannelOpen(InputStream::Reliable) && (m_clientStarted || m_hostStarted))
            suspend();
        else
            refresh();
    });
    connect(&m_session, &SessionNegotiationCoordinator::sessionReady, this, &SessionInputCoordinator::refresh);
    connect(&m_session, &SessionNegotiationCoordinator::firstFramePresented, this,
            [this](quint32, quint32) { refresh(); });
    connect(&m_session, &SessionNegotiationCoordinator::framePresented, this, [this] {
        if (m_remoteValid)
            m_frameAfterGeometry = true;
        refresh();
    });
    connect(&m_session, &SessionNegotiationCoordinator::captureGeometryChanged, this,
            [this](const QRect &region, quint64 generation, bool valid) {
                if (m_hostStarted && (generation != m_localGeneration || !valid))
                    suspend();
                m_localRegion = region;
                m_localGeneration = generation;
                m_localValid = valid;
            });
    connect(&m_session, &SessionNegotiationCoordinator::remoteCaptureGeometryChanged, this,
            [this](const QSize &size, quint64 generation, bool valid) {
                if (m_clientStarted && (generation != m_remoteGeneration || !valid))
                    suspend();
                m_remoteSize = size;
                m_remoteGeneration = generation;
                m_remoteValid = valid;
                m_frameAfterGeometry = false;
            });
    connect(&m_session, &SessionNegotiationCoordinator::inputMessageReceived,
            this, &SessionInputCoordinator::receive);
}

SessionInputCoordinator::~SessionInputCoordinator() { reset(); }

bool SessionInputCoordinator::leaseActive() const { return m_leaseActive; }

bool SessionInputCoordinator::sendCommand(InputStream stream, const InputEvent &event) {
    if (!m_clientStarted || !m_leaseActive || !m_remoteValid || !m_session.hasPresentedFrame() ||
        !m_session.isSessionReady() || !m_session.isInputChannelOpen(stream) ||
        (stream != InputStream::Reliable && stream != InputStream::Pointer))
        return false;
    quint64 &sequence = stream == InputStream::Reliable ? m_reliableSequence : m_pointerSequence;
    if (sequence == std::numeric_limits<quint64>::max()) {
        suspend();
        return false;
    }
    InputCommand command;
    if (!m_client.commandContext(stream, sequence + 1, command.context))
        return false;
    command.event = event;
    QByteArray message;
    QString error;
    if (!easydesk::client::input::encodeInputCommand(command, message, error))
        return false;
    ++sequence;
    const auto result = m_session.sendInputMessage(stream, message);
    if (result != SessionInputTransport::SendResult::Sent &&
        (stream == InputStream::Reliable || result != SessionInputTransport::SendResult::WouldBlock))
        suspend();
    return result == SessionInputTransport::SendResult::Sent;
}

void SessionInputCoordinator::reset() {
    suspend();
    m_localRegion = {};
    m_remoteSize = {};
    m_localGeneration = 0;
    m_remoteGeneration = 0;
    m_enableSequence = 0;
    m_lastRemoteEnableSequence = 0;
    m_reliableSequence = 0;
    m_pointerSequence = 0;
    m_leaseGeneration = 0;
    m_localValid = false;
    m_remoteValid = false;
    m_frameAfterGeometry = false;
    m_blocked = false;
}

void SessionInputCoordinator::suspend() {
    m_renewTimer.stop();
    m_responseTimer.stop();
    m_hostWatchdog.stop();
    m_client.stop();
    if (m_host)
        m_host->stop();
    m_host.reset();
    m_input.reset();
    m_clientStarted = false;
    m_hostStarted = false;
    m_blocked = true;
    setLeaseActive(false);
}

void SessionInputCoordinator::refresh() {
    const auto context = m_session.context();
    if (m_blocked || m_clientStarted || context.role != SessionRole::Controller ||
        !context.capabilities.testFlag(SessionCapability::ControlInput) ||
        !m_session.isSessionReady() || !m_session.hasPresentedFrame() ||
        !m_session.isInputChannelOpen(InputStream::Reliable) || !m_remoteValid ||
        !m_frameAfterGeometry || m_enableSequence == std::numeric_limits<quint64>::max())
        return;
    QString error;
    if (!m_client.start(stateFor(context), m_remoteGeneration, error)) {
        suspend();
        return;
    }
    easydesk::client::input::InputEnableRequest request;
    request.sessionId = context.sessionId;
    request.sessionEpoch = context.sessionEpoch;
    request.permissionEpoch = context.permissionEpoch;
    request.displayGeneration = m_remoteGeneration;
    request.sequence = ++m_enableSequence;
    QByteArray message;
    m_clientStarted = true;
    if (!easydesk::client::input::encodeInputEnableRequest(request, message, error) ||
        m_session.sendInputMessage(InputStream::Reliable, message) != SessionInputTransport::SendResult::Sent) {
        suspend();
        return;
    }
    m_responseTimer.start(kResponseTimeoutMilliseconds);
}

void SessionInputCoordinator::receive(InputStream stream, const QByteArray &message) {
    if (m_blocked || (stream != InputStream::Reliable && stream != InputStream::Pointer))
        return;
    if (!m_session.isSessionReady() || !m_session.isInputChannelOpen(stream) ||
        !m_session.context().capabilities.testFlag(SessionCapability::ControlInput)) {
        if (m_clientStarted || m_hostStarted)
            suspend();
        return;
    }
    if (stream == InputStream::Reliable && easydesk::client::input::isInputEnableRequest(message))
        receiveEnable(message);
    else if (stream == InputStream::Reliable && easydesk::client::input::isInputLeaseMessage(message))
        receiveLease(message);
    else
        receiveCommand(stream, message);
}

void SessionInputCoordinator::receiveEnable(const QByteArray &message) {
    const auto context = m_session.context();
    easydesk::client::input::InputEnableRequest request;
    QString error;
    if (m_hostStarted || context.role != SessionRole::Controlled ||
        !context.capabilities.testFlag(SessionCapability::ControlInput) ||
        !m_session.isSessionReady() || !m_session.isInputChannelOpen(InputStream::Reliable) ||
        !m_localValid || !easydesk::client::input::decodeInputEnableRequest(message, request, error) ||
        request.sessionId != context.sessionId || request.sessionEpoch != context.sessionEpoch ||
        request.permissionEpoch != context.permissionEpoch || request.displayGeneration != m_localGeneration ||
        request.sequence <= m_lastRemoteEnableSequence || m_leaseGeneration == std::numeric_limits<quint64>::max())
        return;
    m_lastRemoteEnableSequence = request.sequence;
    m_input = std::make_unique<WindowsInputController>();
    m_host = std::make_unique<SessionInputLeaseHost>(*m_input);
    QByteArray challenge;
    if (!m_host->start(stateFor(context), m_localRegion, m_localGeneration, ++m_leaseGeneration,
                       challenge, error)) {
        suspend();
        return;
    }
    m_hostStarted = true;
    if (m_session.sendInputMessage(InputStream::Reliable, challenge) != SessionInputTransport::SendResult::Sent) {
        suspend();
        return;
    }
    m_responseTimer.start(kResponseTimeoutMilliseconds);
}

void SessionInputCoordinator::receiveLease(const QByteArray &message) {
    QString error;
    if (m_clientStarted && m_session.context().role == SessionRole::Controller) {
        easydesk::client::input::InputLeaseMessage challenge;
        if (!easydesk::client::input::decodeInputLeaseMessage(message, challenge, error) ||
            !m_client.acceptChallenge(message, error)) {
            suspend();
            return;
        }
        m_responseTimer.stop();
        if (challenge.context.sequence != 0)
            setLeaseActive(true);
        m_renewTimer.start(kRenewIntervalMilliseconds);
    } else if (m_hostStarted && m_host && m_session.context().role == SessionRole::Controlled) {
        QByteArray challenge;
        if (!m_host->renew(message, challenge, error)) {
            suspend();
            return;
        }
        m_responseTimer.stop();
        setLeaseActive(true);
        m_hostWatchdog.start();
        if (m_session.sendInputMessage(InputStream::Reliable, challenge) != SessionInputTransport::SendResult::Sent)
            suspend();
    }
}

void SessionInputCoordinator::receiveCommand(InputStream stream, const QByteArray &message) {
    if (!m_hostStarted || !m_input || !m_leaseActive || !m_localValid ||
        m_session.context().role != SessionRole::Controlled)
        return;
    InputCommand command;
    QString error;
    if (!easydesk::client::input::decodeInputCommand(message, stream, command, error)) {
        suspend();
        return;
    }
    const InputApplyStatus result = m_input->apply(command);
    if (result == InputApplyStatus::Applied) {
        emit commandApplied(stream, command.context.sequence);
    } else if (result == InputApplyStatus::InvalidEvent || result == InputApplyStatus::DesktopUnavailable ||
               result == InputApplyStatus::InjectionRejected || result == InputApplyStatus::SessionUnavailable ||
               result == InputApplyStatus::PermissionDenied) {
        suspend();
    }
}

void SessionInputCoordinator::sendRenewal() {
    if (!m_clientStarted || !m_session.isSessionReady() ||
        m_reliableSequence == std::numeric_limits<quint64>::max()) {
        suspend();
        return;
    }
    QByteArray message;
    QString error;
    if (!m_client.makeRenewal(++m_reliableSequence, message, error) ||
        m_session.sendInputMessage(InputStream::Reliable, message) != SessionInputTransport::SendResult::Sent) {
        suspend();
        return;
    }
    m_responseTimer.start(kResponseTimeoutMilliseconds);
}

void SessionInputCoordinator::setLeaseActive(bool active) {
    if (m_leaseActive == active)
        return;
    m_leaseActive = active;
    emit leaseActiveChanged(active);
}

SessionState SessionInputCoordinator::stateFor(const SessionNegotiationContext &context) const {
    SessionState state;
    state.sessionId = context.sessionId;
    state.sessionEpoch = context.sessionEpoch;
    state.permissionEpoch = context.permissionEpoch;
    state.role = context.role;
    state.phase = SessionPhase::Active;
    state.access.request(context.capabilities);
    state.access.grant(context.capabilities);
    state.readiness.peerIdentityVerified = m_session.isSessionReady();
    state.readiness.dtlsConnected = m_session.isSessionReady();
    state.readiness.sessionReady = m_session.isSessionReady();
    state.readiness.firstFramePresented = m_session.hasPresentedFrame();
    state.readiness.inputChannelReady = m_session.isInputChannelOpen(InputStream::Reliable);
    return state;
}
