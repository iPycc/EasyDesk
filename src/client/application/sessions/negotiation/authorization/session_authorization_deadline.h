#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

class SessionAuthorizationDeadline final : public QObject {
    Q_OBJECT

  public:
    explicit SessionAuthorizationDeadline(QObject *parent = nullptr);

    bool start(qint64 expiresAt);
    void stop();

  signals:
    void expired();

  private:
    void check();

    QTimer m_timer;
    QElapsedTimer m_elapsed;
    qint64 m_expiresAt = 0;
    qint64 m_remainingMilliseconds = 0;
};
