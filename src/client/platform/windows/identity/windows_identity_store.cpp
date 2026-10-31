#include "identity/windows_identity_store.h"

#include "media/webrtc/media_runtime_library.h"
#include "security/secure_random.h"
#include "easydesk_media_abi.h"

#include <QSettings>

#include <windows.h>
#include <dpapi.h>

namespace {

constexpr auto kProtectedSeedKey = "identity/protectedSeedV1";
constexpr char kEntropy[] = "EasyDesk-Identity-v1";

DATA_BLOB blobFor(const QByteArray &bytes) {
    DATA_BLOB blob{};
    blob.cbData = static_cast<DWORD>(bytes.size());
    blob.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(bytes.constData()));
    return blob;
}

QString windowsErrorMessage(DWORD errorCode) {
    wchar_t *message = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, errorCode, 0, reinterpret_cast<wchar_t *>(&message), 0, nullptr);
    const QString result = length > 0 && message != nullptr
                               ? QString::fromWCharArray(message, static_cast<int>(length)).trimmed()
                               : QStringLiteral("Windows error %1").arg(errorCode);
    if (message != nullptr)
        LocalFree(message);
    return result;
}

}  // namespace

WindowsIdentityStore::WindowsIdentityStore(QSettings &settings, MediaRuntimeLibrary &crypto)
    : m_settings(settings), m_crypto(crypto) {}

WindowsIdentityStore::~WindowsIdentityStore() {
    clearPrivateKey();
}

bool WindowsIdentityStore::initialize() {
    clearPrivateKey();
    m_publicKey.clear();
    m_error.clear();
    if (!m_crypto.isReady()) {
        m_error = m_crypto.errorString().isEmpty()
                      ? QStringLiteral("Native identity runtime is unavailable.")
                      : m_crypto.errorString();
        return false;
    }

    QByteArray seed;
    const bool hasProtectedSeed = m_settings.contains(kProtectedSeedKey);
    const QByteArray protectedSeed = m_settings.value(kProtectedSeedKey).toByteArray();
    if (!hasProtectedSeed) {
        seed = createSeed();
        if (seed.isEmpty())
            return false;
        const QByteArray protectedValue = protectSeed(seed);
        if (protectedValue.isEmpty()) {
            SecureZeroMemory(seed.data(), static_cast<SIZE_T>(seed.size()));
            return false;
        }
        m_settings.setValue(kProtectedSeedKey, protectedValue);
        m_settings.sync();
        if (m_settings.status() != QSettings::NoError) {
            SecureZeroMemory(seed.data(), static_cast<SIZE_T>(seed.size()));
            m_error = QStringLiteral("Device identity could not be persisted.");
            return false;
        }
    } else {
        if (protectedSeed.isEmpty()) {
            m_error = QStringLiteral("Stored device identity is empty.");
            return false;
        }
        seed = unprotectSeed(protectedSeed);
        if (seed.isEmpty())
            return false;
    }

    const bool derived = m_crypto.deriveEd25519KeyPair(seed, m_publicKey, m_privateKey);
    SecureZeroMemory(seed.data(), static_cast<SIZE_T>(seed.size()));
    if (!derived) {
        m_error = m_crypto.errorString();
        clearPrivateKey();
        m_publicKey.clear();
        return false;
    }
    return true;
}

bool WindowsIdentityStore::isReady() const {
    return m_publicKey.size() == EASYDESK_IDENTITY_ED25519_PUBLIC_KEY_SIZE &&
           m_privateKey.size() == EASYDESK_IDENTITY_ED25519_PRIVATE_KEY_SIZE;
}

QByteArray WindowsIdentityStore::publicKey() const {
    return m_publicKey;
}

QByteArray WindowsIdentityStore::sign(const QByteArray &message) {
    if (!isReady()) {
        m_error = QStringLiteral("Device identity is unavailable.");
        return {};
    }
    const QByteArray signature = m_crypto.signEd25519(m_privateKey, message);
    if (signature.isEmpty())
        m_error = m_crypto.errorString();
    else
        m_error.clear();
    return signature;
}

QString WindowsIdentityStore::errorString() const {
    return m_error;
}

QByteArray WindowsIdentityStore::createSeed() {
    return easydesk::client::windows::generateSecureRandomBytes(
        static_cast<int>(EASYDESK_IDENTITY_ED25519_SEED_SIZE), m_error);
}

QByteArray WindowsIdentityStore::protectSeed(const QByteArray &seed) {
    DATA_BLOB input = blobFor(seed);
    const QByteArray entropyBytes(kEntropy, static_cast<int>(sizeof(kEntropy) - 1));
    DATA_BLOB entropy = blobFor(entropyBytes);
    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"EasyDesk device identity", &entropy, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        m_error = QStringLiteral("Device identity encryption failed: %1")
                      .arg(windowsErrorMessage(GetLastError()));
        return {};
    }
    const QByteArray protectedSeed(reinterpret_cast<const char *>(output.pbData),
                                   static_cast<int>(output.cbData));
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return protectedSeed;
}

QByteArray WindowsIdentityStore::unprotectSeed(const QByteArray &protectedSeed) {
    DATA_BLOB input = blobFor(protectedSeed);
    const QByteArray entropyBytes(kEntropy, static_cast<int>(sizeof(kEntropy) - 1));
    DATA_BLOB entropy = blobFor(entropyBytes);
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, &entropy, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        m_error = QStringLiteral("Stored device identity could not be decrypted: %1")
                      .arg(windowsErrorMessage(GetLastError()));
        return {};
    }
    QByteArray seed(reinterpret_cast<const char *>(output.pbData),
                    static_cast<int>(output.cbData));
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    if (seed.size() != EASYDESK_IDENTITY_ED25519_SEED_SIZE) {
        SecureZeroMemory(seed.data(), static_cast<SIZE_T>(seed.size()));
        seed.clear();
        m_error = QStringLiteral("Stored device identity has an invalid length.");
    }
    return seed;
}

void WindowsIdentityStore::clearPrivateKey() {
    if (!m_privateKey.isEmpty())
        SecureZeroMemory(m_privateKey.data(), static_cast<SIZE_T>(m_privateKey.size()));
    m_privateKey.clear();
}
