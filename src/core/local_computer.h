#pragma once
#include <QString>
#include <QStringList>

struct LocalComputerInfo {
    QString computerName, displayName, operatingSystem, boardManufacturer, boardProduct, processor;
    QStringList networkAdapters;
};
