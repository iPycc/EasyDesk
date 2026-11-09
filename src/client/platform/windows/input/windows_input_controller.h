#pragma once

#include "features/sessions/input/input_command.h"
#include "features/sessions/model/session_state.h"
#include "input/input_lease.h"
#include "input/send_input_executor.h"

#include <QRect>

#include <condition_variable>
#include <mutex>
#include <thread>

enum class InputApplyStatus {
    Applied,
    SessionUnavailable,
    PermissionDenied,
    LeaseInactive,
    StaleContext,
    ReplayRejected,
    InvalidEvent,
    DesktopUnavailable,
    InjectionRejected,
};

class WindowsInputController final {
  public:
    WindowsInputController();
    ~WindowsInputController();

    WindowsInputController(const WindowsInputController &) = delete;
    WindowsInputController &operator=(const WindowsInputController &) = delete;

    bool startSession(const SessionState &state, const QRect &displayRect,
                      quint64 displayGeneration);
    void stopSession();
    void revokePermission();
    void updateDisplay(const QRect &displayRect, quint64 displayGeneration);

    bool beginLease(const QByteArray &leaseId, quint64 leaseGeneration,
                    int timeoutMilliseconds = 750);
    bool renewLease(const InputCommandContext &context, quint64 leaseGeneration,
                    const QByteArray &challenge);
    QByteArray leaseChallenge() const;
    bool leaseActive() const;

    InputApplyStatus apply(const InputCommand &command);

  private:
    mutable std::mutex m_mutex;
    std::condition_variable m_wakeup;
    std::thread m_watchdog;
    bool m_shutdown = false;
    bool m_sessionActive = false;
    bool m_permissionGranted = false;
    QByteArray m_sessionId;
    quint64 m_sessionEpoch = 0;
    quint64 m_permissionEpoch = 0;
    quint64 m_displayGeneration = 0;
    quint64 m_lastReliableSequence = 0;
    quint64 m_lastPointerSequence = 0;
    InputLease m_lease;
    SendInputExecutor m_executor;

    void watchdogLoop();
    bool contextMatches(const InputCommandContext &context) const;
    InputApplyStatus mapResult(SendInputResult result) const;
};
