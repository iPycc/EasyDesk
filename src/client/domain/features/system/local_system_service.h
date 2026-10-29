#pragma once
#include <QObject>
#include "core/local_computer.h"

class LocalSystemService : public QObject {
    Q_OBJECT
  public:
    using QObject::QObject;
    virtual LocalComputerInfo information() const = 0;
    virtual void refresh() = 0;
    virtual void setDisplayName(const QString &name) = 0;
  signals:
    void changed();
};
