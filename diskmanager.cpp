#include "diskmanager.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

DiskManager::DiskManager(QObject *parent) : QObject(parent)
{
    m_commandExecutor = new CommandExecutor(this);
    m_currentCommand = CommandType::NONE;

    // Подключаем сигналы CommandExecutor
    connect(m_commandExecutor, &CommandExecutor::outputAvailable,
            this, &DiskManager::handleCommandOutput);
    connect(m_commandExecutor, &CommandExecutor::errorOutputAvailable,
            this, &DiskManager::handleCommandErrorOutput);
    connect(m_commandExecutor, &CommandExecutor::finished,
            this, &DiskManager::handleCommandFinished);
}

DiskManager::~DiskManager()
{
}

void DiskManager::refreshDevices()
{
    m_diskStructure.clear();
    m_commandsCompleted = 0;
    m_raidDetailsToCheck.clear();

    // Запускаем lsblk для получения информации о дисках и разделах
    QStringList args;
    args << "-o" << "NAME,SIZE,TYPE,MOUNTPOINT,FSTYPE,MODEL,LABEL" << "-J"; // JSON формат

    m_currentCommand = CommandType::LSBLK;
    if (!m_commandExecutor->execute("lsblk", args)) {
        qWarning() << "Failed to run lsblk command";
        emit devicesRefreshed(false);
    }
}

void DiskManager::handleCommandOutput(const QString &output)
{
    // Передаем вывод для отображения
    emit commandOutput(output);

    // Парсим вывод команды в зависимости от текущей команды
    switch (m_currentCommand) {
    case CommandType::LSBLK:
        // Это вывод lsblk в формате JSON
        parseLsblkOutput(output);
        break;
    case CommandType::MDADM_SCAN:
        // Это вывод mdadm --detail --scan
        parseMdadmScanOutput(output);
        break;
    case CommandType::MDADM_DETAIL:
        // Это вывод mdadm --detail /dev/mdX
        parseMdadmDetailOutput(output);
        break;
    case CommandType::GET_FREE_SPACE_ON_DEVICE:
        // Это вывод parted print free
        emit freeSpaceOnDeviceInfoReceived(output);
        break;
    default:
        break;
    }
}

void DiskManager::handleCommandErrorOutput(const QString &error)
{
    // Просто передаем ошибку для отображения
    emit commandOutput(tr("ERROR: %1").arg(error));
}

void DiskManager::handleCommandFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_commandsCompleted++;

    if (exitCode != 0 || exitStatus != QProcess::NormalExit) {
        qDebug() << "Command failed:" << static_cast<int>(m_currentCommand);

        // Если это lsblk, то мы не можем продолжать
        if (m_currentCommand == CommandType::LSBLK) {
            qDebug() << "devicesRefreshed = false";
            emit devicesRefreshed(false);
            return;
        }
    }

    // Определяем следующую команду на основе текущей
    switch (m_currentCommand) {
    case CommandType::LSBLK:
        // После lsblk запускаем mdadm --detail --scan
        {
            QStringList mdadmArgs;
            mdadmArgs << "--detail" << "--scan";

            m_currentCommand = CommandType::MDADM_SCAN;
            if (!m_commandExecutor->executeAsAdmin("mdadm", mdadmArgs)) {
                qDebug() << "Failed to run mdadm scan command";
                emit devicesRefreshed(true); // Считаем успехом, т.к. основная информация получена
            }
        }
        break;

    case CommandType::MDADM_SCAN:
        // После mdadm scan запускаем mdadm --detail для каждого RAID
        if (!m_raidDetailsToCheck.isEmpty()) {
            QString raidPath = m_raidDetailsToCheck.takeFirst();
            QStringList detailArgs;
            detailArgs << "--detail" << raidPath;

            m_currentCommand = CommandType::MDADM_DETAIL;
            if (!m_commandExecutor->executeAsAdmin("mdadm", detailArgs)) {
                qDebug() << "Failed to run mdadm detail command for" << raidPath;
                // Продолжаем с оставшимися RAID-массивами
                if (!m_raidDetailsToCheck.isEmpty()) {
                    handleCommandFinished(0, QProcess::NormalExit);
                } else {
                    emit devicesRefreshed(true);
                }
            }
        } else {
            // Все готово
            emit devicesRefreshed(true);
        }
        break;

    case CommandType::MDADM_DETAIL:
        // После mdadm detail проверяем, есть ли еще RAID для проверки
        if (!m_raidDetailsToCheck.isEmpty()) {
            QString raidPath = m_raidDetailsToCheck.takeFirst();
            QStringList detailArgs;
            detailArgs << "--detail" << raidPath;

            m_currentCommand = CommandType::MDADM_DETAIL;
            if (!m_commandExecutor->executeAsAdmin("mdadm", detailArgs)) {
                qDebug() << "Failed to run mdadm detail command for" << raidPath;
                // Продолжаем с оставшимися RAID-массивами
                if (!m_raidDetailsToCheck.isEmpty()) {
                    handleCommandFinished(0, QProcess::NormalExit);
                } else {
                    emit devicesRefreshed(true);
                }
            }
        } else {
            // Все готово
            emit devicesRefreshed(true);
        }
        break;

    case CommandType::CREATE_PARTITION_TABLE:
        emit partitionTableCreated(exitCode == 0 && exitStatus == QProcess::NormalExit);
        emit commandOutput(tr("parted created partition table"));
        refreshDevices();
        break;

    case CommandType::CREATE_PARTITION:
        // После создания раздела обновляем список устройств
        emit partitionCreated(exitCode == 0 && exitStatus == QProcess::NormalExit);
        emit commandOutput(tr("parted created partition"));
        refreshDevices();
        break;

    case CommandType::DELETE_PARTITION:
        // После удаления раздела обновляем список устройств
        emit partitionDeleted(exitCode == 0 && exitStatus == QProcess::NormalExit);
        if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
            emit commandOutput(tr("Partition deleted successfully"));
        } else {
            emit commandOutput(tr("Failed to delete partition"));
        }
        refreshDevices();
        break;

    case CommandType::GET_FREE_SPACE_ON_DEVICE:
        break;

    default:
        emit devicesRefreshed(true);
        break;
    }
}

void DiskManager::createPartitionTable(const QString &devicePath, const QString &tableType)
{
    QStringList args;
    args << "-s";
    args << devicePath;
    args << "mklabel";
    args << tableType;

    // Устанавливаем тип команды
    m_currentCommand = CommandType::CREATE_PARTITION_TABLE;

    // Запускаем parted с правами администратора
    if (!m_commandExecutor->executeAsAdmin("parted", args)) {
        qWarning() << "Failed to run parted command for creating partition table";
        emit partitionTableCreated(false);
    }
}

void DiskManager::createPartition(const QString &devicePath, const QString &partitionType,
                                 const QString &filesystemType, const QString &startSize,
                                 const QString &endSize)
{
    // Создаем команду для parted с единицами МиБ
    QStringList args;
    args << "-s"; // режим без вопросов
    args << devicePath;
    args << "mkpart";
    args << partitionType;

    // Добавляем тип файловой системы только если он указан
    if (!filesystemType.isEmpty() && filesystemType != "unformatted") {
        args << filesystemType;
    }

    args << startSize;
    args << endSize;

    // Устанавливаем тип команды
    m_currentCommand = CommandType::CREATE_PARTITION;

    // Запускаем parted с правами администратора
    if (!m_commandExecutor->executeAsAdmin("parted", args)) {
        qWarning() << "Failed to run parted command for creating partition";
        emit partitionCreated(false);
    }
}

void DiskManager::deletePartition(const QString &partitionPath)
{
    // Извлекаем номер раздела из пути (например /dev/sda1 -> 1)
    QRegularExpression partitionRegex(".*?([0-9]+)$");
    QRegularExpressionMatch match = partitionRegex.match(partitionPath);

    if (!match.hasMatch()) {
        qWarning() << "Invalid partition path:" << partitionPath;
        emit partitionDeleted(false);
        return;
    }

    QString partitionNumber = match.captured(1);

    // Извлекаем путь к диску (убираем номер раздела)
    QString diskPath = partitionPath;
    diskPath.remove(QRegularExpression("[0-9]+$"));

    // Создаем команду для parted
    QStringList args;
    args << "-s"; // режим без вопросов
    args << diskPath;
    args << "rm"; // команда удаления
    args << partitionNumber;

    // Устанавливаем тип команды
    m_currentCommand = CommandType::DELETE_PARTITION;

    qDebug() << "Deleting partition:" << partitionNumber << "from disk:" << diskPath;

    // Запускаем parted с правами администратора
    if (!m_commandExecutor->executeAsAdmin("parted", args)) {
        qWarning() << "Failed to run parted command for deleting partition";
        emit partitionDeleted(false);
    }
}

void DiskManager::getFreeSpaceOnDeviceInfo(const QString &devicePath)
{
    // Создаем команду для получения информации о разделах в МиБ
    QStringList args;
    args << "-s";
    args << devicePath;
    args << "unit" << "MiB";  // Указываем единицы измерения сразу в МиБ
    args << "print";
    args << "free";

    // Устанавливаем тип команды
    m_currentCommand = CommandType::GET_FREE_SPACE_ON_DEVICE;

    // Запускаем parted с правами администратора
    if (!m_commandExecutor->executeAsAdmin("parted", args)) {
        qDebug() << "Failed to run parted command for getting free space info";
    }
}

void DiskManager::parseLsblkOutput(const QString &output)
{
    // Парсим JSON вывод lsblk
    QJsonDocument jsonDoc = QJsonDocument::fromJson(output.toUtf8());
    if (jsonDoc.isNull()) {
        qWarning() << "Failed to parse lsblk JSON output";
        return;
    }

    QJsonObject rootObj = jsonDoc.object();
    QJsonArray blockDevices = rootObj["blockdevices"].toArray();

    // Перебираем все блочные устройства
    for (const QJsonValue &deviceValue : blockDevices) {
        QJsonObject deviceObj = deviceValue.toObject();

        QString name = deviceObj["name"].toString();
        QString type = deviceObj["type"].toString();

        // Обрабатываем только диски
        if (type == "disk") {
            DiskInfo disk;
            disk.devicePath = "/dev/" + name;
            disk.model = deviceObj["model"].toString();
            disk.size = deviceObj["size"].toString();

            // Проверяем, есть ли у диска разделы
            if (deviceObj.contains("children")) {
                disk.hasPartTable = true;
                QJsonArray children = deviceObj["children"].toArray();

                // Обрабатываем разделы
                for (const QJsonValue &partValue : children) {
                    QJsonObject partObj = partValue.toObject();

                    QString partName = partObj["name"].toString();
                    QString partType = partObj["type"].toString();

                    if (partType == "part") {
                        PartitionInfo partition;
                        partition.devicePath = "/dev/" + partName;
                        partition.size = partObj["size"].toString();
                        partition.filesystem = partObj["fstype"].toString();
                        partition.mountPoint = partObj["mountpoint"].toString();

                        // Добавляем раздел к диску
                        disk.partitions.append(partition);
                    }
                }
            }

            // Добавляем диск в структуру
            m_diskStructure.addDisk(disk);
        }
    }
}

void DiskManager::parseMdadmScanOutput(const QString &output)
{
    // Парсим вывод mdadm --detail --scan
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    qDebug() << "mdadm scan: " << lines;
    for (const QString &line : lines) {
        if (line.startsWith("ARRAY")) {
            // Найден RAID-массив
            RaidInfo raid;

            // Парсим строку с информацией о RAID
            QRegularExpression rxDevice("/dev/md\\d+");
            QRegularExpressionMatch match = rxDevice.match(line);
            if (match.hasMatch()) {
                raid.devicePath = match.captured(0);
                // Добавляем этот RAID в список для получения детальной информации
                m_raidDetailsToCheck.append(raid.devicePath);
            }

            QRegularExpression rxLevel("level=([\\w\\d]+)");
            match = rxLevel.match(line);
            if (match.hasMatch()) {
                QString levelStr = match.captured(1);
                if (levelStr == "raid0" || levelStr == "0") {
                    raid.type = RaidType::RAID0;
                } else if (levelStr == "raid1" || levelStr == "1") {
                    raid.type = RaidType::RAID1;
                } else if (levelStr == "raid5" || levelStr == "5") {
                    raid.type = RaidType::RAID5;
                }
            }

            // По умолчанию считаем активным
            raid.state = tr("active");

            // Добавляем RAID в структуру
            m_diskStructure.addRaid(raid);
        }
    }
}

void DiskManager::parseMdadmDetailOutput(const QString &output)
{
    // Парсим вывод mdadm --detail /dev/mdX
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    QString currentRaidPath;
    int i = 0;

    // Найдем путь к RAID-массиву
    for (; i < lines.size(); i++) {
        if (lines[i].contains("/dev/md")) {
            QRegularExpression rxDevice("/dev/md\\d+");
            QRegularExpressionMatch match = rxDevice.match(lines[i]);
            if (match.hasMatch()) {
                currentRaidPath = match.captured(0);
                break;
            }
        }
    }

    if (currentRaidPath.isEmpty()) {
        return;
    }

    // Найдем RAID в нашей структуре
    for (int j = 0; j < m_diskStructure.getRaids().size(); j++) {
        RaidInfo &raid = m_diskStructure.getRaids()[j];
        if (raid.devicePath == currentRaidPath) {
            // Обновим информацию о RAID

            // Ищем состояние
            for (; i < lines.size(); i++) {
                if (lines[i].contains("State :")) {
                    raid.state = lines[i].split(':').last().trimmed();
                    break;
                }
            }

            // Ищем размер
            for (; i < lines.size(); i++) {
                if (lines[i].contains("Array Size :")) {
                    raid.size = lines[i].split(':').last().trimmed();
                    break;
                }
            }

            // Ищем устройства
            for (; i < lines.size(); i++) {
                if (lines[i].contains("Number   Major   Minor   RaidDevice State")) {
                    i++; // Переходим к первому устройству
                    break;
                }
            }

            // Парсим устройства
            for (; i < lines.size(); i++) {
                QString line = lines[i].trimmed();
                if (line.isEmpty() || !line[0].isDigit()) {
                    break;
                }

                QStringList parts = line.split(QRegularExpression("\\s+"));
                if (parts.size() >= 6) {
                    RaidMemberInfo member;
                    member.devicePath = "/dev/" + parts[5]; // Имя устройства обычно в 6-й колонке

                    // Определим статус
                    QString status = parts.last();
                    if (status.contains("active")) {
                        member.status = DeviceStatus::NORMAL;
                    } else if (status.contains("faulty")) {
                        member.status = DeviceStatus::FAULTY;
                    } else if (status.contains("spare")) {
                        member.status = DeviceStatus::SPARE;
                    } else if (status.contains("rebuilding")) {
                        member.status = DeviceStatus::REBUILDING;
                    } else if (status.contains("sync")) {
                        member.status = DeviceStatus::SYNCING;
                    }

                    raid.members.append(member);
                }
            }

            break;
        }
    }
}

void DiskManager::parseDfOutput(const QString &output)
{
    // В базовой версии эта функция пока не используется
    // В будущем здесь будет парсинг вывода df для получения информации
    // об использованном и свободном пространстве
}

qint64 DiskManager::sizeStringToBytes(const QString &sizeStr)
{
    // Конвертация строки размера в байты (поддержка бинарных единиц)
    QRegularExpression rx("(\\d+(?:\\.\\d+)?)\\s*([KMGTPE]i?B?)?", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = rx.match(sizeStr.trimmed());

    if (!match.hasMatch()) {
        return 0;
    }

    double size = match.captured(1).toDouble();
    QString unit = match.captured(2).toUpper();

    // Поддержка как бинарных (Ki, Mi, Gi, Ti, Pi), так и десятичных (K, M, G, T, P) единиц
    if (unit == "KIB" || unit == "KI" || unit == "K") {
        return static_cast<qint64>(size * 1024);
    } else if (unit == "MIB" || unit == "MI" || unit == "M") {
        return static_cast<qint64>(size * 1024 * 1024);
    } else if (unit == "GIB" || unit == "GI" || unit == "G") {
        return static_cast<qint64>(size * 1024 * 1024 * 1024);
    } else if (unit == "TIB" || unit == "TI" || unit == "T") {
        return static_cast<qint64>(size * 1024LL * 1024LL * 1024LL * 1024LL);
    } else if (unit == "PIB" || unit == "PI" || unit == "P") {
        return static_cast<qint64>(size * 1024LL * 1024LL * 1024LL * 1024LL * 1024LL);
    }

    return static_cast<qint64>(size);
}
