#include "input/windows_input_controller.h"

WindowsInputController::WindowsInputController()
    : m_watchdog([this] { watchdogLoop(); }) {}

WindowsInputController::~WindowsInputController() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_executor.releaseAll();
        m_lease.reset();
        m_shutdown = true;
    }
    m_wakeup.notify_all();
    if (m_watchdog.joinable())
        m_watchdog.join();
}

bool WindowsInputController::startSession(const SessionState &state, const QRect &displayRect,
                                          quint64 displayGeneration) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_executor.releaseAll();
    m_lease.reset();
    m_sessionActive = false;
    m_permissionGranted = false;
    m_sessionId.clear();
    m_sessionEpoch = 0;
    m_permissionEpoch = 0;
    m_displayGeneration = 0;
    m_lastReliableSequence = 0;
    m_lastPointerSequence = 0;
    if (state.role != SessionRole::Controlled || state.phase != SessionPhase::Active ||
        !state.authenticated() || !state.access.allows(SessionCapability::ControlInput) ||
        state.sessionId.size() != 16 || state.sessionEpoch == 0 || state.permissionEpoch == 0 ||
        displayGeneration == 0 || displayRect.isEmpty())
        return false;

    m_sessionId = state.sessionId;
    m_sessionEpoch = state.sessionEpoch;
    m_permissionEpoch = state.permissionEpoch;
    m_displayGeneration = displayGeneration;
    m_executor.setDisplayRect(displayRect);
    m_sessionActive = true;
    m_permissionGranted = true;
    return true;
}

void WindowsInputController::stopSession() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_executor.releaseAll();
    m_lease.reset();
    m_sessionActive = false;
    m_permissionGranted = false;
    m_sessionId.clear();
    m_sessionEpoch = 0;
    m_permissionEpoch = 0;
    m_displayGeneration = 0;
    m_lastReliableSequence = 0;
    m_lastPointerSequence = 0;
    m_wakeup.notify_all();
}

void WindowsInputController::revokePermission() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_executor.releaseAll();
    m_lease.invalidate();
    m_permissionGranted = false;
    m_wakeup.notify_all();
}

void WindowsInputController::updateDisplay(const QRect &displayRect,
                                           quint64 displayGeneration) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sessionActive || displayGeneration == 0 || displayRect.isEmpty())
        return;
    if (displayGeneration != m_displayGeneration) {
        m_executor.releaseAll();
        m_lease.invalidate();
        m_lastReliableSequence = 0;
        m_lastPointerSequence = 0;
    }
    m_displayGeneration = displayGeneration;
    m_executor.setDisplayRect(displayRect);
    m_wakeup.notify_all();
}

bool WindowsInputController::beginLease(const QByteArray &leaseId, quint64 leaseGeneration,
                                        int timeoutMilliseconds) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sessionActive || !m_permissionGranted)
        return false;
    const bool started = m_lease.begin(leaseId, leaseGeneration, timeoutMilliseconds);
    if (started) {
        m_executor.releaseAll();
        m_wakeup.notify_all();
    }
    return started;
}

bool WindowsInputController::renewLease(const InputCommandContext &context,
                                        quint64 leaseGeneration,
                                        const QByteArray &challenge) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sessionActive || !m_permissionGranted || context.stream != InputStream::Reliable ||
        !contextMatches(context) || context.sequence <= m_lastReliableSequence)
        return false;
    const auto now = InputLease::Clock::now();
    if (m_lease.expireIfNeeded(now)) {
        m_executor.releaseAll();
        m_wakeup.notify_all();
        return false;
    }
    const bool renewed = m_lease.renew(context.leaseId, leaseGeneration, context.sequence,
                                       challenge, now);
    if (renewed) {
        m_lastReliableSequence = context.sequence;
        m_wakeup.notify_all();
    }
    return renewed;
}

QByteArray WindowsInputController::leaseChallenge() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lease.challenge();
}

bool WindowsInputController::leaseActive() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lease.active(InputLease::Clock::now());
}

InputApplyStatus WindowsInputController::apply(const InputCommand &command) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sessionActive)
        return InputApplyStatus::SessionUnavailable;
    if (!m_permissionGranted)
        return InputApplyStatus::PermissionDenied;
    if (!contextMatches(command.context))
        return InputApplyStatus::StaleContext;

    quint64 &lastSequence = command.context.stream == InputStream::Pointer
                                ? m_lastPointerSequence
                                : m_lastReliableSequence;
    if (command.context.sequence == 0 || command.context.sequence <= lastSequence)
        return InputApplyStatus::ReplayRejected;

    if (std::holds_alternative<ReleaseAllInput>(command.event)) {
        lastSequence = command.context.sequence;
        m_executor.releaseAll();
        return InputApplyStatus::Applied;
    }

    const auto now = InputLease::Clock::now();
    if (m_lease.expireIfNeeded(now)) {
        m_executor.releaseAll();
        m_wakeup.notify_all();
    }
    if (!m_lease.matches(command.context.leaseId) || !m_lease.active(now))
        return InputApplyStatus::LeaseInactive;

    lastSequence = command.context.sequence;
    return mapResult(m_executor.apply(command.event, command.context.stream));
}

void WindowsInputController::watchdogLoop() {
    std::unique_lock<std::mutex> lock(m_mutex);
    while (!m_shutdown) {
        m_wakeup.wait_for(lock, std::chrono::milliseconds(25));
        if (m_shutdown)
            break;
        if (m_lease.expireIfNeeded(InputLease::Clock::now()))
            m_executor.releaseAll();
    }
}

bool WindowsInputController::contextMatches(const InputCommandContext &context) const {
    return context.sessionId == m_sessionId && context.sessionEpoch == m_sessionEpoch &&
           context.permissionEpoch == m_permissionEpoch &&
           context.displayGeneration == m_displayGeneration;
}

InputApplyStatus WindowsInputController::mapResult(SendInputResult result) const {
    switch (result) {
    case SendInputResult::Applied:
        return InputApplyStatus::Applied;
    case SendInputResult::InvalidEvent:
        return InputApplyStatus::InvalidEvent;
    case SendInputResult::DesktopUnavailable:
        return InputApplyStatus::DesktopUnavailable;
    case SendInputResult::InjectionRejected:
        return InputApplyStatus::InjectionRejected;
    }
    return InputApplyStatus::InjectionRejected;
}
