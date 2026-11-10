#pragma once

#include "features/transfers/transfer_manifest.h"

#include <QObject>
#include <QQueue>
#include <QThread>
#include <atomic>
#include <memory>

class ReceiveDirectory;
class SessionFileReceiver;

class SessionFileReceiveWorker final : public QObject {
    Q_OBJECT

  public:
    explicit SessionFileReceiveWorker(QObject *parent = nullptr);
    ~SessionFileReceiveWorker() override;
    bool start(const easydesk::client::transfer::TransferManifest &manifest,
               std::shared_ptr<ReceiveDirectory> directory, QString &error);
    bool enqueue(const QByteArray &frame, QString &error);
    void stop();

  signals:
    void receiptReady(const QByteArray &receipt);
    void fileCommitted(const QString &name);
    void batchCompleted();
    void failed(const QString &error);

  private:
    void dispatchNext();
    QThread m_thread;
    QObject *m_dispatcher = nullptr;
    std::shared_ptr<std::atomic_bool> m_cancellation;
    std::shared_ptr<SessionFileReceiver> m_receiver;
    QQueue<QByteArray> m_frames;
    qsizetype m_bufferedBytes = 0;
    bool m_active = false;
    bool m_busy = false;
};
