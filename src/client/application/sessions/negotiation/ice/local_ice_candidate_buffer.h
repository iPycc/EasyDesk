#pragma once

#include "network/signaling/protocol/session/ice/session_ice_codec.h"

#include <QByteArray>
#include <QString>
#include <QVector>

class LocalIceCandidateBuffer final {
  public:
    static constexpr int kMaximumBatchSize = 8;
    static constexpr int kMaximumCandidatesPerGeneration = 64;

    bool enqueue(quint32 iceGeneration,
                 const QByteArray &sdpMid,
                 qint32 sdpMlineIndex,
                 const QByteArray &candidate,
                 QString &error);
    QVector<easydesk::client::signaling::SessionIceCandidate> takeBatch();

    bool isEmpty() const;
    int size() const;
    quint32 iceGeneration() const;
    void clear();

  private:
    quint32 m_iceGeneration = 0;
    QVector<easydesk::client::signaling::SessionIceCandidate> m_pending;
    QVector<easydesk::client::signaling::SessionIceCandidate> m_seen;
};
