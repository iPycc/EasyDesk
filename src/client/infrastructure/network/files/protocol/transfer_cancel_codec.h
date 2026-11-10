#pragma once

#include <QByteArray>
#include <QString>

namespace easydesk::client::transfer {

enum class TransferCancelReason : quint8 {
    SenderCancelled = 1,
    ReceiverCancelled = 2,
    SenderFailed = 3,
    ReceiverFailed = 4,
};

struct TransferCancellation {
    QByteArray transferId;
    QByteArray manifestDigest;
    TransferCancelReason reason = TransferCancelReason::SenderCancelled;
};

bool isTransferCancellation(const QByteArray &message);
bool encodeTransferCancellation(const TransferCancellation &cancel, QByteArray &message, QString &error);
bool decodeTransferCancellation(const QByteArray &message, TransferCancellation &cancel, QString &error);

} // namespace easydesk::client::transfer
