#include "network/files/protocol/transfer_decision_codec.h"

#include <QBuffer>
#include <QDataStream>
#include <utility>

namespace easydesk::client::transfer {
namespace {
constexpr int kMessageSize = 74;
const QByteArray kMagic = QByteArrayLiteral("EDR1");

bool validToken(const QByteArray &token) {
    return token.size() == 16 && token != QByteArray(16, '\0');
}

bool valid(const TransferDecision &value) {
    if (!validToken(value.transferId) || value.manifestDigest.size() != 32)
        return false;
    if (value.decision == OfferDecision::Accept)
        return validToken(value.rootId) && value.windowBytes > 0 &&
               value.windowBytes <= kMaximumReceiveWindow && value.conflictPolicy == ConflictPolicy::Rename;
    return value.decision == OfferDecision::Decline && value.rootId.isEmpty() && value.windowBytes == 0 &&
           value.conflictPolicy == ConflictPolicy::Rename;
}
} // namespace

bool isTransferDecision(const QByteArray &message) { return message.startsWith(kMagic); }

bool encodeTransferDecision(const TransferDecision &decision, QByteArray &message, QString &error) {
    message.clear();
    if (!valid(decision)) {
        error = QStringLiteral("Invalid transfer decision.");
        return false;
    }
    QDataStream stream(&message, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream.writeRawData(kMagic.constData(), kMagic.size());
    stream.writeRawData(decision.transferId.constData(), decision.transferId.size());
    stream.writeRawData(decision.manifestDigest.constData(), decision.manifestDigest.size());
    const QByteArray root = decision.decision == OfferDecision::Accept ? decision.rootId : QByteArray(16, '\0');
    stream << static_cast<quint8>(decision.decision);
    stream.writeRawData(root.constData(), root.size());
    stream << decision.windowBytes << static_cast<quint8>(decision.conflictPolicy);
    if (stream.status() != QDataStream::Ok || message.size() != kMessageSize) {
        message.clear();
        error = QStringLiteral("Transfer decision could not be encoded.");
        return false;
    }
    error.clear();
    return true;
}

bool decodeTransferDecision(const QByteArray &message, TransferDecision &decision, QString &error) {
    decision = {};
    if (message.size() != kMessageSize || !isTransferDecision(message)) {
        error = QStringLiteral("Invalid transfer decision.");
        return false;
    }
    QBuffer buffer;
    buffer.setData(message);
    buffer.open(QIODevice::ReadOnly);
    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);
    char magic[4];
    TransferDecision decoded;
    decoded.transferId.resize(16);
    decoded.manifestDigest.resize(32);
    decoded.rootId.resize(16);
    quint8 kind = 0, policy = 0;
    if (stream.readRawData(magic, 4) != 4 || stream.readRawData(decoded.transferId.data(), 16) != 16 ||
        stream.readRawData(decoded.manifestDigest.data(), 32) != 32) {
        error = QStringLiteral("Invalid transfer decision.");
        return false;
    }
    stream >> kind;
    if (stream.readRawData(decoded.rootId.data(), 16) != 16) {
        error = QStringLiteral("Invalid transfer decision.");
        return false;
    }
    stream >> decoded.windowBytes >> policy;
    decoded.decision = static_cast<OfferDecision>(kind);
    decoded.conflictPolicy = static_cast<ConflictPolicy>(policy);
    if (decoded.decision == OfferDecision::Decline && decoded.rootId == QByteArray(16, '\0'))
        decoded.rootId.clear();
    if (stream.status() != QDataStream::Ok || !buffer.atEnd() || !valid(decoded)) {
        error = QStringLiteral("Invalid transfer decision.");
        return false;
    }
    decision = std::move(decoded);
    error.clear();
    return true;
}

} // namespace easydesk::client::transfer
