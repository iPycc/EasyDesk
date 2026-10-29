#pragma once

#include "features/transfers/transfer_service.h"

class LocalTransferService final : public TransferService {
    Q_OBJECT
  public:
    using TransferService::TransferService;
    const QVector<TransferItem> &items() const override {
        return m_items;
    }
    void start() override;
    void pause() override;
    void resume() override;
    void cancel() override;
    bool isPaused() const override {
        return false;
    }
    bool isAvailable() const override {
        return false;
    }

  private:
    QVector<TransferItem> m_items;
};
