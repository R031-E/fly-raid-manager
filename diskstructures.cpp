#include "diskstructures.h"

namespace DiskUtils {

QString raidTypeToString(RaidType type) {
    switch (type) {
        case RaidType::RAID0:
            return "RAID0";
        case RaidType::RAID1:
            return "RAID1";
        case RaidType::RAID5:
            return "RAID5";
        case RaidType::UNKNOWN:
        default:
            return "Unknown";
    }
}

QString deviceStatusToString(DeviceStatus status) {
    switch (status) {
        case DeviceStatus::NORMAL:
            return "Normal";
        case DeviceStatus::FAULTY:
            return "Faulty";
        case DeviceStatus::SPARE:
            return "Spare";
        case DeviceStatus::REBUILDING:
            return "Rebuilding";
        default:
            return "Unknown";
    }
}

QString formatByteSize(qint64 bytes) {
    constexpr qint64 KB = 1024;
    constexpr qint64 MB = 1024 * KB;
    constexpr qint64 GB = 1024 * MB;
    constexpr qint64 TB = 1024 * GB;

    if (bytes >= TB) {
        return QString("%1 TiB").arg(bytes / static_cast<double>(TB), 0, 'f', 2);
    } else if (bytes >= GB) {
        return QString("%1 GiB").arg(bytes / static_cast<double>(GB), 0, 'f', 2);
    } else if (bytes >= MB) {
        return QString("%1 MiB").arg(bytes / static_cast<double>(MB), 0, 'f', 2);
    } else if (bytes >= KB) {
        return QString("%1 KiB").arg(bytes / static_cast<double>(KB), 0, 'f', 2);
    } else {
        return QString("%1 B").arg(bytes);
    }
}

}
