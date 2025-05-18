#ifndef DISKSTRUCTURES_H
#define DISKSTRUCTURES_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>

enum class RaidType {
    UNKNOWN,
    RAID0,
    RAID1,
    RAID5
};

enum class DeviceStatus {
    NORMAL,  // Нормальное состояние
    FAULTY,  // Неисправность
    SPARE,   // Запасное устройство
    SYNCING, //Сихронизация
    REBUILDING // Восстановление
};

struct RaidMemberInfo {
    QString devicePath;        // Путь к устройству (/dev/sda1)
    DeviceStatus status;       // Статус устройства в массиве

    RaidMemberInfo(const QString &path = QString(), DeviceStatus st = DeviceStatus::NORMAL)
        : devicePath(path), status(st) {}
};


struct RaidInfo {
    QString devicePath;        // Путь к устройству (/dev/md0)
    RaidType type;             // Тип RAID
    QString state;             // Состояние (active, degraded, etc)
    QString size;              // Размер
    QString mountPoint;        // Точка монтирования
    QString filesystem;        // Файловая система
    int syncPercent;           // Процент синхронизации
    QVector<RaidMemberInfo> members; // Устройства в массиве

    RaidInfo() : type(RaidType::UNKNOWN), syncPercent(100) {}
};


struct PartitionInfo {
    QString devicePath;        // Путь к устройству (/dev/sda1)
    QString size;              // Размер
    QString used;              // Используемое пространство
    QString available;         // Свободное пространство
    QString filesystem;        // Файловая система
    QString mountPoint;        // Точка монтирования
    QStringList flags;         // Флаги раздела
    bool isRaidMember;         // Является ли частью RAID

    PartitionInfo() : isRaidMember(false) {}
};

struct DiskInfo {
    QString devicePath;        // Путь к устройству (/dev/sda)
    QString model;             // Модель
    QString size;              // Общий размер
    bool hasPartTable;         // Имеет ли таблицу разделов
    QVector<PartitionInfo> partitions; // Разделы на диске

    DiskInfo() : hasPartTable(false) {}
};

class DiskStructure {
public:
    // Получение всех дисков в системе
    QVector<DiskInfo> getDisks() const { return disks; }

    // Получение всех RAID-массивов в системе
    QVector<RaidInfo> getRaids() const { return raids; }

    // Добавление диска
    void addDisk(const DiskInfo &disk) { disks.append(disk); }

    // Добавление RAID-массива
    void addRaid(const RaidInfo &raid) { raids.append(raid); }

    // Очистка всех данных
    void clear() { disks.clear(); raids.clear(); }

private:
    QVector<DiskInfo> disks;  // Все физические диски
    QVector<RaidInfo> raids;  // Все RAID-массивы
};


namespace DiskUtils {
    // Получение строкового представления типа RAID
    QString raidTypeToString(RaidType type);

    // Получение строкового представления статуса устройства
    QString deviceStatusToString(DeviceStatus status);

    // Форматирование размера в читаемый вид
    QString formatByteSize(qint64 bytes);
}

#endif // DISKSTRUCTURES_H
