#include "devices/device_repository.hpp"

#include "security/ed25519.hpp"
#include "storage/sqlite_database.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string_view>

namespace easydesk::devices {
namespace {

bool isLocatorId(std::string_view value) {
    return value.size() == 9 &&
           std::all_of(value.begin(), value.end(), [](unsigned char character) {
               return std::isdigit(character) != 0;
           });
}

void validate(const DeviceRegistration& registration) {
    if (!isLocatorId(registration.locatorId)) {
        throw std::invalid_argument("locator ID must contain exactly 9 digits");
    }
    if (registration.publicKey.size() != 32) {
        throw std::invalid_argument("Ed25519 public key must contain 32 bytes");
    }
    if (registration.displayName.empty() || registration.displayName.size() > 128) {
        throw std::invalid_argument("display name length is invalid");
    }
    if (registration.platform.empty() || registration.platform.size() > 64) {
        throw std::invalid_argument("platform length is invalid");
    }
    if (registration.schemaVersion < 1 || registration.schemaVersion > 65535) {
        throw std::invalid_argument("schema version is invalid");
    }
    if (registration.registeredAt <= 0) {
        throw std::invalid_argument("registration timestamp is invalid");
    }
}

}  // namespace

DeviceRepository::DeviceRepository(storage::SqliteDatabase& database) : database_(database) {}

void DeviceRepository::migrate() {
    auto version = database_.prepare("PRAGMA user_version;");
    if (!version.step()) {
        throw std::runtime_error("database did not return a schema version");
    }
    const int currentVersion = version.columnInt(0);
    if (currentVersion > 1) {
        throw std::runtime_error("database schema is newer than this server");
    }
    if (currentVersion == 1) {
        return;
    }

    database_.execute("BEGIN IMMEDIATE;");
    try {
        database_.execute(R"SQL(
            CREATE TABLE devices (
                fingerprint TEXT PRIMARY KEY NOT NULL,
                locator_id TEXT NOT NULL UNIQUE CHECK(length(locator_id) = 9),
                public_key BLOB NOT NULL CHECK(length(public_key) = 32),
                display_name TEXT NOT NULL,
                platform TEXT NOT NULL,
                schema_version INTEGER NOT NULL,
                created_at INTEGER NOT NULL,
                last_registered_at INTEGER NOT NULL
            );
            PRAGMA user_version = 1;
        )SQL");
        database_.execute("COMMIT;");
    } catch (...) {
        try {
            database_.execute("ROLLBACK;");
        } catch (...) {
        }
        throw;
    }
}

RegistrationResult DeviceRepository::registerDevice(const DeviceRegistration& registration) {
    validate(registration);
    const std::string fingerprint = security::sha256Fingerprint(registration.publicKey);

    database_.execute("BEGIN IMMEDIATE;");
    try {
        auto byLocator = database_.prepare(
            "SELECT public_key, fingerprint FROM devices WHERE locator_id = ?1;");
        byLocator.bindText(1, registration.locatorId);
        if (byLocator.step()) {
            if (byLocator.columnBlob(0) !=
                std::vector<std::uint8_t>(registration.publicKey.begin(), registration.publicKey.end())) {
                database_.execute("ROLLBACK;");
                return RegistrationResult::locatorCollision;
            }

            auto update = database_.prepare(R"SQL(
                UPDATE devices
                   SET display_name = ?1, platform = ?2, schema_version = ?3,
                       last_registered_at = ?4
                 WHERE fingerprint = ?5;
            )SQL");
            update.bindText(1, registration.displayName);
            update.bindText(2, registration.platform);
            update.bindInt(3, registration.schemaVersion);
            update.bindInt64(4, registration.registeredAt);
            update.bindText(5, fingerprint);
            update.step();
            database_.execute("COMMIT;");
            return RegistrationResult::refreshed;
        }

        auto byIdentity =
            database_.prepare("SELECT locator_id FROM devices WHERE fingerprint = ?1;");
        byIdentity.bindText(1, fingerprint);
        if (byIdentity.step()) {
            database_.execute("ROLLBACK;");
            return RegistrationResult::identityConflict;
        }

        auto insert = database_.prepare(R"SQL(
            INSERT INTO devices (
                fingerprint, locator_id, public_key, display_name, platform, schema_version,
                created_at, last_registered_at
            ) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?7);
        )SQL");
        insert.bindText(1, fingerprint);
        insert.bindText(2, registration.locatorId);
        insert.bindBlob(3, registration.publicKey);
        insert.bindText(4, registration.displayName);
        insert.bindText(5, registration.platform);
        insert.bindInt(6, registration.schemaVersion);
        insert.bindInt64(7, registration.registeredAt);
        insert.step();
        database_.execute("COMMIT;");
        return RegistrationResult::created;
    } catch (...) {
        try {
            database_.execute("ROLLBACK;");
        } catch (...) {
        }
        throw;
    }
}

std::int64_t DeviceRepository::deviceCount() {
    auto count = database_.prepare("SELECT COUNT(*) FROM devices;");
    if (!count.step()) {
        throw std::runtime_error("database did not return a device count");
    }
    return count.columnInt64(0);
}

}  // namespace easydesk::devices
