#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

namespace easydesk::client::transfer {

constexpr quint64 kDefaultMaximumFileBytes = quint64(10) * 1024 * 1024 * 1024;
constexpr int kMaximumFiles = 100;
constexpr int kMaximumChunkPayload = 16292;

struct ManifestFile {
    QByteArray fileId;
    QByteArray sourceGeneration;
    QString name;
    quint64 size = 0;
    QByteArray sha256;
};

struct TransferManifest {
    QByteArray transferId;
    quint32 version = 1;
    QVector<ManifestFile> files;
};

bool isSafeWindowsFileName(const QString &name);
bool validateManifest(const TransferManifest &manifest, quint64 maximumFileBytes,
                      quint64 &totalBytes, QString &error);
QByteArray manifestDigest(const TransferManifest &manifest, quint64 maximumFileBytes,
                          QString &error);

} // namespace easydesk::client::transfer
