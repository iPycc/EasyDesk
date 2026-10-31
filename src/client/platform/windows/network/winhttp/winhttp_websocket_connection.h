#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <memory>

struct WinHttpWebSocketReceiveResult {
    enum class Kind {
        Text,
        Closed,
    };

    Kind kind = Kind::Text;
    QByteArray payload;
    quint16 closeStatus = 0;
    QString closeReason;
};

class WinHttpWebSocketConnection final {
  public:
    WinHttpWebSocketConnection();
    ~WinHttpWebSocketConnection();

    WinHttpWebSocketConnection(const WinHttpWebSocketConnection &) = delete;
    WinHttpWebSocketConnection &operator=(const WinHttpWebSocketConnection &) = delete;

    bool open(const QUrl &endpoint, bool allowInsecureLoopback, QString &error);
    bool sendText(const QByteArray &message, QString &error);
    bool receive(WinHttpWebSocketReceiveResult &result, QString &error);
    bool shutdown(QString &error);
    void abort();
    bool isOpen() const;

  private:
    struct Implementation;
    std::unique_ptr<Implementation> m_impl;
};
