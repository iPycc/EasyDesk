#include "sessions/negotiation/ice/session_ice_description_binding.h"

#include <QStringList>

#include <utility>

namespace {

constexpr int kMaximumMediaSections = 64;
constexpr int kMaximumMidBytes = 64;
constexpr int kMaximumUfragBytes = 256;

bool validAsciiToken(const QString &value,
                     int maximumBytes,
                     bool iceCharactersOnly) {
    const QByteArray bytes = value.toLatin1();
    if (value.isEmpty() || bytes.size() > maximumBytes ||
        QString::fromLatin1(bytes) != value) {
        return false;
    }
    for (const char byte : bytes) {
        const bool alphaNumeric =
            (byte >= 'A' && byte <= 'Z') ||
            (byte >= 'a' && byte <= 'z') ||
            (byte >= '0' && byte <= '9');
        if (iceCharactersOnly) {
            if (!alphaNumeric && byte != '+' && byte != '/')
                return false;
        } else if (byte <= 0x20 || byte >= 0x7f) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool SessionIceDescriptionBinding::parse(const QByteArray &sdp,
                                         QString &error) {
    clear();
    if (sdp.isEmpty() || sdp.size() > 64 * 1024 || sdp.contains('\0')) {
        error = QStringLiteral("ICE description SDP is invalid.");
        return false;
    }
    const QString text = QString::fromUtf8(sdp);
    if (text.toUtf8() != sdp || !text.startsWith(QStringLiteral("v=0"))) {
        error = QStringLiteral("ICE description SDP is invalid.");
        return false;
    }

    QString sessionUfrag;
    QVector<MediaSection> sections;
    int currentSection = -1;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        if (line.startsWith(QStringLiteral("m="))) {
            if (sections.size() >= kMaximumMediaSections) {
                error = QStringLiteral("ICE description has too many media sections.");
                return false;
            }
            sections.append({});
            currentSection = sections.size() - 1;
            continue;
        }
        if (line.startsWith(QStringLiteral("a=mid:"))) {
            if (currentSection < 0 ||
                !sections.at(currentSection).mid.isEmpty()) {
                error = QStringLiteral("ICE description MID is invalid.");
                return false;
            }
            const QString mid = line.mid(6);
            if (!validAsciiToken(mid, kMaximumMidBytes, false)) {
                error = QStringLiteral("ICE description MID is invalid.");
                return false;
            }
            sections[currentSection].mid = mid;
            continue;
        }
        if (!line.startsWith(QStringLiteral("a=ice-ufrag:")))
            continue;

        const QString ufrag = line.mid(12);
        if (!validAsciiToken(ufrag, kMaximumUfragBytes, true)) {
            error = QStringLiteral("ICE description username fragment is invalid.");
            return false;
        }
        QString &destination = currentSection < 0
                                   ? sessionUfrag
                                   : sections[currentSection].usernameFragment;
        if (!destination.isEmpty()) {
            error = QStringLiteral(
                "ICE description username fragment is duplicated.");
            return false;
        }
        destination = ufrag;
    }

    if (sections.isEmpty()) {
        error = QStringLiteral("ICE description has no media sections.");
        return false;
    }
    for (int index = 0; index < sections.size(); ++index) {
        MediaSection &section = sections[index];
        if (section.mid.isEmpty()) {
            error = QStringLiteral("ICE description media MID is missing.");
            return false;
        }
        for (int previous = 0; previous < index; ++previous) {
            if (sections.at(previous).mid == section.mid) {
                error = QStringLiteral("ICE description media MID is duplicated.");
                return false;
            }
        }
        if (section.usernameFragment.isEmpty())
            section.usernameFragment = sessionUfrag;
        if (section.usernameFragment.isEmpty()) {
            error = QStringLiteral(
                "ICE description media username fragment is missing.");
            return false;
        }
    }

    m_mediaSections = std::move(sections);
    m_valid = true;
    error.clear();
    return true;
}

bool SessionIceDescriptionBinding::matches(
    const easydesk::client::signaling::SessionIceCandidate &candidate,
    QString &error) const {
    if (!m_valid || candidate.sdpMLineIndex >=
                        static_cast<quint32>(m_mediaSections.size())) {
        error = QStringLiteral("ICE candidate media section is invalid.");
        return false;
    }
    const MediaSection &section =
        m_mediaSections.at(static_cast<int>(candidate.sdpMLineIndex));
    if (candidate.sdpMid != section.mid ||
        candidate.usernameFragment != section.usernameFragment) {
        error = QStringLiteral("ICE candidate does not match its description.");
        return false;
    }
    error.clear();
    return true;
}

bool SessionIceDescriptionBinding::isValid() const {
    return m_valid;
}

void SessionIceDescriptionBinding::clear() {
    m_mediaSections.clear();
    m_valid = false;
}
