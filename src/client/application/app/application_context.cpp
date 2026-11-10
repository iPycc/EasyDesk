#include "app/application_context.h"
#include "features/sessions/model/session_state.h"
#include "data/local/local_transfer_service.h"
#include "data/local/settings_device_service.h"
#include "data/local/windows_system_service.h"
#include "data/settings/identity/device_profile_store.h"
#include "data/settings/identity/trusted_peer_store.h"
#include "identity/windows_identity_store.h"
#include "media/webrtc/media_runtime_library.h"
#include "network/signaling/client/signaling_client.h"
#include "network/signaling/service/signaling_session_service.h"
#include "sessions/session_invitation_coordinator.h"
#include "sessions/negotiation/session_negotiation_coordinator.h"
#include "sessions/input/session_input_coordinator.h"
#include "sessions/files/session_transfer_offer.h"
#include "sessions/files/session_file_incoming.h"
#include "sessions/files/session_file_outgoing.h"
#include "sessions/session_request_coordinator.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

ApplicationContext::ApplicationContext() {
    qRegisterMetaType<SessionTelemetry>();
    qRegisterMetaType<SessionOptions>();
    qRegisterMetaType<IncomingSessionRequest>();
    qRegisterMetaType<PeerIdentityPrompt>();
    qRegisterMetaType<SessionInputStatus>();
    qRegisterMetaType<SessionState>();
    qRegisterMetaType<Device>();
    QString root = qEnvironmentVariable("EASYDESK_DATA_DIR");
    if (root.isEmpty())
        root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(root);
    const QString settingsPath = root + "/settings.ini";
    const QString legacyPath = root + "/preview.ini";
    if (!QFile::exists(settingsPath) && QFile::exists(legacyPath))
        QFile::copy(legacyPath, settingsPath);
    m_settings = std::make_unique<QSettings>(settingsPath, QSettings::IniFormat);
    m_devices = std::make_unique<SettingsDeviceService>(*m_settings);
    m_trustedPeers = std::make_unique<TrustedPeerStore>(*m_settings, *m_devices);
    m_media = std::make_unique<MediaRuntimeLibrary>();
    const bool mediaReady = m_media->start();
    m_identity = std::make_unique<WindowsIdentityStore>(*m_settings, *m_media);
    const bool identityReady = m_identity->initialize();
    if (!identityReady)
        qWarning().noquote() << "EasyDesk identity initialization failed:"
                             << m_identity->errorString();
    m_profile = std::make_unique<DeviceProfileStore>(*m_settings);
    const bool profileReady = m_profile->initialize();
    if (!profileReady)
        qWarning().noquote() << "EasyDesk device profile initialization failed:"
                             << m_profile->errorString();
    m_signaling = std::make_unique<SignalingClient>(*m_profile, *m_identity);
    m_sessionRequests = std::make_unique<SessionRequestCoordinator>(
        *m_profile, *m_identity, *m_media, *m_signaling);
    m_sessionInvitations = std::make_unique<SessionInvitationCoordinator>(
        *m_profile, *m_identity, *m_media, *m_signaling);
    m_sessionNegotiation = std::make_unique<SessionNegotiationCoordinator>(
        *m_media, *m_identity, *m_signaling);
    m_sessionInput = std::make_unique<SessionInputCoordinator>(*m_sessionNegotiation);
    m_sessionTransferOffer = std::make_unique<SessionTransferOffer>(*m_sessionNegotiation);
    m_sessionFileIncoming = std::make_unique<SessionFileIncoming>(*m_sessionNegotiation, *m_sessionTransferOffer, *m_trustedPeers);
    m_sessionFileOutgoing = std::make_unique<SessionFileOutgoing>(*m_sessionNegotiation, *m_sessionTransferOffer, *m_trustedPeers);
    connect(m_sessionRequests.get(), &SessionRequestCoordinator::sessionAccepted,
            this, [this] {
                (void)m_sessionNegotiation->begin(
                    m_sessionRequests->negotiationContext());
            });
    connect(m_sessionInvitations.get(), &SessionInvitationCoordinator::sessionAccepted,
            this, [this] {
                (void)m_sessionNegotiation->begin(
                    m_sessionInvitations->negotiationContext());
            });
    const auto completeSession = [this] {
        m_sessionRequests->completeSession();
        m_sessionInvitations->completeSession();
    };
    connect(m_sessionNegotiation.get(),
            &SessionNegotiationCoordinator::localSessionEnded,
            this, [completeSession](auto, quint64) { completeSession(); });
    connect(m_sessionNegotiation.get(),
            &SessionNegotiationCoordinator::remoteSessionEnded,
            this, [completeSession](auto, quint64) { completeSession(); });
    connect(m_sessionNegotiation.get(), &SessionNegotiationCoordinator::failed,
            this, [completeSession](const QString &code,
                                    const QString &message) {
                completeSession();
                qWarning().noquote() << "EasyDesk session negotiation failed:"
                                     << code << message;
            });
    m_sessions = std::make_unique<SignalingSessionService>(
        *m_profile, *m_signaling, *m_sessionRequests,
        *m_sessionInvitations,
        *m_sessionNegotiation, *m_sessionInput, *m_identity, *m_trustedPeers);
    m_transfers = std::make_unique<LocalTransferService>();
    m_local = std::make_unique<WindowsSystemService>(*m_settings);
    QString signalingUrl = qEnvironmentVariable("EASYDESK_SIGNAL_URL").trimmed();
    if (signalingUrl.isEmpty())
        signalingUrl = m_settings->value(QStringLiteral("signalAddress")).toString().trimmed();
    if (mediaReady && identityReady && profileReady && !signalingUrl.isEmpty()) {
        const bool allowInsecureLoopback =
            qEnvironmentVariable("EASYDESK_ALLOW_INSECURE_LOOPBACK") == QStringLiteral("1");
        if (!m_signaling->start(QUrl(signalingUrl), allowInsecureLoopback))
            qWarning().noquote() << "EasyDesk signaling configuration is invalid:"
                                 << signalingUrl;
    }
}
ApplicationContext::~ApplicationContext() = default;
DeviceService &ApplicationContext::devices() const {
    return *m_devices;
}
SessionService &ApplicationContext::sessions() const {
    return *m_sessions;
}
TransferService &ApplicationContext::transfers() const {
    return *m_transfers;
}
QSettings &ApplicationContext::settings() const {
    return *m_settings;
}

LocalSystemService &ApplicationContext::localSystem() const {
    return *m_local;
}

MediaRuntimeLibrary &ApplicationContext::mediaRuntime() const {
    return *m_media;
}

WindowsIdentityStore &ApplicationContext::identity() const {
    return *m_identity;
}

DeviceProfileStore &ApplicationContext::deviceProfile() const {
    return *m_profile;
}

TrustedPeerStore &ApplicationContext::trustedPeers() const {
    return *m_trustedPeers;
}

SignalingClient &ApplicationContext::signaling() const {
    return *m_signaling;
}

SessionRequestCoordinator &ApplicationContext::sessionRequests() const {
    return *m_sessionRequests;
}

SessionInvitationCoordinator &ApplicationContext::sessionInvitations() const {
    return *m_sessionInvitations;
}

SessionNegotiationCoordinator &ApplicationContext::sessionNegotiation() const {
    return *m_sessionNegotiation;
}

SessionTransferOffer &ApplicationContext::sessionTransferOffer() const {
    return *m_sessionTransferOffer;
}

SessionFileIncoming &ApplicationContext::sessionFileIncoming() const {
    return *m_sessionFileIncoming;
}

SessionFileOutgoing &ApplicationContext::sessionFileOutgoing() const {
    return *m_sessionFileOutgoing;
}
