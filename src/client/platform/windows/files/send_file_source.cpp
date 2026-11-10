#include "files/send_file_source.h"
#include "security/secure_random.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>

#include <windows.h>
#include <utility>

using namespace easydesk::client::transfer;

struct SendFileSourceState {
    HANDLE handle = INVALID_HANDLE_VALUE;
    BY_HANDLE_FILE_INFORMATION identity{};
    ManifestFile file;
    quint64 offset = 0;
    bool failed = false;
    ~SendFileSourceState() {
        if (handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
    }
};

namespace {
quint64 fileSize(const BY_HANDLE_FILE_INFORMATION &info) {
    return (static_cast<quint64>(info.nFileSizeHigh) << 32U) | info.nFileSizeLow;
}

bool unchanged(const SendFileSourceState &state) {
    BY_HANDLE_FILE_INFORMATION info{};
    if (state.handle == INVALID_HANDLE_VALUE || !GetFileInformationByHandle(state.handle, &info))
        return false;
    const auto &before = state.identity;
    return info.dwVolumeSerialNumber == before.dwVolumeSerialNumber &&
           info.nFileIndexHigh == before.nFileIndexHigh && info.nFileIndexLow == before.nFileIndexLow &&
           fileSize(info) == fileSize(before) &&
           info.ftLastWriteTime.dwLowDateTime == before.ftLastWriteTime.dwLowDateTime &&
           info.ftLastWriteTime.dwHighDateTime == before.ftLastWriteTime.dwHighDateTime &&
           !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
}

bool rewindHandle(HANDLE handle) {
    LARGE_INTEGER start{};
    return SetFilePointerEx(handle, start, nullptr, FILE_BEGIN) != FALSE;
}

bool hashSource(SendFileSourceState &state) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QByteArray buffer(64 * 1024, Qt::Uninitialized);
    quint64 remaining = state.file.size;
    while (remaining != 0) {
        const DWORD amount = static_cast<DWORD>(qMin<quint64>(remaining, buffer.size()));
        DWORD read = 0;
        if (!ReadFile(state.handle, buffer.data(), amount, &read, nullptr) || read == 0 || read > amount)
            return false;
        hash.addData(buffer.constData(), static_cast<int>(read));
        remaining -= read;
    }
    if (!unchanged(state) || !rewindHandle(state.handle))
        return false;
    state.file.sha256 = hash.result();
    return true;
}
} // namespace

SendFileSource::SendFileSource(std::unique_ptr<SendFileSourceState> state) : m_state(std::move(state)) {}
SendFileSource::~SendFileSource() = default;

std::unique_ptr<SendFileSource> SendFileSource::open(const QString &localPath, QString &error) {
    const QFileInfo selected(localPath);
    if (localPath.isEmpty() || !selected.isAbsolute() || !isSafeWindowsFileName(selected.fileName())) {
        error = QStringLiteral("Selected file name or path is invalid.");
        return {};
    }
    const QString native = QDir::toNativeSeparators(localPath);
    auto state = std::make_unique<SendFileSourceState>();
    state->handle = CreateFileW(reinterpret_cast<const wchar_t *>(native.utf16()),
                                GENERIC_READ | FILE_READ_ATTRIBUTES, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OPEN_REPARSE_POINT,
                                nullptr);
    if (state->handle == INVALID_HANDLE_VALUE || GetFileType(state->handle) != FILE_TYPE_DISK ||
        !GetFileInformationByHandle(state->handle, &state->identity) ||
        (state->identity.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
        fileSize(state->identity) > kDefaultMaximumFileBytes) {
        error = QStringLiteral("Selected file is unavailable, unsupported or too large.");
        return {};
    }
    state->file.name = selected.fileName();
    state->file.size = fileSize(state->identity);
    state->file.fileId = easydesk::client::windows::generateSecureRandomBytes(16, error);
    if (state->file.fileId.size() != 16 || state->file.fileId == QByteArray(16, '\0')) {
        if (error.isEmpty()) error = QStringLiteral("File identifier could not be created.");
        return {};
    }
    state->file.sourceGeneration = easydesk::client::windows::generateSecureRandomBytes(16, error);
    if (state->file.sourceGeneration.size() != 16 || state->file.sourceGeneration == QByteArray(16, '\0')) {
        if (error.isEmpty()) error = QStringLiteral("File generation could not be created.");
        return {};
    }
    if (!hashSource(*state)) {
        error = QStringLiteral("Selected file changed or could not be read.");
        return {};
    }
    error.clear();
    return std::unique_ptr<SendFileSource>(new SendFileSource(std::move(state)));
}

const ManifestFile &SendFileSource::manifest() const { return m_state->file; }

bool SendFileSource::checkUnchanged(QString &error) {
    if (!m_state || m_state->failed || !unchanged(*m_state)) {
        if (m_state) m_state->failed = true;
        error = QStringLiteral("Selected file changed during transfer.");
        return false;
    }
    error.clear();
    return true;
}

bool SendFileSource::readChunk(quint64 offset, quint32 maximumBytes, QByteArray &bytes,
                               QByteArray &sha256, QString &error) {
    bytes.clear();
    sha256.clear();
    if (!m_state || m_state->failed || offset != m_state->offset || offset >= m_state->file.size ||
        maximumBytes == 0 || maximumBytes > static_cast<quint32>(kMaximumChunkPayload)) {
        error = QStringLiteral("File read position or size is invalid.");
        return false;
    }
    if (!checkUnchanged(error)) {
        return false;
    }
    const DWORD requested = static_cast<DWORD>(qMin<quint64>(m_state->file.size - offset, maximumBytes));
    QByteArray buffer(static_cast<int>(requested), Qt::Uninitialized);
    DWORD read = 0;
    if (!ReadFile(m_state->handle, buffer.data(), requested, &read, nullptr) ||
        read != requested || !unchanged(*m_state)) {
        m_state->failed = true;
        error = QStringLiteral("Selected file changed or could not be read.");
        return false;
    }
    m_state->offset += read;
    sha256 = QCryptographicHash::hash(buffer, QCryptographicHash::Sha256);
    bytes = std::move(buffer);
    error.clear();
    return true;
}
