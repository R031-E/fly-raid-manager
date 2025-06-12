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

qint64 parseSizeString(const QString &sizeStr) {
    if (sizeStr.isEmpty() || sizeStr == "-" || sizeStr.isNull()) {
        return 0;
    }

    // Регулярное выражение для парсинга размеров
    // lsblk выводит размеры в бинарных единицах: "1.5T", "500G", "2048M", "1024K", "512B"
    // Поддерживаем форматы с десятичными разделителями (точка и запятая)
    QRegularExpression rx(R"((\d+(?:[.,]\d+)?)\s*([KMGTPE]?)i?B?)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = rx.match(sizeStr.trimmed());

    if (!match.hasMatch()) {
        // Попробуем парсить только число (предполагаем кибибайты для RAID)
        bool ok;
        qint64 numberOnly = sizeStr.toLongLong(&ok);
        if (ok) {
            // Если единица не указана, предполагаем кибибайты (KiB)
            return numberOnly * 1024;
        }
        return 0;
    }

    QString numberStr = match.captured(1);
    QString unit = match.captured(2).toUpper();

    // Заменяем запятую на точку для корректного парсинга
    numberStr.replace(',', '.');

    bool ok;
    double size = numberStr.toDouble(&ok);
    if (!ok) {
        return 0;
    }

    // Преобразуем в байты на основе единицы измерения
    qint64 result = 0;

    if (unit.isEmpty()) {
        // Если единица измерения не указана, предполагаем кибибайты (KiB)
        result = static_cast<qint64>(size * 1024);
    } else if (unit == "B") {
        result = static_cast<qint64>(size);
    } else if (unit == "K") {
        // lsblk: K = 1024 байт (бинарный килобайт = KiB)
        result = static_cast<qint64>(size * 1024);
    } else if (unit == "M") {
        // lsblk: M = 1024^2 байт (бинарный мегабайт = MiB)
        result = static_cast<qint64>(size * 1024 * 1024);
    } else if (unit == "G") {
        // lsblk: G = 1024^3 байт (бинарный гигабайт = GiB)
        result = static_cast<qint64>(size * 1024LL * 1024LL * 1024LL);
    } else if (unit == "T") {
        // lsblk: T = 1024^4 байт (бинарный терабайт = TiB)
        result = static_cast<qint64>(size * 1024LL * 1024LL * 1024LL * 1024LL);
    } else if (unit == "P") {
        // lsblk: P = 1024^5 байт (бинарный петабайт = PiB)
        result = static_cast<qint64>(size * 1024LL * 1024LL * 1024LL * 1024LL * 1024LL);
    } else if (unit == "E") {
        // lsblk: E = 1024^6 байт (бинарный эксабайт = EiB)
        result = static_cast<qint64>(size * 1024LL * 1024LL * 1024LL * 1024LL * 1024LL * 1024LL);
    } else {
        // Неизвестная единица измерения, предполагаем кибибайты
        result = static_cast<qint64>(size * 1024);
    }
    return result;
}
}
