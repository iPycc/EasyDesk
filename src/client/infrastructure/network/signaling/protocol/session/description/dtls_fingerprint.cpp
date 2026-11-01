#include "network/signaling/protocol/session/description/dtls_fingerprint.h"

#include <QStringList>

namespace easydesk::client::signaling {
namespace {

constexpr int kMaximumSessionDescriptionBytes = 32 * 1024;
constexpr auto kFingerprintPrefix = "a=fingerprint:";
constexpr auto kSha256Prefix = "a=fingerprint:sha-256 ";

int hexNibble(QChar character) {
    if (character >= QLatin1Char('0') && character <= QLatin1Char('9'))
        return character.unicode() - QLatin1Char('0').unicode();
    const QChar lowered = character.toLower();
    if (lowered >= QLatin1Char('a') && lowered <= QLatin1Char('f'))
        return lowered.unicode() - QLatin1Char('a').unicode() + 10;
    return -1;
}

bool parseFingerprint(const QString &value, QByteArray &fingerprint) {
    if (value.size() != 95)
        return false;
    QByteArray parsed;
    parsed.reserve(32);
    for (int index = 0; index < 32; ++index) {
        const int offset = index * 3;
        const int high = hexNibble(value.at(offset));
        const int low = hexNibble(value.at(offset + 1));
        if (high < 0 || low < 0 ||
            (index < 31 && value.at(offset + 2) != QLatin1Char(':'))) {
            return false;
        }
        parsed.append(static_cast<char>((high << 4) | low));
    }
    fingerprint = parsed;
    return true;
}

}  // namespace

bool extractSha256DtlsFingerprint(const QString &sdp,
                                  QByteArray &fingerprint,
                                  QString &error) {
    fingerprint.clear();
    const QByteArray utf8 = sdp.toUtf8();
    if (utf8.isEmpty() || utf8.size() > kMaximumSessionDescriptionBytes ||
        utf8.contains('\0')) {
        error = QStringLiteral("Session description size or encoding is invalid.");
        return false;
    }

    bool found = false;
    const QStringList lines = sdp.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        if (!line.startsWith(QString::fromLatin1(kFingerprintPrefix)))
            continue;
        if (!line.startsWith(QString::fromLatin1(kSha256Prefix))) {
            error = QStringLiteral("Session description DTLS fingerprint algorithm is invalid.");
            return false;
        }
        QByteArray parsed;
        if (!parseFingerprint(line.mid(
                                  QString::fromLatin1(kSha256Prefix).size()),
                              parsed) ||
            (found && parsed != fingerprint)) {
            error = QStringLiteral("Session description DTLS fingerprint is invalid.");
            return false;
        }
        fingerprint = parsed;
        found = true;
    }

    if (!found) {
        error = QStringLiteral("Session description has no SHA-256 DTLS fingerprint.");
        return false;
    }
    error.clear();
    return true;
}

}  // namespace easydesk::client::signaling
