#pragma once

#include "network/signaling/protocol/session/ice/session_ice_codec.h"

#include <QByteArray>
#include <QString>
#include <QVector>

class SessionIceDescriptionBinding final {
  public:
    bool parse(const QByteArray &sdp, QString &error);
    bool matches(
        const easydesk::client::signaling::SessionIceCandidate &candidate,
        QString &error) const;

    bool isValid() const;
    void clear();

  private:
    struct MediaSection {
        QString mid;
        QString usernameFragment;
    };

    QVector<MediaSection> m_mediaSections;
    bool m_valid = false;
};
