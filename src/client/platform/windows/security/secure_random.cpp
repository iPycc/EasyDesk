#include "security/secure_random.h"

#include <windows.h>
#include <bcrypt.h>

namespace easydesk::client::windows {

QByteArray generateSecureRandomBytes(int size, QString &error) {
    if (size <= 0) {
        error = QStringLiteral("Secure random byte count must be positive.");
        return {};
    }

    QByteArray output(size, '\0');
    const NTSTATUS status = BCryptGenRandom(
        nullptr, reinterpret_cast<PUCHAR>(output.data()), static_cast<ULONG>(output.size()),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status < 0) {
        output.fill('\0');
        output.clear();
        error = QStringLiteral("Secure random generation failed (0x%1).")
                    .arg(static_cast<quint32>(status), 8, 16, QLatin1Char('0'));
        return {};
    }
    error.clear();
    return output;
}

}  // namespace easydesk::client::windows
