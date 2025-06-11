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
    m_hasDevicePartitionsLine = false;
    m_isDeletingRaid = false;  // ДОБАВИТЬ ЭТУ СТРОКУ
    m_currentWipeContext = WipeContext::RAID_CREATION;

    // Подключаем сигналы CommandExecutor
    connect(m_commandExecutor, &CommandExecutor::outputAvailable,
            this, &DiskManager::handleCommandOutput);
    connect(m_commandExecutor, &CommandExecutor::errorOutputAvailable,
            this, &DiskManager::handleCommandErrorOutput);
    connect(m_commandExecutor, &CommandExecutor::finished,
            this, &DiskManager::handleCommandFinished);

    ensureMdadmConfSetup();
}

DiskManager::~DiskManager()
{
}

void DiskManager::ensureMdadmConfSetup()
{
    // Проверяем и настраиваем mdadm.conf при инициализации
    m_hasDevicePartitionsLine = checkDevicePartitionsInMdadmConf();

    if (m_hasDevicePartitionsLine) {
        emit commandOutput(tr("mdadm.conf already contains 'DEVICE partitions' line"));
    } else {
        emit commandOutput(tr("mdadm.conf missing 'DEVICE partitions' line - will be added when creating RAID"));
    }
}

bool DiskManager::checkDevicePartitionsInMdadmConf()
{
    // Проверяем наличие строки "DEVICE partitions" в /etc/mdadm/mdadm.conf
    QString output, errorOutput;
    int exitCode = m_commandExecutor->executeSync("grep",
        QStringList() << "-q" << "^DEVICE partitions" << "/etc/mdadm/mdadm.conf",
        output, errorOutput);

    // grep возвращает 0 если строка найдена, 1 если не найдена
    return (exitCode == 0);
}

bool DiskManager::addDevicePartitionsToMdadmConf()
{
    // Добавляем строку "DEVICE partitions" в mdadm.conf
    QStringList args;
    args << "-c" << "echo 'DEVICE partitions' | sudo tee -a /etc/mdadm/mdadm.conf > /dev/null";

    QString output, errorOutput;
    int exitCode = m_commandExecutor->executeSync("sh", args, output, errorOutput);

    if (exitCode == 0) {
        emit commandOutput(tr("Added 'DEVICE partitions' to /etc/mdadm/mdadm.conf"));
        return true;
    } else {
        emit commandOutput(tr("Failed to add 'DEVICE partitions' to mdadm.conf: %1").arg(errorOutput));
        return false;
    }
}

bool DiskManager::addRaidToMdadmConf(const QString &raidDevice)
{
    emit commandOutput(tr("Adding RAID array %1 to /etc/mdadm/mdadm.conf...").arg(raidDevice));
    QString scanOutput, scanErrorOutput;
    int scanExitCode = m_commandExecutor->executeAsAdminSync("mdadm",
        QStringList() << "--detail" << "--scan",
        scanOutput, scanErrorOutput);

    if (scanExitCode != 0) {
        emit commandOutput(tr("Failed to scan RAID arrays: %1").arg(scanErrorOutput));
        return false;
    }

    // Шаг 2: Ищем строку для нашего устройства в выводе
    QStringList lines = scanOutput.split('\n', Qt::SkipEmptyParts);
    QString targetLine;

    for (const QString &line : lines) {
        if (line.trimmed().startsWith("ARRAY") && line.contains(raidDevice)) {
            targetLine = line.trimmed();
            break;
        }
    }

    if (targetLine.isEmpty()) {
        emit commandOutput(tr("Could not find ARRAY line for device %1 in mdadm scan output").arg(raidDevice));
        return false;
    }

    // Шаг 3: Добавляем только найденную строку в mdadm.conf
    QString addOutput, addErrorOutput;
    int addExitCode = m_commandExecutor->executeAsAdminSync("sh",
        QStringList() << "-c" << QString("echo '%1' >> /etc/mdadm/mdadm.conf").arg(targetLine),
        addOutput, addErrorOutput);

    if (addExitCode == 0) {
        emit commandOutput(tr("Successfully added RAID array %1 to mdadm.conf").arg(raidDevice));
        emit commandOutput(tr("Added line: %1").arg(targetLine));
        return true;
    } else {
        emit commandOutput(tr("Failed to add RAID array to mdadm.conf: %1").arg(addErrorOutput));
        return false;
    }
}

bool DiskManager::updateInitramfs()
{
    emit commandOutput(tr("Updating initramfs to reflect mdadm.conf changes..."));
    emit commandOutput(tr("This may take a few moments..."));

    QString output, errorOutput;
    int exitCode = m_commandExecutor->executeAsAdminSync("update-initramfs",
        QStringList() << "-u",
        output, errorOutput);

    if (exitCode == 0) {
        emit commandOutput(tr("✓ Successfully updated initramfs"));
        if (!output.trimmed().isEmpty()) {
            // Показываем вывод команды, если он есть
            QStringList outputLines = output.split('\n', Qt::SkipEmptyParts);
            for (const QString &line : outputLines) {
                emit commandOutput(tr("  %1").arg(line.trimmed()));
            }
        }
        return true;
    } else {
        emit commandOutput(tr("Failed to update initramfs: %1").arg(errorOutput));
        emit commandOutput(tr("RAID configuration may not persist after reboot"));
        emit commandOutput(tr("Manual execution of 'sudo update-initramfs -u' may be required"));
        return false;
    }
}

void DiskManager::deleteRaidArray(const QString &raidDevice, const QStringList &memberDevices)
{
    m_raidToDelete = raidDevice;
    m_devicesToCleanSuperblock = memberDevices;
    m_cleanedSuperblockDevices.clear();
    m_devicesToWipe = memberDevices; // Переиспользуем для wipefs
    m_wipedDevices.clear();
    m_isDeletingRaid = true;
    m_currentWipeContext = WipeContext::RAID_DELETION;

    emit commandOutput(tr("Starting RAID %1 deletion process").arg(raidDevice));

    // Начинаем с остановки RAID массива
    stopRaidArray();
}

void DiskManager::stopRaidArray()
{
    QStringList args;
    args << "--stop" << m_raidToDelete;

    m_currentCommand = CommandType::STOP_RAID_ARRAY;

    emit commandOutput(tr("Stopping RAID array %1...").arg(m_raidToDelete));
    emit raidStopProgress(tr("Executing mdadm --stop %1").arg(m_raidToDelete));

    if (!m_commandExecutor->executeAsAdmin("mdadm", args)) {
        qWarning() << "Failed to run mdadm stop command";
        emit raidStopCompleted(false, m_raidToDelete);
    }
}

void DiskManager::processNextSuperblockClean()
{
    /*if (m_devicesToCleanSuperblock.isEmpty()) {
        // Все суперблоки очищены, начинаем wipefs
        processNextWipeOperation();
        return;
    }*/

    QString deviceToClean = m_devicesToCleanSuperblock.takeFirst();
    m_currentSuperblockDevice = deviceToClean;

    QStringList args;
    args << "--zero-superblock" << deviceToClean;

    m_currentCommand = CommandType::CLEAN_SUPERBLOCK;

    emit commandOutput(tr("Cleaning superblock from %1...").arg(deviceToClean));

    QString output, errorOutput;
    int exitCode = m_commandExecutor->executeAsAdminSync("mdadm", args, output, errorOutput);

    bool success = (exitCode == 0);
    emit superblockCleanProgress(deviceToClean, success);

    if (success) {
        emit commandOutput(tr("✓ Superblock cleared from %1").arg(deviceToClean));
        m_cleanedSuperblockDevices.append(deviceToClean);
        emit m_commandExecutor->finished(0, QProcess::NormalExit);
    } else {
        emit commandOutput(tr("✗ Failed to clear superblock from %1: %2").arg(deviceToClean).arg(errorOutput));
        emit m_commandExecutor->finished(1, QProcess::CrashExit);
    }

    /*if (!m_commandExecutor->executeAsAdmin("mdadm", args)) {
        qWarning() << "Failed to run mdadm zero-superblock command for" << deviceToClean;
        emit superblockCleanProgress(deviceToClean, false);
    }*/
}

bool DiskManager::removeRaidFromMdadmConf(const QString &raidDevice)
{
    emit commandOutput(tr("Removing RAID array %1 from /etc/mdadm/mdadm.conf...").arg(raidDevice));

    // Экранируем слэши в пути устройства для sed
    QString escapedPath = raidDevice;
    escapedPath.replace("/", "\\/");

    // Удаляем строки содержащие путь к RAID устройству
    QStringList args;
    args << "-c" << QString("sed -i '/%1/d' /etc/mdadm/mdadm.conf").arg(escapedPath);

    QString output, errorOutput;
    int exitCode = m_commandExecutor->executeAsAdminSync("sh", args, output, errorOutput);

    if (exitCode == 0) {
        emit commandOutput(tr("Successfully removed RAID array %1 from mdadm.conf").arg(raidDevice));
        return true;
    } else {
        emit commandOutput(tr("Failed to remove RAID array from mdadm.conf: %1").arg(errorOutput));
        return false;
    }
}

void DiskManager::finishRaidDeletion()
{
    if (m_raidToDelete.isEmpty()) {
        return;
    }

    emit commandOutput(tr("Finalizing RAID deletion..."));

    // Удаляем запись из mdadm.conf
    bool mdadmConfUpdated = removeRaidFromMdadmConf(m_raidToDelete);

    // Обновляем initramfs
    if (mdadmConfUpdated) {
        emit commandOutput(tr("Updating system boot configuration..."));
        updateInitramfs();
    }

    // Завершаем процесс удаления
    emit commandOutput(tr("RAID array %1 deletion completed successfully").arg(m_raidToDelete));
    emit raidDeletionCompleted(true, m_raidToDelete);

    // Очищаем состояние
    m_raidToDelete.clear();
    m_isDeletingRaid = false;

    // Обновляем список устройств
    refreshDevices();
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

void DiskManager::createRaidArray(RaidType raidType, const QStringList &devices, const QString &arrayName)
{
    if (devices.size() < 2) {
        emit commandOutput(tr("Error: At least 2 devices required for RAID creation"));
        emit raidCreationCompleted(false, QString());
        return;
    }

    // Сохраняем параметры для создания RAID
    m_raidType = raidType;
    m_raidArrayName = arrayName;
    m_devicesToWipe = devices;
    m_wipedDevices.clear();
    m_createdRaidDevice.clear();
    m_isDeletingRaid = false;  // ДОБАВИТЬ ЭТУ СТРОКУ
    m_currentWipeContext = WipeContext::RAID_CREATION;  // ДОБАВИТЬ ЭТУ СТРОКУ

    emit raidCreationStarted();
    emit commandOutput(tr("Starting RAID %1 creation with %2 devices")
                      .arg(static_cast<int>(raidType))
                      .arg(devices.size()));

    // Начинаем с очистки первого устройства
    processNextWipeOperation();
}

void DiskManager::processAllWipeOperationsSync()
{
    emit commandOutput(tr("Starting wipefs operations for %1 devices...").arg(m_devicesToWipe.size()));

    // Выполняем wipefs для всех устройств последовательно
    for (const QString &devicePath : m_devicesToWipe) {
        emit commandOutput(tr("Wiping device %1...").arg(devicePath));

        QStringList args;
        args << "--all" << "--force" << devicePath;

        QString output, errorOutput;
        int exitCode = m_commandExecutor->executeAsAdminSync("wipefs", args, output, errorOutput);

        bool success = (exitCode == 0);
        emit deviceWipeCompleted(devicePath, success);

        if (success) {
            emit commandOutput(tr("✓ Device %1 wiped successfully").arg(devicePath));
            m_wipedDevices.append(devicePath);
        } else {
            emit commandOutput(tr("✗ Failed to wipe device %1: %2").arg(devicePath).arg(errorOutput));
        }
    }

    // Очищаем список устройств для wipefs
    m_devicesToWipe.clear();

    // Проверяем контекст и завершаем соответствующую операцию
    if (m_currentWipeContext == WipeContext::RAID_DELETION) {
        emit commandOutput(tr("All cleanup operations completed, finalizing RAID deletion..."));
        finishRaidDeletion();
    }
    // Для создания RAID оставляем существующую логику через processNextWipeOperation()
}

void DiskManager::wipeDevice(const QString &devicePath)
{
    m_currentWipeDevice = devicePath;

    // Формируем команду wipefs
    QStringList args;
    args << "--all" << "--force" << devicePath;

    // Устанавливаем тип команды
    m_currentCommand = CommandType::WIPE_DEVICE;

    emit commandOutput(tr("Wiping device %1...").arg(devicePath));

    if (m_currentWipeContext == WipeContext::RAID_CREATION) {
        emit raidCreationProgress(tr("Preparing device %1").arg(devicePath));
    } else {
        emit raidStopProgress(tr("Wiping device %1").arg(devicePath));
    }

    // Запускаем wipefs с правами администратора
    if (!m_commandExecutor->executeAsAdmin("wipefs", args)) {
        qWarning() << "Failed to run wipefs command for" << devicePath;
        emit deviceWipeCompleted(devicePath, false);
    }
}

void DiskManager::processNextWipeOperation()
{
    /*if (m_devicesToWipe.isEmpty()) {
        // Все устройства очищены, переходим к следующему этапу в зависимости от контекста
        if (m_currentWipeContext == WipeContext::RAID_CREATION) {
            executeRaidCreation();
        } else if (m_currentWipeContext == WipeContext::RAID_DELETION) {
            finishRaidDeletion();
        }
        return;
    }*/

    // Берем следующее устройство для очистки
    QString deviceToWipe = m_devicesToWipe.takeFirst();
    m_currentWipeDevice = deviceToWipe;

    QStringList args;
    args << "--all" << "--force" << deviceToWipe;

    m_currentCommand = CommandType::WIPE_DEVICE;

    emit commandOutput(tr("Wiping device %1...").arg(deviceToWipe));

    // Устанавливаем правильное сообщение в зависимости от контекста
    if (m_currentWipeContext == WipeContext::RAID_CREATION) {
        emit raidCreationProgress(tr("Preparing device %1").arg(deviceToWipe));
    }

    // Выполняем wipefs синхронно
    QString output, errorOutput;
    int exitCode = m_commandExecutor->executeAsAdminSync("wipefs", args, output, errorOutput);

    bool success = (exitCode == 0);
    emit deviceWipeCompleted(deviceToWipe, success);

    if (success) {
        emit commandOutput(tr("✓ Device %1 wiped successfully").arg(deviceToWipe));
        m_wipedDevices.append(deviceToWipe);
        // Эмитим сигнал успешного завершения
        emit m_commandExecutor->finished(0, QProcess::NormalExit);
    } else {
        emit commandOutput(tr("✗ Failed to wipe device %1: %2").arg(deviceToWipe).arg(errorOutput));
        // Эмитим сигнал ошибки
        emit m_commandExecutor->finished(1, QProcess::CrashExit);
    }
}

void DiskManager::executeRaidCreation()
{
    if (m_wipedDevices.size() < 2) {
        emit commandOutput(tr("Error: Not enough devices prepared for RAID creation"));
        emit raidCreationCompleted(false, QString());
        return;
    }

    // Генерируем имя RAID устройства
    m_createdRaidDevice = generateRaidDeviceName();

    // Формируем команду mdadm
    QStringList args;
    args << "--create" << m_createdRaidDevice;
    args << "--level=" + raidTypeToMdadmLevel(m_raidType);
    args << "--raid-devices=" + QString::number(m_wipedDevices.size());

    // Добавляем имя массива если указано
    if (!m_raidArrayName.isEmpty()) {
        args << "--name=" + m_raidArrayName;
    }

    // Для RAID 0 и 1 устанавливаем metadata версию для совместимости
    args << "--metadata=1.2";

    // Добавляем устройства
    args.append(m_wipedDevices);

    // Устанавливаем тип команды
    m_currentCommand = CommandType::CREATE_RAID_ARRAY;

    emit commandOutput(tr("Creating RAID array %1...").arg(m_createdRaidDevice));
    emit raidCreationProgress(tr("Running mdadm to create RAID array"));

    // Запускаем mdadm с правами администратора
    if (!m_commandExecutor->executeAsAdmin("mdadm", args)) {
        qWarning() << "Failed to run mdadm command";
        emit raidCreationCompleted(false, QString());
    }
}

QString DiskManager::generateRaidDeviceName() const
{
    // Ищем свободное имя для RAID устройства
    for (int i = 0; i < 128; ++i) {
        QString candidateName = QString("/dev/md%1").arg(i);

        // Проверяем, не существует ли уже такое устройство
        bool exists = false;
        for (const RaidInfo &raid : m_diskStructure.getRaids()) {
            if (raid.devicePath == candidateName) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            return candidateName;
        }
    }

    // Если все заняты, используем /dev/md127 (максимальный номер)
    return "/dev/md127";
}

QString DiskManager::raidTypeToMdadmLevel(RaidType type) const
{
    switch (type) {
    case RaidType::RAID0: return "0";
    case RaidType::RAID1: return "1";
    case RaidType::RAID5: return "5";
    default: return "1";
    }
}

void DiskManager::mountDevice(const QString &devicePath, const QString &mountPoint, const QString &options)
{
    QString errorMessage;

    // Валидация операции монтирования
    if (!validateMountOperation(devicePath, mountPoint, errorMessage)) {
        emit commandOutput(tr("Mount validation failed: %1").arg(errorMessage));
        emit deviceMounted(false, devicePath, mountPoint);
        return;
    }

    // Сохраняем данные для обработки результата
    m_currentMountDevice = devicePath;
    m_currentMountPoint = mountPoint;

    // Формируем команду mount
    QStringList args;
    if (!options.isEmpty()) {
        args << "-o" << options;
    }
    args << devicePath << mountPoint;

    // Устанавливаем тип команды
    m_currentCommand = CommandType::MOUNT_DEVICE;

    // Запускаем mount с правами администратора
    if (!m_commandExecutor->executeAsAdmin("mount", args)) {
        qWarning() << "Failed to run mount command";
        emit deviceMounted(false, devicePath, mountPoint);
    }
}

void DiskManager::unmountDevice(const QString &devicePath)
{
    QString errorMessage;

    // Валидация операции размонтирования
    if (!validateUnmountOperation(devicePath, errorMessage)) {
        emit commandOutput(tr("Unmount validation failed: %1").arg(errorMessage));
        emit deviceUnmounted(false, devicePath);
        return;
    }

    // Получаем текущую точку монтирования для удаления из fstab
    QString currentMountPoint = getMountPoint(devicePath);
    m_currentMountDevice = devicePath;
    m_currentMountPoint = currentMountPoint;

    // Формируем команду umount
    QStringList args;
    args << devicePath;

    // Устанавливаем тип команды
    m_currentCommand = CommandType::UNMOUNT_DEVICE;

    // Запускаем umount с правами администратора
    if (!m_commandExecutor->executeAsAdmin("umount", args)) {
        qWarning() << "Failed to run umount command";
        emit deviceUnmounted(false, devicePath);
    }
}

bool DiskManager::isDeviceMounted(const QString &devicePath) const
{
    // Проверяем в нашей структуре данных
    for (const DiskInfo &disk : m_diskStructure.getDisks()) {
        for (const PartitionInfo &partition : disk.partitions) {
            if (partition.devicePath == devicePath) {
                return !partition.mountPoint.isEmpty();
            }
        }
    }

    for (const RaidInfo &raid : m_diskStructure.getRaids()) {
        if (raid.devicePath == devicePath) {
            return !raid.mountPoint.isEmpty();
        }
    }

    return false;
}

QString DiskManager::getMountPoint(const QString &devicePath) const
{
    // Получаем точку монтирования из нашей структуры данных
    for (const DiskInfo &disk : m_diskStructure.getDisks()) {
        for (const PartitionInfo &partition : disk.partitions) {
            if (partition.devicePath == devicePath) {
                return partition.mountPoint;
            }
        }
    }

    for (const RaidInfo &raid : m_diskStructure.getRaids()) {
        if (raid.devicePath == devicePath) {
            return raid.mountPoint;
        }
    }

    return QString();
}

bool DiskManager::validateMountOperation(const QString &devicePath, const QString &mountPoint, QString &errorMessage)
{
    // Проверяем, что устройство не примонтировано
    if (isDeviceMounted(devicePath)) {
        errorMessage = tr("Device %1 is already mounted").arg(devicePath);
        return false;
    }

    // Проверяем наличие файловой системы
    QString filesystem = getDeviceFilesystem(devicePath);
    if (filesystem.isEmpty() || filesystem == "unknown") {
        errorMessage = tr("Device %1 has no recognizable filesystem").arg(devicePath);
        return false;
    }

    // Проверяем, что точка монтирования не пустая
    if (mountPoint.isEmpty()) {
        errorMessage = tr("Mount point cannot be empty");
        return false;
    }

    // Проверяем, что точка монтирования не используется
    QDir mountDir(mountPoint);
    if (mountDir.exists()) {
        QFileInfoList entries = mountDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
        if (!entries.isEmpty()) {
            errorMessage = tr("Mount point %1 is not empty").arg(mountPoint);
            return false;
        }
    }

    return true;
}

bool DiskManager::validateUnmountOperation(const QString &devicePath, QString &errorMessage)
{
    // Проверяем, что устройство примонтировано
    if (!isDeviceMounted(devicePath)) {
        errorMessage = tr("Device %1 is not mounted").arg(devicePath);
        return false;
    }

    return true;
}

bool DiskManager::addToFstab(const QString &devicePath, const QString &mountPoint, const QString &filesystem, const QString &options)
{
    // Получаем UUID устройства для более надежного монтирования
    QString uuid = "";
    QString output, errorOutput;
    int exitCode = m_commandExecutor->executeAsAdminSync("blkid",
        QStringList() << "-s" << "UUID" << "-o" << "value" << devicePath,
        output, errorOutput);

    if (exitCode == 0 && !output.trimmed().isEmpty()) {
        uuid = output.trimmed();
    }

    // Формируем запись для fstab
    QString fstabEntry;
    if (!uuid.isEmpty()) {
        fstabEntry = QString("UUID=%1 %2 %3 %4 0 2\n")
                     .arg(uuid)
                     .arg(mountPoint)
                     .arg(filesystem)
                     .arg(options.isEmpty() ? "defaults" : options);
    } else {
        fstabEntry = QString("%1 %2 %3 %4 0 2\n")
                     .arg(devicePath)
                     .arg(mountPoint)
                     .arg(filesystem)
                     .arg(options.isEmpty() ? "defaults" : options);
    }

    // Добавляем запись в /etc/fstab
    QStringList args;
    args << "-c" << QString("echo '%1' >> /etc/fstab").arg(fstabEntry.trimmed());

    QProcess addToFstabProcess;
    addToFstabProcess.start("sudo", QStringList() << "sh" << args);
    if (!addToFstabProcess.waitForFinished(5000)) {
        qWarning() << "Failed to add entry to fstab";
        return false;
    }

    if (addToFstabProcess.exitCode() != 0) {
        qWarning() << "Failed to add entry to fstab:" << addToFstabProcess.readAllStandardError();
        return false;
    }

    return true;
}

bool DiskManager::removeFromFstab(const QString &devicePath)
{
    // Получаем UUID устройства
    QString uuid = "";
    QString output, errorOutput;
    int exitCode = m_commandExecutor->executeAsAdminSync("blkid",
        QStringList() << "-s" << "UUID" << "-o" << "value" << devicePath,
        output, errorOutput);

    if (exitCode == 0 && !output.trimmed().isEmpty()) {
        uuid = output.trimmed();
    }

    // Формируем команду для удаления строки из fstab
    QString sedPattern;
    if (!uuid.isEmpty()) {
        sedPattern = QString("UUID=%1").arg(uuid);
    } else {
        // Экранируем слэши в пути устройства для sed
        QString escapedPath = devicePath;
        escapedPath.replace("/", "\\/");
        sedPattern = escapedPath;
    }

    QStringList args;
    args << "-c" << QString("sed -i '/%1/d' /etc/fstab").arg(sedPattern);

    QProcess removeFromFstabProcess;
    removeFromFstabProcess.start("sudo", QStringList() << "sh" << args);
    if (!removeFromFstabProcess.waitForFinished(5000)) {
        qWarning() << "Failed to remove entry from fstab";
        return false;
    }

    if (removeFromFstabProcess.exitCode() != 0) {
        qWarning() << "Failed to remove entry from fstab:" << removeFromFstabProcess.readAllStandardError();
        return false;
    }

    return true;
}

QString DiskManager::getDeviceFilesystem(const QString &devicePath) const
{
    // Ищем в нашей структуре данных
    for (const DiskInfo &disk : m_diskStructure.getDisks()) {
        for (const PartitionInfo &partition : disk.partitions) {
            if (partition.devicePath == devicePath) {
                return partition.filesystem;
            }
        }
    }

    for (const RaidInfo &raid : m_diskStructure.getRaids()) {
        if (raid.devicePath == devicePath) {
            return raid.filesystem;
        }
    }

    return QString();
}

bool DiskManager::createMountPoint(const QString &mountPoint)
{
    QDir dir;
    if (dir.exists(mountPoint)) {
        return true; // Уже существует
    }

    // Создаем каталог с правами администратора
    QProcess mkdirProcess;
    mkdirProcess.start("sudo", QStringList() << "mkdir" << "-p" << mountPoint);
    if (!mkdirProcess.waitForFinished(5000)) {
        qWarning() << "Failed to create mount point:" << mountPoint;
        return false;
    }

    return mkdirProcess.exitCode() == 0;
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
                emit devicesRefreshed(true);
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
                if (!m_raidDetailsToCheck.isEmpty()) {
                    handleCommandFinished(0, QProcess::NormalExit);
                } else {
                    emit devicesRefreshed(true);
                }
            }
        } else {
            emit devicesRefreshed(true);
        }
        break;

    case CommandType::MDADM_DETAIL:
        if (!m_raidDetailsToCheck.isEmpty()) {
            QString raidPath = m_raidDetailsToCheck.takeFirst();
            QStringList detailArgs;
            detailArgs << "--detail" << raidPath;

            m_currentCommand = CommandType::MDADM_DETAIL;
            if (!m_commandExecutor->executeAsAdmin("mdadm", detailArgs)) {
                qDebug() << "Failed to run mdadm detail command for" << raidPath;
                if (!m_raidDetailsToCheck.isEmpty()) {
                    handleCommandFinished(0, QProcess::NormalExit);
                } else {
                    emit devicesRefreshed(true);
                }
            }
        } else {
            emit devicesRefreshed(true);
        }
        break;

    case CommandType::CREATE_PARTITION_TABLE:
        emit partitionTableCreated(exitCode == 0 && exitStatus == QProcess::NormalExit);
        emit commandOutput(tr("parted created partition table"));
        refreshDevices();
        break;

    case CommandType::CREATE_PARTITION:
        emit partitionCreated(exitCode == 0 && exitStatus == QProcess::NormalExit);
        emit commandOutput(tr("parted created partition"));
        refreshDevices();
        break;

    case CommandType::DELETE_PARTITION:
        emit partitionDeleted(exitCode == 0 && exitStatus == QProcess::NormalExit);
        if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
            emit commandOutput(tr("Partition deleted successfully"));
        } else {
            emit commandOutput(tr("Failed to delete partition"));
        }
        refreshDevices();
        break;

    case CommandType::FORMAT_PARTITION:
        emit partitionFormatted(exitCode == 0 && exitStatus == QProcess::NormalExit);
        if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
            emit commandOutput(tr("Partition formatted successfully"));
        } else {
            emit commandOutput(tr("Failed to format partition"));
        }
        refreshDevices();
        break;

    case CommandType::GET_FREE_SPACE_ON_DEVICE:
        break;

    case CommandType::MOUNT_DEVICE:
        if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
            QString filesystem = getDeviceFilesystem(m_currentMountDevice);
            if (addToFstab(m_currentMountDevice, m_currentMountPoint, filesystem, "defaults")) {
                emit commandOutput(tr("Device %1 mounted successfully at %2 and added to fstab")
                                  .arg(m_currentMountDevice).arg(m_currentMountPoint));
                emit deviceMounted(true, m_currentMountDevice, m_currentMountPoint);
            } else {
                emit commandOutput(tr("Device %1 mounted at %2, but failed to add to fstab")
                                  .arg(m_currentMountDevice).arg(m_currentMountPoint));
                emit deviceMounted(true, m_currentMountDevice, m_currentMountPoint);
            }
        } else {
            emit commandOutput(tr("Failed to mount device %1").arg(m_currentMountDevice));
            emit deviceMounted(false, m_currentMountDevice, m_currentMountPoint);
        }
        refreshDevices();
        break;

    case CommandType::UNMOUNT_DEVICE:
        if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
            if (removeFromFstab(m_currentMountDevice)) {
                emit commandOutput(tr("Device %1 unmounted successfully and removed from fstab")
                                  .arg(m_currentMountDevice));
            } else {
                emit commandOutput(tr("Device %1 unmounted, but failed to remove from fstab")
                                  .arg(m_currentMountDevice));
            }
            emit deviceUnmounted(true, m_currentMountDevice);
        } else {
            emit commandOutput(tr("Failed to unmount device %1").arg(m_currentMountDevice));
            emit deviceUnmounted(false, m_currentMountDevice);
        }
        refreshDevices();
        break;

    case CommandType::STOP_RAID_ARRAY:
        emit raidStopCompleted(exitCode == 0 && exitStatus == QProcess::NormalExit, m_raidToDelete);
        if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
            emit commandOutput(tr("RAID array %1 stopped successfully").arg(m_raidToDelete));
            // Начинаем очистку суперблоков
            processNextSuperblockClean();
        } else {
            emit commandOutput(tr("Failed to stop RAID array %1").arg(m_raidToDelete));
            emit raidDeletionCompleted(false, m_raidToDelete);
            m_isDeletingRaid = false;
            m_raidToDelete.clear();
        }
        break;

    case CommandType::CLEAN_SUPERBLOCK:
        emit superblockCleanProgress(m_currentSuperblockDevice,
                                    exitCode == 0 && exitStatus == QProcess::NormalExit);
        /*if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
            emit commandOutput(tr("Superblock cleared from %1").arg(m_currentSuperblockDevice));
            m_cleanedSuperblockDevices.append(m_currentSuperblockDevice);
        } else {
            emit commandOutput(tr("Failed to clear superblock from %1").arg(m_currentSuperblockDevice));
        }*/

        if (m_devicesToCleanSuperblock.isEmpty()) {
            // Все суперблоки очищены, начинаем wipefs
            processNextWipeOperation();
            return;
        }
        else{
            // Обрабатываем следующее устройство
            processNextSuperblockClean();
        }
        break;

    case CommandType::WIPE_DEVICE:
        emit deviceWipeCompleted(m_currentWipeDevice, exitCode == 0 && exitStatus == QProcess::NormalExit);

        /*if (exitCode != 0 || exitStatus != QProcess::NormalExit) {
            // При ошибке wipefs прерываем операцию
            if (m_currentWipeContext == WipeContext::RAID_CREATION) {
                emit commandOutput(tr("Failed to wipe device %1, aborting RAID creation").arg(m_currentWipeDevice));
                emit raidCreationCompleted(false, QString());
            } else if (m_currentWipeContext == WipeContext::RAID_DELETION) {
                emit commandOutput(tr("Failed to wipe device %1, RAID deletion may be incomplete").arg(m_currentWipeDevice));
                emit raidDeletionCompleted(false, m_raidToDelete);
                m_isDeletingRaid = false;
                m_raidToDelete.clear();
            }
            return;
        }*/

        if (m_devicesToWipe.isEmpty()) {
            // Все устройства обработаны, переходим к следующему этапу
            if (m_currentWipeContext == WipeContext::RAID_CREATION) {
                executeRaidCreation();
            } else if (m_currentWipeContext == WipeContext::RAID_DELETION) {
                finishRaidDeletion();
            }
        } else {
            // Обрабатываем следующее устройство
            processNextWipeOperation();
        }
        break;

    case CommandType::CREATE_RAID_ARRAY:
        if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
            emit commandOutput(tr("RAID array %1 created successfully").arg(m_createdRaidDevice));

            bool mdadmConfUpdated = true;
            if (!m_hasDevicePartitionsLine) {
                if (addDevicePartitionsToMdadmConf()) {
                    m_hasDevicePartitionsLine = true;
                } else {
                    mdadmConfUpdated = false;
                }
            }

            if (mdadmConfUpdated) {
                if (addRaidToMdadmConf(m_createdRaidDevice)) {
                    emit commandOutput(tr("Updating system boot configuration..."));
                    updateInitramfs();
                } else {
                    emit commandOutput(tr("RAID created but not added to mdadm.conf - manual configuration may be required"));
                    mdadmConfUpdated = false;
                }
            }

            if (mdadmConfUpdated) {
                emit commandOutput(tr("RAID array fully configured and ready for use"));
            }

            emit raidCreationCompleted(true, m_createdRaidDevice);
        } else {
            emit commandOutput(tr("Failed to create RAID array"));
            emit raidCreationCompleted(false, QString());
        }

        refreshDevices();
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

void DiskManager::formatPartition(const QString &partitionPath, const QString &filesystemType, const QString &label)
{
    // Получаем команду форматирования для указанной файловой системы
    QString formatCommand = getFormatCommand(filesystemType);
    if (formatCommand.isEmpty()) {
        qWarning() << "Unsupported filesystem type:" << filesystemType;
        emit partitionFormatted(false);
        return;
    }

    // Создаем аргументы для команды форматирования
    QStringList args;

    // Добавляем общие опции в зависимости от типа ФС
    if (filesystemType.startsWith("ext")) {
        // Для ext2/3/4: принудительное форматирование, быстрое создание
        args << "-F"; // принудительно
        if (!label.isEmpty()) {
            args << "-L" << label; // метка тома
        }
    } else if (filesystemType == "xfs") {
        // Для XFS: принудительное форматирование
        args << "-f"; // принудительно
        if (!label.isEmpty()) {
            args << "-L" << label; // метка тома
        }
    } else if (filesystemType == "btrfs") {
        // Для Btrfs: принудительное форматирование
        args << "-f"; // принудительно
        if (!label.isEmpty()) {
            args << "-L" << label; // метка тома
        }
    } else if (filesystemType == "fat32") {
        // Для FAT32
        args << "-F" << "32"; // FAT32
        if (!label.isEmpty()) {
            args << "-n" << label; // метка тома
        }
    } else if (filesystemType == "fat16") {
        // Для FAT16
        args << "-F" << "16"; // FAT16
        if (!label.isEmpty()) {
            args << "-n" << label; // метка тома
        }
    } else if (filesystemType == "ntfs") {
        // Для NTFS: быстрое форматирование
        args << "-f"; // быстрое форматирование
        if (!label.isEmpty()) {
            args << "-L" << label; // метка тома
        }
    } else if (filesystemType == "linux-swap") {
        // Для swap: только метка если указана
        if (!label.isEmpty()) {
            args << "-L" << label; // метка swap
        }
    }

    // Добавляем путь к устройству
    args << partitionPath;

    // Устанавливаем тип команды
    m_currentCommand = CommandType::FORMAT_PARTITION;

    qDebug() << "Formatting partition:" << partitionPath << "as" << filesystemType
             << "using command:" << formatCommand << "with args:" << args;

    // Запускаем команду форматирования с правами администратора
    if (!m_commandExecutor->executeAsAdmin(formatCommand, args)) {
        qWarning() << "Failed to run format command:" << formatCommand;
        emit partitionFormatted(false);
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

        // Обрабатываем обычные диски
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
                    parsePartitionFromJson(partObj, disk);
                }
            }

            // Добавляем диск в структуру
            m_diskStructure.addDisk(disk);
        }
        // Обрабатываем RAID устройства (md)
        else if (type == "raid0" || type == "raid1" || type == "raid5" || type == "raid10") {
            RaidInfo raid;
            raid.devicePath = "/dev/" + name;
            raid.size = deviceObj["size"].toString();
            raid.filesystem = deviceObj["fstype"].toString();
            raid.mountPoint = deviceObj["mountpoint"].toString();
            raid.state = tr("active"); // По умолчанию считаем активным

            // Определяем тип RAID по типу устройства
            if (type == "raid0") {
                raid.type = RaidType::RAID0;
            } else if (type == "raid1") {
                raid.type = RaidType::RAID1;
            } else if (type == "raid5") {
                raid.type = RaidType::RAID5;
            } else {
                raid.type = RaidType::UNKNOWN;
            }

            // Добавляем RAID в структуру (члены будут добавлены в parseMdadmDetailOutput)
            m_diskStructure.addRaid(raid);
        }
    }
}

// Добавить новый вспомогательный метод для парсинга разделов
void DiskManager::parsePartitionFromJson(const QJsonObject &partObj, DiskInfo &disk)
{
    QString partName = partObj["name"].toString();
    QString partType = partObj["type"].toString();

    if (partType == "part") {
        PartitionInfo partition;
        partition.devicePath = "/dev/" + partName;
        partition.size = partObj["size"].toString();
        partition.filesystem = partObj["fstype"].toString();
        partition.mountPoint = partObj["mountpoint"].toString();

        // Проверяем, является ли раздел членом RAID
        // Это будет определено позже в parseMdadmDetailOutput
        partition.isRaidMember = false;

        // Добавляем раздел к диску
        disk.partitions.append(partition);
    }
}

void DiskManager::parseMdadmScanOutput(const QString &output)
{
    if (output.trimmed().isEmpty()) {
        // Нет RAID массивов - это нормально
        return;
    }

    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        if (!line.startsWith("ARRAY")) {
            continue;
        }

        RaidInfo raid;
        bool foundDevice = false;

        // Парсим строку с информацией о RAID
        QRegularExpression rxDevice("(/dev/md\\d+)");
        QRegularExpressionMatch deviceMatch = rxDevice.match(line);
        if (deviceMatch.hasMatch()) {
            raid.devicePath = deviceMatch.captured(1);
            foundDevice = true;

            // Добавляем в список для детального анализа
            if (!m_raidDetailsToCheck.contains(raid.devicePath)) {
                m_raidDetailsToCheck.append(raid.devicePath);
            }
        }

        // Добавляем RAID в структуру, если найдено устройство
        if (foundDevice) {
            // Устанавливаем значения по умолчанию (уровень будет определен в parseMdadmDetailOutput)
            raid.type = RaidType::UNKNOWN;
            raid.state = tr("unknown");
            raid.syncPercent = 100;
            raid.filesystem = ""; // Будет определена позже
            raid.mountPoint = ""; // Будет определена позже

            // Проверяем, не добавлен ли уже этот RAID
            bool alreadyExists = false;
            for (const RaidInfo &existingRaid : m_diskStructure.getRaids()) {
                if (existingRaid.devicePath == raid.devicePath) {
                    alreadyExists = true;
                    break;
                }
            }

            if (!alreadyExists) {
                m_diskStructure.addRaid(raid);
            }
        }
    }
}

void DiskManager::parseMdadmDetailOutput(const QString &output)
{
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        return;
    }

    QString currentRaidPath;

    // Находим путь к RAID устройству в первых строках
    for (int i = 0; i < qMin(5, lines.size()); ++i) {
        QRegularExpression rxDevice("(/dev/md\\d+)");
        QRegularExpressionMatch match = rxDevice.match(lines[i]);
        if (match.hasMatch()) {
            currentRaidPath = match.captured(1);
            break;
        }
    }

    if (currentRaidPath.isEmpty()) {
        qWarning() << "Could not find RAID device path in mdadm output";
        return;
    }

    // Находим RAID в нашей структуре данных
    QVector<RaidInfo> &raids = m_diskStructure.getRaids();
    RaidInfo *targetRaid = nullptr;

    for (int i = 0; i < raids.size(); ++i) {
        if (raids[i].devicePath == currentRaidPath) {
            targetRaid = &raids[i];
            break;
        }
    }

    if (!targetRaid) {
        qWarning() << "RAID device not found in structure:" << currentRaidPath;
        return;
    }

    // Очищаем список членов перед заполнением
    targetRaid->members.clear();

    // Парсим информацию о RAID
    bool inDeviceSection = false;

    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();

        // Ищем уровень RAID
        if (trimmedLine.contains("Raid Level :")) {
            QStringList parts = trimmedLine.split(':', Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                QString levelStr = parts[1].trimmed().toLower();
                if (levelStr == "raid0" || levelStr == "0") {
                    targetRaid->type = RaidType::RAID0;
                } else if (levelStr == "raid1" || levelStr == "1") {
                    targetRaid->type = RaidType::RAID1;
                } else if (levelStr == "raid5" || levelStr == "5") {
                    targetRaid->type = RaidType::RAID5;
                } else {
                    targetRaid->type = RaidType::UNKNOWN;
                }
            }
        }
        // Ищем состояние RAID
        else if (trimmedLine.contains("State :")) {
            QStringList parts = trimmedLine.split(':', Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                targetRaid->state = parts[1].trimmed();
            }
        }
        // Ищем размер массива
        else if (trimmedLine.contains("Array Size :")) {
            QStringList parts = trimmedLine.split(':', Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                QString sizeInfo = parts[1].trimmed();
                // Извлекаем только число и единицу измерения
                QRegularExpression sizeRegex("(\\d+(?:\\.\\d+)?\\s*\\w+)");
                QRegularExpressionMatch sizeMatch = sizeRegex.match(sizeInfo);
                if (sizeMatch.hasMatch()) {
                    targetRaid->size = sizeMatch.captured(1);
                }
            }
        }
        // Ищем заголовок таблицы устройств
        else if (trimmedLine.contains("Number") && trimmedLine.contains("Major") &&
                 trimmedLine.contains("Minor") && trimmedLine.contains("RaidDevice")) {
            inDeviceSection = true;
            continue;
        }
        // Если мы в секции устройств, парсим их
        else if (inDeviceSection) {
            // Пустая строка означает конец секции устройств
            if (trimmedLine.isEmpty()) {
                break;
            }

            // Парсим строку с устройством
            QStringList parts = trimmedLine.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 6) {
                // Формат: Number Major Minor RaidDevice State DeviceName
                QString deviceName = parts.last(); // Имя устройства в последней колонке
                QString devicePath = deviceName;

                // Определяем статус устройства из предпоследней колонки или из состояния
                QString statusStr = parts.size() > 6 ? parts[parts.size() - 2] : parts[4];

                DeviceStatus status = DeviceStatus::NORMAL;
                if (statusStr.contains("active", Qt::CaseInsensitive)) {
                    status = DeviceStatus::NORMAL;
                } else if (statusStr.contains("faulty", Qt::CaseInsensitive)) {
                    status = DeviceStatus::FAULTY;
                } else if (statusStr.contains("spare", Qt::CaseInsensitive)) {
                    status = DeviceStatus::SPARE;
                } else if (statusStr.contains("rebuilding", Qt::CaseInsensitive)) {
                    status = DeviceStatus::REBUILDING;
                } else if (statusStr.contains("sync", Qt::CaseInsensitive)) {
                    status = DeviceStatus::SYNCING;
                } else if (statusStr.contains("active sync", Qt::CaseInsensitive)) {
                    status = DeviceStatus::NORMAL;
                }

                RaidMemberInfo member(devicePath, status);
                targetRaid->members.append(member);

                // Отмечаем соответствующий раздел как член RAID
                markPartitionAsRaidMember(devicePath);
            }
        }
    }

    // После парсинга mdadm detail получаем информацию о файловой системе и монтировании
    updateRaidFilesystemInfo(targetRaid);
}

// НОВАЯ ФУНКЦИЯ: Обновление информации о файловой системе RAID массива
void DiskManager::updateRaidFilesystemInfo(RaidInfo *raidInfo)
{
    if (!raidInfo) {
        return;
    }

    // Используем blkid для получения информации о файловой системе
    QString output, errorOutput;
    int exitCode = m_commandExecutor->executeAsAdminSync("blkid",
        QStringList() << "-o" << "value" << "-s" << "TYPE" << raidInfo->devicePath,
        output, errorOutput);

    if (exitCode == 0 && !output.trimmed().isEmpty()) {
        raidInfo->filesystem = output.trimmed();
    }

    // Проверяем, смонтирован ли RAID массив
    QString mountOutput, mountErrorOutput;
    int mountExitCode = m_commandExecutor->executeSync("findmnt",
        QStringList() << "-n" << "-o" << "TARGET" << raidInfo->devicePath,
        mountOutput, mountErrorOutput);

    if (mountExitCode == 0 && !mountOutput.trimmed().isEmpty()) {
        raidInfo->mountPoint = mountOutput.trimmed();
    } else {
        raidInfo->mountPoint = "";
    }

    // Отладочная информация
    qDebug() << "Updated RAID filesystem info for" << raidInfo->devicePath
             << "- FS:" << raidInfo->filesystem
             << "- Mount:" << raidInfo->mountPoint;
}

// Добавить новый вспомогательный метод
void DiskManager::markPartitionAsRaidMember(const QString &devicePath)
{
    // Проходим по всем дискам и их разделам
    QVector<DiskInfo> &disks = m_diskStructure.getDisks();
    for (DiskInfo &disk : disks) {
        for (PartitionInfo &partition : disk.partitions) {
            if (partition.devicePath == devicePath) {
                partition.isRaidMember = true;
                return;
            }
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

QString DiskManager::getFormatCommand(const QString &filesystemType) const
{
    // Возвращаем команду форматирования для указанного типа файловой системы
    if (filesystemType == "ext4") {
        return "mkfs.ext4";
    } else if (filesystemType == "ext3") {
        return "mkfs.ext3";
    } else if (filesystemType == "ext2") {
        return "mkfs.ext2";
    } else if (filesystemType == "xfs") {
        return "mkfs.xfs";
    } else if (filesystemType == "btrfs") {
        return "mkfs.btrfs";
    } else if (filesystemType == "fat32" || filesystemType == "fat16") {
        return "mkfs.fat";
    } else if (filesystemType == "ntfs") {
        return "mkfs.ntfs";
    } else if (filesystemType == "linux-swap") {
        return "mkswap";
    }

    return QString(); // Неподдерживаемый тип ФС
}
