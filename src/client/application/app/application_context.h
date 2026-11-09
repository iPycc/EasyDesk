#pragma once
#include <QObject>
#include <memory>
class DeviceService;
class SessionService;
class TransferService;
class QSettings;
class LocalSystemService;
class MediaRuntimeLibrary;
class WindowsIdentityStore;
class DeviceProfileStore;
class SignalingClient;
class SessionRequestCoordinator;
class SessionInvitationCoordinator;
class SessionNegotiationCoordinator;
class SessionInputCoordinator;
class SessionTransferOffer;

class ApplicationContext final : public QObject {
  public:
    ApplicationContext();
    ~ApplicationContext() override;
    DeviceService &devices() const;
    SessionService &sessions() const;
    TransferService &transfers() const;
    QSettings &settings() const;
    LocalSystemService &localSystem() const;
    MediaRuntimeLibrary &mediaRuntime() const;
    WindowsIdentityStore &identity() const;
    DeviceProfileStore &deviceProfile() const;
    SignalingClient &signaling() const;
    SessionRequestCoordinator &sessionRequests() const;
    SessionInvitationCoordinator &sessionInvitations() const;
    SessionNegotiationCoordinator &sessionNegotiation() const;
    SessionTransferOffer &sessionTransferOffer() const;

  private:
    std::unique_ptr<QSettings> m_settings;
    std::unique_ptr<MediaRuntimeLibrary> m_media;
    std::unique_ptr<WindowsIdentityStore> m_identity;
    std::unique_ptr<DeviceProfileStore> m_profile;
    std::unique_ptr<SignalingClient> m_signaling;
    std::unique_ptr<SessionRequestCoordinator> m_sessionRequests;
    std::unique_ptr<SessionInvitationCoordinator> m_sessionInvitations;
    std::unique_ptr<SessionNegotiationCoordinator> m_sessionNegotiation;
    std::unique_ptr<SessionInputCoordinator> m_sessionInput;
    std::unique_ptr<SessionTransferOffer> m_sessionTransferOffer;
    std::unique_ptr<DeviceService> m_devices;
    std::unique_ptr<SessionService> m_sessions;
    std::unique_ptr<TransferService> m_transfers;
    std::unique_ptr<LocalSystemService> m_local;
};
