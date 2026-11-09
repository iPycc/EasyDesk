#pragma once

#include "features/transfers/transfer_manifest.h"

namespace easydesk::client::transfer {

bool encodeTransferOffer(const TransferManifest &manifest, QVector<QByteArray> &frames,
                         QString &error);

class TransferOfferDecoder final {
  public:
    enum class Result { Pending, Complete, Invalid };

    explicit TransferOfferDecoder(quint64 maximumFileBytes = kDefaultMaximumFileBytes);
    Result receive(const QByteArray &frame, TransferManifest &manifest, QString &error);
    void reset();

  private:
    quint64 m_maximumFileBytes;
    QByteArray m_transferId;
    QByteArray m_digest;
    QByteArray m_payload;
    quint32 m_payloadSize = 0;
    quint16 m_count = 0;
    quint16 m_next = 0;
};

} // namespace easydesk::client::transfer
