#pragma once

#include "features/transfers/transfer_manifest.h"
#include "network/files/protocol/file_receipt_codec.h"

#include <QObject>
#include <QStringList>
#include <QThread>
#include <atomic>
#include <memory>

struct SessionFileSendWorkerState;

class SessionFileSendWorker final : public QObject {
    Q_OBJECT

  public:
    explicit SessionFileSendWorker(QObject *parent = nullptr);
    ~SessionFileSendWorker() override;
    bool prepare(const QStringList &paths, QString &error);
    bool begin(quint32 windowBytes);
    bool requestFrame();
    bool markQueued();
    bool applyReceipt(const easydesk::client::transfer::FileReceipt &receipt);
    void stop();

  signals:
    void prepared(const easydesk::client::transfer::TransferManifest &manifest);
    void readinessChanged(bool ready);
    void frameReady(const QByteArray &frame);
    void progress(quint64 queuedBytes, quint64 verifiedBytes);
    void receiptProcessed();
    void fileCommitted(const QString &name);
    void batchCompleted();
    void failed(const QString &error);

  private:
    void fail(const QString &error);
    bool usable() const;
    QThread m_thread;
    QObject *m_dispatcher = nullptr;
    std::shared_ptr<std::atomic_bool> m_cancellation;
    std::shared_ptr<SessionFileSendWorkerState> m_state;
    bool m_active = false;
    bool m_frameRequested = false;
    bool m_framePending = false;
};
