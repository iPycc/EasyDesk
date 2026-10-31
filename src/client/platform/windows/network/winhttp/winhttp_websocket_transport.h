#pragma once

#include "network/winhttp/winhttp_websocket_connection.h"

#include <QObject>
#include <QUrl>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

class WinHttpWebSocketTransport final : public QObject {
    Q_OBJECT

  public:
    enum class State {
        Disconnected,
        Connecting,
        Connected,
        Closing,
    };
    Q_ENUM(State)

    explicit WinHttpWebSocketTransport(QObject *parent = nullptr);
    ~WinHttpWebSocketTransport() override;

    State state() const;
    bool connectToServer(const QUrl &endpoint, bool allowInsecureLoopback = false);
    bool sendText(const QByteArray &message);
    void disconnectFromServer();

  signals:
    void stateChanged(WinHttpWebSocketTransport::State state);
    void textReceived(const QByteArray &message);
    void remoteClosed(quint16 status, const QString &reason);
    void errorOccurred(const QString &error);

  private:
    void receiveLoop(QUrl endpoint, bool allowInsecureLoopback);
    void sendLoop();
    void clearSendQueue();
    void postState(State state);
    void postError(QString error);

    WinHttpWebSocketConnection m_connection;
    std::atomic<State> m_state{State::Disconnected};
    std::atomic_bool m_stopRequested{false};
    std::thread m_receiveThread;
    std::thread m_sendThread;
    std::mutex m_sendQueueMutex;
    std::condition_variable m_sendQueueChanged;
    std::deque<QByteArray> m_sendQueue;
    qsizetype m_queuedBytes = 0;
    bool m_shutdownQueued = false;
    bool m_senderStopping = false;
};
