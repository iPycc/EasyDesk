#include "features/devices/device_service.h"

const Device *DeviceService::find(const QString &id) const {
    for (const auto &device : devices())
        if (device.id == id)
            return &device;
    return nullptr;
}
