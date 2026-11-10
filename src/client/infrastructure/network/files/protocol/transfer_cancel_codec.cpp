#include "network/files/protocol/transfer_cancel_codec.h"

#include <utility>

namespace easydesk::client::transfer {
namespace {
const QByteArray kMagic = QByteArrayLiteral("EDX1");
constexpr int kMessageSize = 53;

bool valid(const TransferCancellation &cancel) {
    if (cancel.transferId.size() != 16 || cancel.transferId == QByteArray(16, '\0') ||
        cancel.manifestDigest.size() != 32)
        return false;
    switch (cancel.reason) {
    case TransferCancelReason::SenderCancelled:
    case TransferCancelReason::ReceiverCancelled:
    case TransferCancelReason::SenderFailed:
    case TransferCancelReason::ReceiverFailed:
        return true;
    }
    return false;
}
} // namespace

bool isTransferCancellation(const QByteArray &message) { return message.startsWith(kMagic); }

bool encodeTransferCancellation(const TransferCancellation &cancel, QByteArray &message, QString &error) {
    message.clear();
    if (!valid(cancel)) {
        error = QStringLiteral("Invalid transfer cancellation.");
        return false;
    }
    message.reserve(kMessageSize);
    message.append(kMagic);
    message.append(cancel.transferId);
    message.append(cancel.manifestDigest);
    message.append(static_cast<char>(cancel.reason));
    error.clear();
    return true;
}

bool decodeTransferCancellation(const QByteArray &message, TransferCancellation &cancel, QString &error) {
    cancel = {};
    if (message.size() != kMessageSize || !isTransferCancellation(message)) {
        error = QStringLiteral("Invalid transfer cancellation.");
        return false;
    }
    TransferCancellation decoded;
    decoded.transferId = message.mid(4, 16);
    decoded.manifestDigest = message.mid(20, 32);
    decoded.reason = static_cast<TransferCancelReason>(static_cast<quint8>(message.at(52)));
    if (!valid(decoded)) {
        error = QStringLiteral("Invalid transfer cancellation.");
        return false;
    }
    cancel = std::move(decoded);
    error.clear();
    return true;
}

} // namespace easydesk::client::transfer
