#pragma once

#include <QByteArray>
#include <QCryptographicHash>
#include <QString>

#include <windows.h>

#include <memory>

struct ReceiveDirectoryState {
    HANDLE handle = INVALID_HANDLE_VALUE;
    QByteArray rootId;
    ~ReceiveDirectoryState();
};

struct ReceiveFileState {
    std::shared_ptr<ReceiveDirectoryState> directory;
    HANDLE handle = INVALID_HANDLE_VALUE;
    QString temporaryName;
    QString desiredName;
    quint64 expectedSize = 0;
    quint64 received = 0;
    QByteArray expectedDigest;
    QCryptographicHash digest{QCryptographicHash::Sha256};
    bool committed = false;
    bool failed = false;
    ~ReceiveFileState();
};

bool receivePathStillContained(const ReceiveFileState &file);
