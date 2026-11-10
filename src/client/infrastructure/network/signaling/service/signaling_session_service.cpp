#include "network/signaling/service/signaling_session_service.h"

#include "data/settings/identity/device_profile_store.h"
#include "data/settings/identity/trusted_peer_store.h"
#include "identity/windows_identity_store.h"
#include "network/signaling/client/signaling_client.h"
#include "sessions/negotiation/session_negotiation_coordinator.h"
#include "sessions/input/session_input_coordinator.h"
#include "sessions/session_invitation_coordinator.h"
#include "sessions/session_request_coordinator.h"

#include <QRegularExpression>

SignalingSessionService::SignalingSessionService(DeviceProfileStore &profile,
                                                 SignalingClient &signaling,
                                                 SessionRequestCoordinator &requests,
                                                 SessionInvitationCoordinator &invitations,
                                                 SessionNegotiationCoordinator &negotiation,
                                                 SessionInputCoordinator &input,
                                                 WindowsIdentityStore &identity,
                                                 TrustedPeerStore &trustedPeers,
                                                 QObject *parent)
    : SessionService(parent),
      m_profile(profile),
      m_signaling(signaling),
      m_requests(requests),
      m_invitations(invitations),
      m_negotiation(negotiation),
      m_input(input), m_identity(identity), m_trustedPeers(trustedPeers) {
    connect(&m_input, &SessionInputCoordinator::leaseActiveChanged,
            this, [this] { publishInputStatus(); });
    connect(&m_negotiation, &SessionNegotiationCoordinator::framePresented,
            this, [this] { publishInputStatus(); });
    connect(&m_negotiation, &SessionNegotiationCoordinator::firstFramePresented,
            this, [this] { publishInputStatus(); });
    connect(&m_negotiation, &SessionNegotiationCoordinator::remoteCaptureGeometryChanged,
            this, [this] { publishInputStatus(); });
    connect(&m_negotiation, &SessionNegotiationCoordinator::transportStateChanged,
            this, [this] { publishInputStatus(); });
    connect(&m_negotiation, &SessionNegotiationCoordinator::stateChanged,
            this, [this] { publishInputStatus(); });
    connect(&m_signaling, &SignalingClient::profileChanged,
            this, &SessionService::identityChanged);
    connect(&m_signaling, &SignalingClient::assistanceCodeChanged,
            this, &SessionService::identityChanged);
    connect(&m_signaling, &SignalingClient::stateChanged, this, [this] {
        emit identityChanged();
        emit availabilityChanged();
    });
    connect(&m_requests, &SessionRequestCoordinator::stateChanged,
            this, [this] { emit availabilityChanged(); });
    connect(&m_requests, &SessionRequestCoordinator::identityResolved, this,
            [this](const QString &id, const QByteArray &publicKey) {
                const auto status = m_trustedPeers.status(id, publicKey);
                if (status == TrustedPeerStore::Status::Matched) {
                    (void)m_requests.confirmIdentity(true);
                    return;
                }
                if (status != TrustedPeerStore::Status::Unverified && status != TrustedPeerStore::Status::NotAdded) {
                    (void)m_requests.confirmIdentity(false);
                    emit failed(QStringLiteral("设备身份已变化或保存记录损坏，请先移除设备并重新核对"));
                    return;
                }
                emit peerIdentityConfirmationRequested({id, TrustedPeerStore::fingerprint(m_identity.publicKey()),
                                                        TrustedPeerStore::fingerprint(publicKey),
                                                        status == TrustedPeerStore::Status::Unverified});
            });
    connect(&m_requests, &SessionRequestCoordinator::requestDelivered,
            this, &SessionService::requestDelivered);
    connect(&m_requests, &SessionRequestCoordinator::sessionAccepted,
            this, &SessionService::sessionAccepted);
    connect(&m_requests, &SessionRequestCoordinator::requestCancelled,
            this, &SessionService::sessionCancelled);
    connect(&m_requests, &SessionRequestCoordinator::sessionRejected,
            this, [this](quint8 reason) {
                emit sessionCancelled();
                emit failed(reason == 1
                                ? QStringLiteral("对方拒绝了远程请求")
                                : QStringLiteral("对方当前无法接受远程请求"));
            });
    connect(&m_requests, &SessionRequestCoordinator::failed,
            this, [this](const QString &, const QString &message) {
                emit sessionCancelled();
                emit failed(message);
            });
    connect(&m_invitations, &SessionInvitationCoordinator::stateChanged,
            this, [this] { emit availabilityChanged(); });
    connect(&m_invitations, &SessionInvitationCoordinator::invitationReceived,
            this, [this](const IncomingSessionInvitation &invitation) {
                if (m_requests.state() != SessionRequestCoordinator::State::Idle ||
                    m_negotiation.state() != SessionNegotiationCoordinator::State::Idle ||
                    !invitation.requesterIsController) {
                    (void)m_invitations.reject();
                    return;
                }
                const auto publicKey = m_invitations.requesterPublicKey();
                const auto status = m_trustedPeers.status(invitation.fromDeviceId, publicKey);
                if (status != TrustedPeerStore::Status::Matched && status != TrustedPeerStore::Status::Unverified &&
                    status != TrustedPeerStore::Status::NotAdded) {
                    (void)m_invitations.reject();
                    emit failed(QStringLiteral("请求方身份已变化或保存记录损坏"));
                    return;
                }
                emit incomingSessionRequested({invitation.fromDeviceId, invitation.requestedMode,
                    TrustedPeerStore::fingerprint(m_identity.publicKey()), TrustedPeerStore::fingerprint(publicKey),
                    status == TrustedPeerStore::Status::Unverified, status == TrustedPeerStore::Status::Matched,
                    invitation.requestsLocalFileReceive});
            });
    connect(&m_invitations, &SessionInvitationCoordinator::consentExpired,
            this, &SessionService::incomingSessionCancelled);
    connect(&m_invitations, &SessionInvitationCoordinator::sessionAccepted,
            this, [this] {
                emit sessionAccepted();
                if (m_negotiation.state() != SessionNegotiationCoordinator::State::Idle)
                    emit incomingSessionAccepted();
            });
    connect(&m_invitations, &SessionInvitationCoordinator::invitationRejected,
            this, &SessionService::incomingSessionCancelled);
    connect(&m_invitations, &SessionInvitationCoordinator::invitationCancelled,
            this, [this](quint8) { emit incomingSessionCancelled(); });
    connect(&m_invitations, &SessionInvitationCoordinator::failed,
            this, [this](const QString &, const QString &) {
                emit incomingSessionCancelled();
            });
    connect(&m_negotiation, &SessionNegotiationCoordinator::stateChanged,
            this, [this] { emit availabilityChanged(); });
    connect(&m_negotiation, &SessionNegotiationCoordinator::transportStateChanged,
            this, [this](SessionTransportState::State) {
                emit availabilityChanged();
            });
    connect(&m_negotiation, &SessionNegotiationCoordinator::firstFramePresented,
            this, [this](quint32, quint32) { emit availabilityChanged(); });
    connect(&m_negotiation, &SessionNegotiationCoordinator::sessionReady,
            this, [this] { emit availabilityChanged(); });
    connect(&m_negotiation, &SessionNegotiationCoordinator::localSessionEnded,
            this, [this](
                      easydesk::client::signaling::SessionEndReason,
                      quint64) {
                emit availabilityChanged();
                emit sessionCancelled();
            });
    connect(&m_negotiation, &SessionNegotiationCoordinator::remoteSessionEnded,
            this, [this](
                      easydesk::client::signaling::SessionEndReason,
                      quint64) {
                emit availabilityChanged();
                emit sessionCancelled();
            });
    connect(&m_negotiation, &SessionNegotiationCoordinator::failed,
            this, [this](const QString &, const QString &message) {
                emit availabilityChanged();
                emit sessionCancelled();
                emit failed(message);
            });
}

LocalIdentity SignalingSessionService::localIdentity() const {
    const DeviceProfile &profile = m_profile.profile();
    return {profile.deviceId, profile.displayName, m_signaling.assistanceCode(),
            TrustedPeerStore::fingerprint(m_identity.publicKey())};
}

bool SignalingSessionService::isAvailable() const {
    return m_requests.isAvailable() &&
           m_invitations.state() == SessionInvitationCoordinator::State::Idle &&
           m_negotiation.state() == SessionNegotiationCoordinator::State::Idle;
}

QString SignalingSessionService::statusText() const {
    switch (m_negotiation.state()) {
    case SessionNegotiationCoordinator::State::AwaitingTurnCredentials:
        return QStringLiteral("正在获取连接配置");
    case SessionNegotiationCoordinator::State::AwaitingLocalOffer:
    case SessionNegotiationCoordinator::State::AwaitingRemoteOffer:
    case SessionNegotiationCoordinator::State::ApplyingRemoteOffer:
    case SessionNegotiationCoordinator::State::AwaitingLocalAnswer:
    case SessionNegotiationCoordinator::State::AwaitingRemoteAnswer:
    case SessionNegotiationCoordinator::State::ApplyingRemoteAnswer:
        return QStringLiteral("正在协商安全连接");
    case SessionNegotiationCoordinator::State::AwaitingIce:
        if (m_negotiation.transportState() == SessionTransportState::State::Disconnected)
            return QStringLiteral("连接已中断");
        if (m_negotiation.transportState() != SessionTransportState::State::Connected)
            return QStringLiteral("正在检查网络路径");
        if (!m_negotiation.isSessionReady())
            return QStringLiteral("正在验证安全会话");
        if (m_negotiation.context().role == SessionRole::Controlled)
            return QStringLiteral("安全通道已连接");
        return m_negotiation.hasPresentedFrame() ? QStringLiteral("远程画面已显示")
                                                   : QStringLiteral("安全通道已连接，等待画面");
    case SessionNegotiationCoordinator::State::Ending:
        return QStringLiteral("正在结束会话");
    case SessionNegotiationCoordinator::State::Idle:
        break;
    }
    switch (m_invitations.state()) {
    case SessionInvitationCoordinator::State::AwaitingConsent:
        return QStringLiteral("等待确认远程请求");
    case SessionInvitationCoordinator::State::AwaitingAcceptanceDelivery:
        return QStringLiteral("正在接受远程请求");
    case SessionInvitationCoordinator::State::AwaitingRejectionDelivery:
        return QStringLiteral("正在拒绝远程请求");
    case SessionInvitationCoordinator::State::Accepted:
        return QStringLiteral("远程会话已授权");
    case SessionInvitationCoordinator::State::Idle:
        break;
    }
    switch (m_requests.state()) {
    case SessionRequestCoordinator::State::ResolvingIdentity:
        return QStringLiteral("正在验证协助码");
    case SessionRequestCoordinator::State::AwaitingIdentity:
        return QStringLiteral("等待核对设备身份");
    case SessionRequestCoordinator::State::AwaitingDelivery:
        return QStringLiteral("正在发送远程请求");
    case SessionRequestCoordinator::State::AwaitingDecision:
        return QStringLiteral("等待对方确认");
    case SessionRequestCoordinator::State::AwaitingCancellationDelivery:
        return QStringLiteral("正在取消请求");
    case SessionRequestCoordinator::State::Accepted:
        return QStringLiteral("等待媒体通道");
    case SessionRequestCoordinator::State::Idle:
        break;
    }
    switch (m_signaling.state()) {
    case SignalingClient::State::Offline:
        return QStringLiteral("信令未连接");
    case SignalingClient::State::Connecting:
        return QStringLiteral("正在连接信令");
    case SignalingClient::State::Registering:
        return QStringLiteral("正在验证设备");
    case SignalingClient::State::Registered:
        return QStringLiteral("信令在线");
    case SignalingClient::State::RetryWaiting:
        return QStringLiteral("等待重新连接");
    }
    return QStringLiteral("信令未连接");
}

void SignalingSessionService::refreshAssistanceCode() {
    if (!m_signaling.refreshAssistanceCode())
        emit failed(statusText());
}

QString SignalingSessionService::validateConnection(const QString &id,
                                                     const QString &code) const {
    if (!QRegularExpression(QStringLiteral("^[0-9]{9}$")).match(id).hasMatch())
        return QStringLiteral("请输入 9 位数字设备 ID。");
    if (id == m_profile.profile().deviceId)
        return QStringLiteral("不能连接此设备。");
    if (!QRegularExpression(QStringLiteral("^[0-9]{8}$")).match(code).hasMatch())
        return QStringLiteral("请输入 8 位数字临时协助码。");
    if (!m_signaling.isRegistered())
        return statusText();
    if (!isAvailable())
        return QStringLiteral("会话协商尚未就绪");
    return {};
}

void SignalingSessionService::start(const QString &deviceId,
                                    const QString &assistanceCode,
                                    const SessionOptions &options) {
    if (!m_requests.requestSession(deviceId, assistanceCode, options.mode, options.requestPeerFileReceive)) {
        emit sessionCancelled();
        emit failed(QStringLiteral("无法发送远程请求"));
        return;
    }
    m_options = options;
    emit optionsChanged(m_options);
    emit availabilityChanged();
}

void SignalingSessionService::stop() {
    if (m_negotiation.state() != SessionNegotiationCoordinator::State::Idle) {
        if (!m_negotiation.endSession(
                easydesk::client::signaling::SessionEndReason::UserStopped)) {
            emit failed(QStringLiteral("无法结束当前会话"));
        }
        return;
    }
    (void)m_requests.cancel();
}

SessionTelemetry SignalingSessionService::telemetry() const {
    return m_telemetry;
}

SessionOptions SignalingSessionService::options() const {
    return m_options;
}

void SignalingSessionService::setFrameRate(int frameRate) {
    if (frameRate != 15 && frameRate != 30 && frameRate != 60)
        return;
    m_options.frameRate = frameRate;
    emit optionsChanged(m_options);
}

void SignalingSessionService::setQuality(const QString &quality) {
    if (quality != QStringLiteral("清晰") && quality != QStringLiteral("流畅") &&
        quality != QStringLiteral("节流")) {
        return;
    }
    m_options.quality = quality;
    emit optionsChanged(m_options);
}

bool SignalingSessionService::attachVideoTarget(quintptr nativeWindow) {
    QString error;
    if (m_negotiation.attachVideoRenderer(nativeWindow, error))
        return true;
    emit failed(error.isEmpty() ? QStringLiteral("无法初始化远程画面") : error);
    return false;
}

void SignalingSessionService::detachVideoTarget() {
    QString error;
    if (!m_negotiation.detachVideoRenderer(error) && !error.isEmpty())
        emit failed(error);
}

SessionInputStatus SignalingSessionService::inputStatus() const {
    const auto geometry = m_negotiation.remoteCaptureGeometry();
    SessionInputStatus status;
    status.presentedFrame = m_negotiation.presentedFrameSize();
    status.presentedTarget = m_negotiation.presentedTargetSize();
    if (geometry.valid) {
        status.remoteRegion = geometry.size;
        status.displayGeneration = geometry.generation;
    }
    status.ready = m_negotiation.context().role == SessionRole::Controller &&
                   m_negotiation.isSessionReady() && m_negotiation.hasPresentedFrame() &&
                   m_input.leaseActive() && geometry.valid &&
                   !status.presentedFrame.isEmpty() && !status.presentedTarget.isEmpty() &&
                   !status.remoteRegion.isEmpty();
    return status;
}

bool SignalingSessionService::sendInput(InputStream stream, const InputEvent &event) {
    return inputStatus().ready && m_input.sendCommand(stream, event);
}

void SignalingSessionService::releaseInput() {
    if (m_input.leaseActive())
        (void)m_input.sendCommand(InputStream::Reliable, ReleaseAllInput{});
}

void SignalingSessionService::publishInputStatus() {
    const SessionInputStatus current = inputStatus();
    if (current == m_lastInputStatus)
        return;
    m_lastInputStatus = current;
    emit inputStatusChanged(current);
}

bool SignalingSessionService::confirmPeerIdentity(bool confirmed, bool rememberIdentity) {
    if (m_requests.state() != SessionRequestCoordinator::State::AwaitingIdentity)
        return false;
    if (!confirmed)
        return m_requests.confirmIdentity(false);
    const auto id = m_requests.targetDeviceId();
    const auto key = m_requests.targetPublicKey();
    const auto status = m_trustedPeers.status(id, key);
    if (status != TrustedPeerStore::Status::Matched && status != TrustedPeerStore::Status::Unverified &&
        status != TrustedPeerStore::Status::NotAdded) {
        (void)m_requests.confirmIdentity(false);
        emit failed(QStringLiteral("设备身份已变化，无法继续连接"));
        return false;
    }
    QString error;
    if (rememberIdentity && !m_trustedPeers.remember(id, key, error)) {
        (void)m_requests.confirmIdentity(false);
        emit failed(error);
        return false;
    }
    return m_requests.confirmIdentity(true);
}

bool SignalingSessionService::acceptIncomingSession(SessionMode grantedMode, bool rememberIdentity,
                                                    bool grantLocalFileReceive) {
    if (grantedMode != SessionMode::ViewOnly) {
        emit failed(QStringLiteral("远程控制功能尚未就绪，只能允许观看"));
        return false;
    }
    const auto invitation = m_invitations.invitation();
    if (grantLocalFileReceive && !invitation.requestsLocalFileReceive) {
        emit failed(QStringLiteral("对方没有请求文件接收权限"));
        return false;
    }
    const auto key = m_invitations.requesterPublicKey();
    const auto status = m_trustedPeers.status(invitation.fromDeviceId, key);
    if (status != TrustedPeerStore::Status::Matched && status != TrustedPeerStore::Status::Unverified &&
        status != TrustedPeerStore::Status::NotAdded) {
        emit failed(QStringLiteral("请求方身份已变化，无法接受"));
        return false;
    }
    QString error;
    if (rememberIdentity && !m_trustedPeers.remember(invitation.fromDeviceId, key, error)) {
        emit failed(error);
        return false;
    }
    if (m_invitations.accept(grantedMode, grantLocalFileReceive))
        return true;
    emit failed(QStringLiteral("无法接受远程协助请求"));
    return false;
}

bool SignalingSessionService::rejectIncomingSession() {
    if (m_invitations.reject())
        return true;
    emit failed(QStringLiteral("无法拒绝远程协助请求"));
    return false;
}
