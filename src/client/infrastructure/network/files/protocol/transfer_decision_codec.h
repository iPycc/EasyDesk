#pragma once

#include <QByteArray>
#include <QString>

namespace easydesk::client::transfer {

enum class OfferDecision : quint8 { Accept = 1, Decline = 2 };
enum class ConflictPolicy : quint8 { Rename = 1 };
constexpr quint32 kMaximumReceiveWindow = 512 * 1024;

struct TransferDecision {
    QByteArray transferId;
    QByteArray manifestDigest;
    OfferDecision decision = OfferDecision::Decline;
    QByteArray rootId;
    quint32 windowBytes = 0;
    ConflictPolicy conflictPolicy = ConflictPolicy::Rename;
};

bool isTransferDecision(const QByteArray &message);
bool encodeTransferDecision(const TransferDecision &decision, QByteArray &message, QString &error);
bool decodeTransferDecision(const QByteArray &message, TransferDecision &decision, QString &error);

} // namespace easydesk::client::transfer
