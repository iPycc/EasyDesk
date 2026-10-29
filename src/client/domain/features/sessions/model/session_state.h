#pragma once

#include "features/sessions/model/session_access.h"

#include <QString>

enum class SessionRole : quint8 {
    Controller,
    Controlled,
};

enum class SessionPhase : quint8 {
    Idle,
    AwaitingApproval,
    Negotiating,
    Connecting,
    Active,
    Recovering,
    Stopping,
    Ended,
    Failed,
};

struct SessionReadiness {
    bool peerIdentityVerified = false;
    bool dtlsConnected = false;
    bool sessionReady = false;
    bool firstFramePresented = false;
    bool inputChannelReady = false;
    bool inputLeaseActive = false;
};

struct SessionState {
    QString sessionId;
    QString peerDeviceId;
    quint64 sessionEpoch = 0;
    quint64 permissionEpoch = 0;
    SessionRole role = SessionRole::Controller;
    SessionPhase phase = SessionPhase::Idle;
    SessionAccess access;
    SessionReadiness readiness;
    QString failureReason;

    bool transitionTo(SessionPhase next);
    void fail(const QString &reason);
    void reset();
    bool authenticated() const;
    bool canPresentDesktop() const;
    bool canUseInput() const;
};

bool isSessionTransitionAllowed(SessionPhase from, SessionPhase to);

Q_DECLARE_METATYPE(SessionRole)
Q_DECLARE_METATYPE(SessionPhase)
Q_DECLARE_METATYPE(SessionReadiness)
Q_DECLARE_METATYPE(SessionState)
