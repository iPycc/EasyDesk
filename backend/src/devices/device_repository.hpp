#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace easydesk::storage {
class SqliteDatabase;
}

namespace easydesk::devices {

struct DeviceRegistration {
    std::string locatorId;
    std::span<const std::uint8_t> publicKey;
    std::string displayName;
    std::string platform;
    int schemaVersion;
    std::int64_t registeredAt;
};

enum class RegistrationResult {
    created,
    refreshed,
    locatorCollision,
    identityConflict,
};

class DeviceRepository final {
public:
    explicit DeviceRepository(storage::SqliteDatabase& database);

    void migrate();
    RegistrationResult registerDevice(const DeviceRegistration& registration);
    std::int64_t deviceCount();

private:
    storage::SqliteDatabase& database_;
};

}  // namespace easydesk::devices
