#include "files/receive_file.h"
#include "files/receive_state.h"
#include "features/transfers/transfer_manifest.h"

#include <cstring>
#include <winternl.h>

namespace {
constexpr int kMaximumChunkBytes = 16384;
constexpr int kMaximumCollisionAttempts = 1000;
constexpr ULONG kStatusObjectNameCollision = 0xc0000035U;
using NtSetInformationFileFunction = NTSTATUS(NTAPI *)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG,
                                                       FILE_INFORMATION_CLASS);

QString collisionName(const QString &name, int index) {
    if (index == 0)
        return name;
    const QString suffix = QStringLiteral(" (%1)").arg(index);
    const int dot = name.lastIndexOf(QLatin1Char('.'));
    const QString extension = dot > 0 && name.size() - dot <= 32 ? name.mid(dot) : QString();
    QString stem = extension.isEmpty() ? name : name.left(dot);
    stem.truncate(255 - suffix.size() - extension.size());
    if (!stem.isEmpty() && stem.at(stem.size() - 1).isHighSurrogate())
        stem.chop(1);
    while (stem.endsWith(QLatin1Char('.')) || stem.endsWith(QLatin1Char(' ')))
        stem.chop(1);
    if (stem.isEmpty())
        stem = QStringLiteral("file");
    return stem + suffix + extension;
}

bool setDeleteOnClose(HANDLE handle, bool enabled) {
    FILE_DISPOSITION_INFO disposition{static_cast<BOOLEAN>(enabled ? TRUE : FALSE)};
    return SetFileInformationByHandle(handle, FileDispositionInfo, &disposition, sizeof(disposition));
}

bool isCancelled(const std::atomic_bool *cancelled) {
    return cancelled && cancelled->load(std::memory_order_acquire);
}

bool verifyStoredFile(const ReceiveFileState &state, const std::atomic_bool *cancelled) {
    LARGE_INTEGER size{};
    LARGE_INTEGER start{};
    if (!GetFileSizeEx(state.handle, &size) || size.QuadPart < 0 ||
        static_cast<quint64>(size.QuadPart) != state.expectedSize ||
        !SetFilePointerEx(state.handle, start, nullptr, FILE_BEGIN))
        return false;
    QCryptographicHash digest(QCryptographicHash::Sha256);
    QByteArray buffer(64 * 1024, Qt::Uninitialized);
    quint64 remaining = state.expectedSize;
    while (remaining != 0) {
        if (isCancelled(cancelled)) return false;
        const DWORD request = static_cast<DWORD>(qMin<quint64>(remaining, static_cast<quint64>(buffer.size())));
        DWORD read = 0;
        if (!ReadFile(state.handle, buffer.data(), request, &read, nullptr) || read == 0 || read > request)
            return false;
        digest.addData(buffer.constData(), static_cast<int>(read));
        remaining -= read;
    }
    return !isCancelled(cancelled) && digest.result() == state.expectedDigest;
}
} // namespace

ReceiveFileState::~ReceiveFileState() {
    if (handle != INVALID_HANDLE_VALUE) {
        if (!committed)
            (void)setDeleteOnClose(handle, true);
        CloseHandle(handle);
    }
}

ReceiveFile::ReceiveFile(std::unique_ptr<ReceiveFileState> state) : m_state(std::move(state)) {}
ReceiveFile::~ReceiveFile() = default;

bool ReceiveFile::append(quint64 offset, const QByteArray &bytes, const QByteArray &chunkSha256,
                         QString &error, const std::atomic_bool *cancelled) {
    if (isCancelled(cancelled) || !m_state || m_state->committed || m_state->failed || offset != m_state->received ||
        bytes.isEmpty() || bytes.size() > kMaximumChunkBytes || chunkSha256.size() != 32 ||
        static_cast<quint64>(bytes.size()) > m_state->expectedSize - m_state->received ||
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256) != chunkSha256 ||
        !receivePathStillContained(*m_state)) {
        error = QStringLiteral("File chunk is invalid or the destination is unavailable.");
        return false;
    }
    DWORD written = 0;
    if (!WriteFile(m_state->handle, bytes.constData(), static_cast<DWORD>(bytes.size()), &written, nullptr) ||
        written != static_cast<DWORD>(bytes.size()) || isCancelled(cancelled)) {
        m_state->failed = true;
        error = QStringLiteral("File chunk could not be written.");
        return false;
    }
    m_state->digest.addData(bytes);
    m_state->received += written;
    error.clear();
    return true;
}

bool ReceiveFile::commit(QString &finalName, QString &error, const std::atomic_bool *cancelled) {
    finalName.clear();
    if (isCancelled(cancelled) || !m_state || m_state->committed || m_state->failed || m_state->received != m_state->expectedSize ||
        m_state->digest.result() != m_state->expectedDigest || !receivePathStillContained(*m_state)) {
        error = QStringLiteral("Received file is incomplete or failed verification.");
        return false;
    }
    FILE_BASIC_INFO attributes{};
    if (!FlushFileBuffers(m_state->handle) || !verifyStoredFile(*m_state, cancelled) ||
        !GetFileInformationByHandleEx(m_state->handle, FileBasicInfo, &attributes, sizeof(attributes))) {
        error = QStringLiteral("Received file could not be committed.");
        return false;
    }
    attributes.FileAttributes &= ~FILE_ATTRIBUTE_TEMPORARY;
    if (attributes.FileAttributes == 0)
        attributes.FileAttributes = FILE_ATTRIBUTE_NORMAL;
    if (!SetFileInformationByHandle(m_state->handle, FileBasicInfo, &attributes, sizeof(attributes)) ||
        !setDeleteOnClose(m_state->handle, false)) {
        error = QStringLiteral("Received file could not be committed.");
        return false;
    }
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    const auto renameFile = reinterpret_cast<NtSetInformationFileFunction>(
        ntdll ? GetProcAddress(ntdll, "NtSetInformationFile") : nullptr);
    if (!renameFile) {
        (void)setDeleteOnClose(m_state->handle, true);
        error = QStringLiteral("Secure file rename is unavailable.");
        return false;
    }
    ULONG renameError = kStatusObjectNameCollision;
    for (int index = 0; index < kMaximumCollisionAttempts; ++index) {
        if (isCancelled(cancelled) || !receivePathStillContained(*m_state)) {
            (void)setDeleteOnClose(m_state->handle, true);
            m_state->failed = true;
            error = QStringLiteral("Receive directory changed before commit.");
            return false;
        }
        const QString name = collisionName(m_state->desiredName, index);
        if (!easydesk::client::transfer::isSafeWindowsFileName(name))
            continue;
        const DWORD byteCount = static_cast<DWORD>(name.size() * sizeof(wchar_t));
        QByteArray buffer(static_cast<int>(sizeof(FILE_RENAME_INFO) + byteCount), '\0');
        auto *info = reinterpret_cast<FILE_RENAME_INFO *>(buffer.data());
        info->ReplaceIfExists = FALSE;
        info->RootDirectory = m_state->directory->handle;
        info->FileNameLength = byteCount;
        std::memcpy(info->FileName, name.utf16(), byteCount);
        IO_STATUS_BLOCK io{};
        const NTSTATUS result = renameFile(m_state->handle, &io, info,
                                           static_cast<ULONG>(buffer.size()),
                                           static_cast<FILE_INFORMATION_CLASS>(10));
        if (result >= 0) {
            m_state->temporaryName = name;
            if (isCancelled(cancelled) || !receivePathStillContained(*m_state)) {
                (void)setDeleteOnClose(m_state->handle, true);
                m_state->failed = true;
                error = QStringLiteral("Renamed file is outside the selected directory.");
                return false;
            }
            m_state->committed = true;
            finalName = name;
            error.clear();
            return true;
        }
        const ULONG status = static_cast<ULONG>(result);
        renameError = status;
        if (status != kStatusObjectNameCollision)
            break;
    }
    (void)setDeleteOnClose(m_state->handle, true);
    error = QStringLiteral("Received file could not be safely renamed (0x%1).")
                .arg(renameError, 8, 16, QLatin1Char('0'));
    return false;
}

void ReceiveFile::cancel() { m_state.reset(); }
