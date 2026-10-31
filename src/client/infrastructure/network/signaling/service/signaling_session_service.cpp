#include "network/signaling/service/signaling_session_service.h"

#include "data/settings/identity/device_profile_store.h"
#include "network/signaling/client/signaling_client.h"
#include "sessions/session_request_coordinator.h"

#include <QRegularExpression>

SignalingSessionService::SignalingSessionService(DeviceProfileStore &profile,
                                                 SignalingClient &signaling,
                                                 SessionRequestCoordinator &requests,
                                                 QObject *parent)
    : SessionService(parent),
      m_profile(profile),
      m_signaling(signaling),
      m_requests(requests) {
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
    connect(&m_requests, &SessionRequestCoordinator::requestDelivered,
            this, &SessionService::requestDelivered);
    connect(&m_requests, &SessionRequestCoordinator::sessionAccepted,
            this, &SessionService::sessionAccepted);
    connect(&m_requests, &SessionRequestCoordinator::requestCancelled,
            this, &SessionService::sessionCancelled);
    connect(&m_requests, &SessionRequestCoordinator::sessionRejected,
            this, [this](quint8 reason) {
                emit failed(reason == 1
                                ? QStringLiteral("对方拒绝了远程请求")
                                : QStringLiteral("对方当前无法接受远程请求"));
            });
    connect(&m_requests, &SessionRequestCoordinator::failed,
            this, [this](const QString &, const QString &message) {
                emit failed(message);
            });
}

LocalIdentity SignalingSessionService::localIdentity() const {
    const DeviceProfile &profile = m_profile.profile();
    return {profile.deviceId, profile.displayName, m_signaling.assistanceCode()};
}

bool SignalingSessionService::isAvailable() const {
    return false;
}

QString SignalingSessionService::statusText() const {
    switch (m_requests.state()) {
    case SessionRequestCoordinator::State::ResolvingIdentity:
        return QStringLiteral("正在验证协助码");
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
    if (!m_requests.requestSession(deviceId, assistanceCode, options.mode)) {
        emit failed(QStringLiteral("无法发送远程请求"));
        return;
    }
    m_options = options;
    emit optionsChanged(m_options);
    emit availabilityChanged();
}

void SignalingSessionService::stop() {
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
