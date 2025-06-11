#ifndef DISKMANAGER_H
#define DISKMANAGER_H

#include <QObject>
#include <QDir>
#include "diskstructures.h"
#include "commandexecutor.h"

class DiskManager : public QObject
{
    Q_OBJECT
public:
    explicit DiskManager(QObject *parent = nullptr);
    ~DiskManager();
    void refreshDevices();
    void createPartitionTable(const QString &devicePath, const QString &tableType);
    void createPartition(const QString &devicePath, const QString &partitionType,
                        const QString &filesystemType, const QString &startSize,
                        const QString &endSize);
    void deletePartition(const QString &partitionPath);
    void formatPartition(const QString &partitionPath, const QString &filesystemType, const QString &label);
    void getFreeSpaceOnDeviceInfo(const QString &devicePath);

    // Новые методы для монтирования/размонтирования
    void mountDevice(const QString &devicePath, const QString &mountPoint, const QString &options = QString());
    void unmountDevice(const QString &devicePath);
    bool isDeviceMounted(const QString &devicePath) const;
    QString getMountPoint(const QString &devicePath) const;

    QString getFormatCommand(const QString &filesystemType) const;
    //DiskStructure getDiskStructure() const { return m_diskStructure; }
    const DiskStructure& getDiskStructure() const { return m_diskStructure; }
    QString getDeviceFilesystem(const QString &devicePath) const;

    void createRaidArray(RaidType raidType, const QStringList &devices, const QString &arrayName = QString());
    void deleteRaidArray(const QString &raidDevice, const QStringList &memberDevices);
    void wipeDevice(const QString &devicePath);

signals:
    void devicesRefreshed(bool success);
    void commandOutput(const QString &output);
    void partitionTableCreated(bool success);
    void partitionCreated(bool success);
    void partitionDeleted(bool success);
    void partitionFormatted(bool success);
    void freeSpaceOnDeviceInfoReceived(const QString &freeSpaceInfo);

    void deviceMounted(bool success, const QString &devicePath, const QString &mountPoint);
    void deviceUnmounted(bool success, const QString &devicePath);

    void raidCreationStarted();
    void deviceWipeCompleted(const QString &device, bool success);
    void raidCreationProgress(const QString &message);
    void raidCreationCompleted(bool success, const QString &raidDevice);

    void raidStopProgress(const QString &message);
    void raidStopCompleted(bool success, const QString &raidDevice);
    void superblockCleanProgress(const QString &device, bool success);
    void raidDeletionCompleted(bool success, const QString &raidDevice);

private slots:
    void handleCommandFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleCommandOutput(const QString &output);
    void handleCommandErrorOutput(const QString &error);

private:
    // Тип текущей выполняемой команды
    enum class CommandType {
        NONE,
        LSBLK,
        MDADM_SCAN,
        MDADM_DETAIL,
        CREATE_PARTITION_TABLE,
        CREATE_PARTITION,
        DELETE_PARTITION,
        GET_FREE_SPACE_ON_DEVICE,
        FORMAT_PARTITION,
        MOUNT_DEVICE,
        UNMOUNT_DEVICE,
        WIPE_DEVICE,
        CREATE_RAID_ARRAY,
        STOP_RAID_ARRAY,
        CLEAN_SUPERBLOCK,
        DELETE_RAID_ARRAY
    };

    enum class WipeContext {
        RAID_CREATION,
        RAID_DELETION
    };

    CommandType m_currentCommand = CommandType::NONE;
    WipeContext m_currentWipeContext;
    bool m_isDeletingRaid;
    int m_commandsCompleted = 0;
    QStringList m_raidDetailsToCheck; // Список путей к RAID-массивам для детального анализа
    bool m_hasDevicePartitionsLine;

    // Переменные для операций монтирования
    QString m_currentMountDevice;
    QString m_currentMountPoint;

    QStringList m_devicesToWipe;
    QStringList m_wipedDevices;
    RaidType m_raidType;
    QString m_raidArrayName;
    QString m_currentWipeDevice;
    QString m_createdRaidDevice;

    QStringList m_devicesToCleanSuperblock;
    QStringList m_cleanedSuperblockDevices;
    QString m_raidToDelete;
    QString m_currentSuperblockDevice;

    void parseMdadmScanOutput(const QString &output);
    void parseMdadmDetailOutput(const QString &output);
    CommandExecutor *m_commandExecutor; // Исполнитель команд
    DiskStructure m_diskStructure;      // Структура дисков
    void parseLsblkOutput(const QString &output);
    void parseDfOutput(const QString &output);
    qint64 sizeStringToBytes(const QString &sizeStr);
    void parsePartitionFromJson(const QJsonObject &partObj, DiskInfo &disk);
    void markPartitionAsRaidMember(const QString &devicePath);

    bool validateMountOperation(const QString &devicePath, const QString &mountPoint, QString &errorMessage);
    bool validateUnmountOperation(const QString &devicePath, QString &errorMessage);
    bool addToFstab(const QString &devicePath, const QString &mountPoint, const QString &filesystem, const QString &options);
    bool removeFromFstab(const QString &devicePath);
    bool createMountPoint(const QString &mountPoint);

    void processNextWipeOperation();
    void processAllWipeOperationsSync();
    void executeRaidCreation();
    QString generateRaidDeviceName() const;
    QString raidTypeToMdadmLevel(RaidType type) const;
    bool checkDevicePartitionsInMdadmConf();
    bool addDevicePartitionsToMdadmConf();
    bool addRaidToMdadmConf(const QString &raidDevice);
    void ensureMdadmConfSetup();
    bool updateInitramfs();
    bool removeRaidFromMdadmConf(const QString &raidDevice);
    void finishRaidDeletion();
    void updateRaidFilesystemInfo(RaidInfo *raidInfo);
    void stopRaidArray();
    void processNextSuperblockClean();

};

#endif // DISKMANAGER_H
