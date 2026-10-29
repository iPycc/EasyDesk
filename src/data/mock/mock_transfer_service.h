#pragma once
#include <QTimer>
#include "features/transfers/transfer_service.h"

class MockTransferService final : public TransferService {
    Q_OBJECT
  public:
    explicit MockTransferService(QObject *parent = nullptr);
    const QVector<TransferItem> &items() const override {
        return m_items;
    }
    void start() override;
    void pause() override;
    void resume() override;
    void cancel() override;
    bool isPaused() const override {
        return m_paused;
    }
    bool isSimulated() const override {
        return true;
    }

  private:
    QVector<TransferItem> m_items;
    QTimer m_timer;
    bool m_paused = false;
    void advance();
};
