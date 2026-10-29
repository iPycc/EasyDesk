#include "core/device.h"

QString spacedId(const QString &id) {
    QString value = id;
    for (int i = value.size() - 3; i > 0; i -= 3)
        value.insert(i, ' ');
    return value;
}
