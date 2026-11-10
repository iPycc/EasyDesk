#include "files/receive_directory.h"
#include "files/receive_file.h"
#include "files/receive_state.h"
#include "security/secure_random.h"

#include <QFileInfo>
#include <QDir>

#include <winternl.h>

namespace {
constexpr ULONG kObjDontReparse = 0x00001000;
constexpr ULONG kFileCreate = 0x00000002;
constexpr ULONG kFileNonDirectory = 0x00000040;
constexpr ULONG kFileSynchronousNonalert = 0x00000020;
constexpr ULONG kFileOpenReparsePoint = 0x00200000;
using NtCreateFileFunction = NTSTATUS(NTAPI *)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES,
    PIO_STATUS_BLOCK, PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);

QString finalPath(HANDLE handle) {
    const DWORD flags = FILE_NAME_NORMALIZED | VOLUME_NAME_DOS;
    const DWORD required = GetFinalPathNameByHandleW(handle, nullptr, 0, flags);
    if (required == 0 || required > 32768)
        return {};
    QString path(static_cast<int>(required), Qt::Uninitialized);
    const DWORD written = GetFinalPathNameByHandleW(handle, reinterpret_cast<wchar_t *>(path.data()),
                                                     required, flags);
    if (written == 0 || written >= required)
        return {};
    path.resize(static_cast<int>(written));
    return path;
}

bool hasExpectedAttributes(HANDLE handle, bool directory) {
    FILE_ATTRIBUTE_TAG_INFO info{};
    return GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &info, sizeof(info)) &&
           !(info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
           bool(info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) == directory;
}
} // namespace

ReceiveDirectoryState::~ReceiveDirectoryState() {
    if (handle != INVALID_HANDLE_VALUE)
        CloseHandle(handle);
}

bool receivePathStillContained(const ReceiveFileState &file) {
    if (!file.directory || file.directory->handle == INVALID_HANDLE_VALUE ||
        file.handle == INVALID_HANDLE_VALUE || !hasExpectedAttributes(file.directory->handle, true) ||
        !hasExpectedAttributes(file.handle, false))
        return false;
    QString parent = finalPath(file.directory->handle);
    const QString actual = finalPath(file.handle);
    if (parent.isEmpty() || actual.isEmpty())
        return false;
    if (!parent.endsWith(QLatin1Char('\\')))
        parent.append(QLatin1Char('\\'));
    return QString::compare(parent + file.temporaryName, actual, Qt::CaseInsensitive) == 0;
}

ReceiveDirectory::ReceiveDirectory(std::shared_ptr<ReceiveDirectoryState> state)
    : m_state(std::move(state)) {}
ReceiveDirectory::~ReceiveDirectory() = default;

std::unique_ptr<ReceiveDirectory> ReceiveDirectory::open(const QString &directory, QString &error) {
    if (directory.isEmpty() || !QFileInfo(directory).isAbsolute()) {
        error = QStringLiteral("Receive directory must be selected locally.");
        return {};
    }
    const QString native = QDir::toNativeSeparators(directory);
    auto state = std::make_shared<ReceiveDirectoryState>();
    state->handle = CreateFileW(reinterpret_cast<const wchar_t *>(native.utf16()),
                                FILE_LIST_DIRECTORY | FILE_ADD_FILE | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (state->handle == INVALID_HANDLE_VALUE || !hasExpectedAttributes(state->handle, true) ||
        finalPath(state->handle).isEmpty()) {
        error = QStringLiteral("Receive directory could not be opened safely.");
        return {};
    }
    state->rootId = easydesk::client::windows::generateSecureRandomBytes(16, error);
    if (state->rootId.size() != 16 || state->rootId == QByteArray(16, '\0')) {
        if (error.isEmpty())
            error = QStringLiteral("Receive directory identifier could not be created.");
        return {};
    }
    error.clear();
    return std::unique_ptr<ReceiveDirectory>(new ReceiveDirectory(std::move(state)));
}

QByteArray ReceiveDirectory::rootId() const { return m_state ? m_state->rootId : QByteArray(); }

std::unique_ptr<ReceiveFile> ReceiveDirectory::createFile(
    const easydesk::client::transfer::ManifestFile &file, QString &error) const {
    using namespace easydesk::client::transfer;
    if (!m_state || !isSafeWindowsFileName(file.name) || file.size > kDefaultMaximumFileBytes ||
        file.sha256.size() != 32 || !hasExpectedAttributes(m_state->handle, true)) {
        error = QStringLiteral("Receive file metadata is invalid.");
        return {};
    }
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    const auto create = reinterpret_cast<NtCreateFileFunction>(
        ntdll ? GetProcAddress(ntdll, "NtCreateFile") : nullptr);
    if (!create) {
        error = QStringLiteral("Secure file creation is unavailable.");
        return {};
    }
    auto state = std::make_unique<ReceiveFileState>();
    state->directory = m_state;
    state->desiredName = file.name;
    state->expectedSize = file.size;
    state->expectedDigest = file.sha256;
    const QByteArray random = easydesk::client::windows::generateSecureRandomBytes(16, error);
    if (random.size() != 16)
        return {};
    state->temporaryName = QStringLiteral(".easydesk-") + QString::fromLatin1(random.toHex()) + QStringLiteral(".part");
    UNICODE_STRING name{};
    name.Buffer = reinterpret_cast<PWSTR>(state->temporaryName.data());
    name.Length = static_cast<USHORT>(state->temporaryName.size() * sizeof(wchar_t));
    name.MaximumLength = name.Length;
    OBJECT_ATTRIBUTES attributes{};
    attributes.Length = sizeof(attributes);
    attributes.RootDirectory = m_state->handle;
    attributes.ObjectName = &name;
    attributes.Attributes = OBJ_CASE_INSENSITIVE | kObjDontReparse;
    IO_STATUS_BLOCK io{};
    const NTSTATUS status = create(&state->handle, FILE_READ_DATA | FILE_WRITE_DATA | FILE_READ_ATTRIBUTES |
        FILE_WRITE_ATTRIBUTES | DELETE | SYNCHRONIZE,
        &attributes, &io, nullptr, FILE_ATTRIBUTE_TEMPORARY, 0, kFileCreate,
        kFileNonDirectory | kFileSynchronousNonalert | kFileOpenReparsePoint, nullptr, 0);
    if (status < 0 || state->handle == INVALID_HANDLE_VALUE || !state->handle) {
        state->handle = INVALID_HANDLE_VALUE;
        error = QStringLiteral("Temporary receive file could not be created (0x%1).")
                    .arg(static_cast<quint32>(status), 8, 16, QLatin1Char('0'));
        return {};
    }
    FILE_DISPOSITION_INFO disposition{TRUE};
    if (!SetFileInformationByHandle(state->handle, FileDispositionInfo, &disposition, sizeof(disposition)) ||
        !receivePathStillContained(*state)) {
        error = QStringLiteral("Temporary receive file could not be secured.");
        return {};
    }
    error.clear();
    return std::unique_ptr<ReceiveFile>(new ReceiveFile(std::move(state)));
}
