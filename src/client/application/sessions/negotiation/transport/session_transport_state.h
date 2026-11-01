#pragma once

#include <QObject>
#include <QString>

class SessionTransportState final : public QObject {
    Q_OBJECT

  public:
    enum class State {
        Idle,
        Connecting,
        Connected,
        Disconnected,
        Failed,
        Closed,
    };
    Q_ENUM(State)

    explicit SessionTransportState(QObject *parent = nullptr);

    State state() const;
    quint32 iceGeneration() const;
    qint32 peerConnectionState() const;
    qint32 iceConnectionState() const;

    bool start(quint32 iceGeneration);
    void stop();

  public slots:
    void updatePeerConnectionState(qint32 state);
    void updateIceConnectionState(quint32 iceGeneration, qint32 state);

  signals:
    void stateChanged(SessionTransportState::State state);
    void connected();
    void failed(const QString &code, const QString &message);

  private:
    void evaluate();
    void setState(State state);
    void reject(const QString &message);

    State m_state = State::Idle;
    quint32 m_iceGeneration = 0;
    qint32 m_peerConnectionState = 0;
    qint32 m_iceConnectionState = 0;
    bool m_active = false;
};
