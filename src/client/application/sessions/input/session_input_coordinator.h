#pragma once

#include "sessions/input/session_input_lease_client.h"
#include "sessions/negotiation/session_negotiation_context.h"

#include <QObject>
#include <QRect>
#include <QSize>
#include <QTimer>

#include <memory>

class SessionInputLeaseHost;
class SessionNegotiationCoordinator;
class WindowsInputController;

class SessionInputCoordinator final : public QObject {
    Q_OBJECT

  public:
    explicit SessionInputCoordinator(SessionNegotiationCoordinator &session, QObject *parent = nullptr);
    ~SessionInputCoordinator() override;
    bool leaseActive() const;
    bool sendCommand(InputStream stream, const InputEvent &event);

  signals:
    void leaseActiveChanged(bool active);
    void commandApplied(InputStream stream, quint64 sequence);

  private:
    void reset();
    void suspend();
    void refresh();
    void receive(InputStream stream, const QByteArray &message);
    void receiveEnable(const QByteArray &message);
    void receiveLease(const QByteArray &message);
    void receiveCommand(InputStream stream, const QByteArray &message);
    void sendRenewal();
    void setLeaseActive(bool active);
    SessionState stateFor(const SessionNegotiationContext &context) const;

    SessionNegotiationCoordinator &m_session;
    SessionInputLeaseClient m_client;
    std::unique_ptr<WindowsInputController> m_input;
    std::unique_ptr<SessionInputLeaseHost> m_host;
    QTimer m_renewTimer;
    QTimer m_responseTimer;
    QTimer m_hostWatchdog;
    QRect m_localRegion;
    QSize m_remoteSize;
    quint64 m_localGeneration = 0;
    quint64 m_remoteGeneration = 0;
    quint64 m_enableSequence = 0;
    quint64 m_lastRemoteEnableSequence = 0;
    quint64 m_reliableSequence = 0;
    quint64 m_pointerSequence = 0;
    quint64 m_leaseGeneration = 0;
    bool m_localValid = false;
    bool m_remoteValid = false;
    bool m_frameAfterGeometry = false;
    bool m_clientStarted = false;
    bool m_hostStarted = false;
    bool m_blocked = false;
    bool m_leaseActive = false;
};
