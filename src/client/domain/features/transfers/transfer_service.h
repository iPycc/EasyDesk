#pragma once
#include <QObject>
#include "core/transfer.h"

class TransferService : public QObject {
    Q_OBJECT
  public:
    using QObject::QObject;
    virtual const QVector<TransferItem> &items() const = 0;
    virtual void start() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void cancel() = 0;
    virtual bool isPaused() const = 0;
    virtual bool isAvailable() const = 0;
  signals:
    void changed();
};
