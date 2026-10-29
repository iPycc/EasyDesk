#pragma once
#include <QString>
#include <QVector>

enum class TransferState { Waiting, Running, Paused, Completed, Cancelled };
struct TransferItem {
    QString name, size;
    int progress = 0;
    TransferState state = TransferState::Waiting;
};
