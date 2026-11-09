#include "features/transfers/transfer_manifest.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QSet>

#include <limits>

namespace easydesk::client::transfer {
namespace {
bool validToken(const QByteArray &value) {
    return value.size() == 16 && value != QByteArray(16, '\0');
}

bool reservedDeviceName(const QString &name) {
    QString base = name.section(QLatin1Char('.'), 0, 0).toUpper();
    while (base.endsWith(QLatin1Char(' ')))
        base.chop(1);
    if (base == QLatin1String("CON") || base == QLatin1String("PRN") ||
        base == QLatin1String("AUX") || base == QLatin1String("NUL") ||
        base == QLatin1String("CONIN$") || base == QLatin1String("CONOUT$"))
        return true;
    if (base.size() != 4 || (!base.startsWith(QLatin1String("COM")) &&
                             !base.startsWith(QLatin1String("LPT"))))
        return false;
    const ushort digit = base.at(3).unicode();
    return (digit >= '1' && digit <= '9') || digit == 0x00b9 || digit == 0x00b2 || digit == 0x00b3;
}
} // namespace

bool isSafeWindowsFileName(const QString &name) {
    if (name.isEmpty() || name.size() > 255 || name.endsWith(QLatin1Char('.')) ||
        name.endsWith(QLatin1Char(' ')) || reservedDeviceName(name))
        return false;
    for (int i = 0; i < name.size(); ++i) {
        const QChar character = name.at(i);
        const ushort value = character.unicode();
        if (value < 32 || QStringLiteral("<>:\"/\\|?*").contains(character))
            return false;
        if (character.isHighSurrogate()) {
            if (++i >= name.size() || !name.at(i).isLowSurrogate())
                return false;
        } else if (character.isLowSurrogate()) {
            return false;
        }
    }
    return true;
}

bool validateManifest(const TransferManifest &manifest, quint64 maximumFileBytes,
                      quint64 &totalBytes, QString &error) {
    totalBytes = 0;
    if (!validToken(manifest.transferId) || manifest.version != 1 || manifest.files.isEmpty() ||
        manifest.files.size() > kMaximumFiles || maximumFileBytes > kDefaultMaximumFileBytes) {
        error = QStringLiteral("Invalid transfer manifest.");
        return false;
    }
    QSet<QByteArray> ids;
    quint64 total = 0;
    for (const ManifestFile &file : manifest.files) {
        if (!validToken(file.fileId) || !validToken(file.sourceGeneration) ||
            ids.contains(file.fileId) || !isSafeWindowsFileName(file.name) ||
            file.size > maximumFileBytes || file.sha256.size() != 32 ||
            file.size > std::numeric_limits<quint64>::max() - total) {
            error = QStringLiteral("Invalid transfer file entry.");
            return false;
        }
        ids.insert(file.fileId);
        total += file.size;
    }
    totalBytes = total;
    error.clear();
    return true;
}

QByteArray manifestDigest(const TransferManifest &manifest, quint64 maximumFileBytes,
                          QString &error) {
    quint64 total = 0;
    if (!validateManifest(manifest, maximumFileBytes, total, error))
        return {};

    QByteArray canonical;
    QDataStream stream(&canonical, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    const QByteArray domain = QByteArrayLiteral("EasyDesk transfer manifest v1");
    stream.writeRawData(domain.constData(), domain.size());
    stream << manifest.version;
    stream.writeRawData(manifest.transferId.constData(), manifest.transferId.size());
    stream << static_cast<quint16>(manifest.files.size()) << total;
    for (const ManifestFile &file : manifest.files) {
        const QByteArray utf8Name = file.name.toUtf8();
        stream.writeRawData(file.fileId.constData(), file.fileId.size());
        stream.writeRawData(file.sourceGeneration.constData(), file.sourceGeneration.size());
        stream << static_cast<quint16>(utf8Name.size());
        stream.writeRawData(utf8Name.constData(), utf8Name.size());
        stream << file.size;
        stream.writeRawData(file.sha256.constData(), file.sha256.size());
    }
    if (stream.status() != QDataStream::Ok) {
        error = QStringLiteral("Could not encode transfer manifest.");
        return {};
    }
    error.clear();
    return QCryptographicHash::hash(canonical, QCryptographicHash::Sha256);
}

} // namespace easydesk::client::transfer
