#include "diskstructures.h"
#include <QObject>

namespace DiskUtils {
QString raidTypeToString(RaidType type) {
    switch (type) {
        case RaidType::RAID0:
            return QObject::tr("RAID0");
        case RaidType::RAID1:
            return QObject::tr("RAID1");
        case RaidType::RAID5:
            return QObject::tr("RAID5");
        case RaidType::UNKNOWN:
        default:
            return QObject::tr("Unknown");
    }
}

QString deviceStatusToString(DeviceStatus status) {
    switch (status) {
        case DeviceStatus::NORMAL:
            return QObject::tr("Normal");
        case DeviceStatus::FAULTY:
            return QObject::tr("Faulty");
        case DeviceStatus::SPARE:
            return QObject::tr("Spare");
        case DeviceStatus::SYNCING:
            return QObject::tr("Syncing");
        case DeviceStatus::REBUILDING:
            return QObject::tr("Rebuilding");
        default:
            return QObject::tr("Unknown");
    }
}

QString formatByteSize(qint64 bytes) {
    constexpr qint64 KiB = 1024;
    constexpr qint64 MiB = 1024 * KiB;
    constexpr qint64 GiB = 1024 * MiB;
    constexpr qint64 TiB = 1024 * GiB;
    constexpr qint64 PiB = 1024 * TiB;

    if (bytes >= PiB) {
        return QObject::tr("%1 PiB").arg(bytes / static_cast<double>(PiB), 0, 'f', 2);
    } else if (bytes >= TiB) {
        return QObject::tr("%1 TiB").arg(bytes / static_cast<double>(TiB), 0, 'f', 2);
    } else if (bytes >= GiB) {
        return QObject::tr("%1 GiB").arg(bytes / static_cast<double>(GiB), 0, 'f', 2);
    } else if (bytes >= MiB) {
        return QObject::tr("%1 MiB").arg(bytes / static_cast<double>(MiB), 0, 'f', 2);
    } else if (bytes >= KiB) {
        return QObject::tr("%1 KiB").arg(bytes / static_cast<double>(KiB), 0, 'f', 2);
    } else {
        return QObject::tr("%1 B").arg(bytes);
    }
}

}
