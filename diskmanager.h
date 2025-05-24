#ifndef DISKMANAGER_H
#define DISKMANAGER_H

#include <QObject>
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
    void getFreeSpaceOnDeviceInfo(const QString &devicePath);
    DiskStructure getDiskStructure() const { return m_diskStructure; }

signals:
    void devicesRefreshed(bool success);
    void commandOutput(const QString &output);
    void partitionTableCreated(bool success);
    void partitionCreated(bool success);
    void partitionDeleted(bool success);
    void partitionFormatted(bool success);
    void freeSpaceOnDeviceInfoReceived(const QString &freeSpaceInfo);

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
        GET_FREE_SPACE_ON_DEVICE
    };

    CommandType m_currentCommand = CommandType::NONE;
    int m_commandsCompleted = 0;
    QStringList m_raidDetailsToCheck; // Список путей к RAID-массивам для детального анализа

    void parseMdadmScanOutput(const QString &output);
    void parseMdadmDetailOutput(const QString &output);
    CommandExecutor *m_commandExecutor; // Исполнитель команд
    DiskStructure m_diskStructure;      // Структура дисков
    void parseLsblkOutput(const QString &output);
    //void parseMdadmOutput(const QString &output);
    void parseDfOutput(const QString &output);
    qint64 sizeStringToBytes(const QString &sizeStr);
};

#endif // DISKMANAGER_H
