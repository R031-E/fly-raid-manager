#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_showAllDevices(true) // По умолчанию показываем все устройства
{
    ui->setupUi(this);

    // Создаем менеджер дисков
    m_diskManager = new DiskManager(this);

    // Подключаем сигналы
    connect(m_diskManager, &DiskManager::devicesRefreshed,
            this, &MainWindow::onDevicesRefreshed);
    connect(m_diskManager, &DiskManager::commandOutput,
            this, &MainWindow::onCommandOutput);
    connect(m_diskManager, &DiskManager::partitionTableCreated,
            this, [this](bool success) {
                if (success) {
                    ui->statusBar->showMessage(tr("Partition table created successfully"), 3000);
                } else {
                    ui->statusBar->showMessage(tr("Failed to create partition table"), 3000);
                }
            });
    connect(m_diskManager, &DiskManager::partitionCreated,
            this, [this](bool success) {
                if (success) {
                    ui->statusBar->showMessage(tr("Partition created successfully"), 3000);
                } else {
                    ui->statusBar->showMessage(tr("Failed to create partition"), 3000);
                }
            });
    connect(m_diskManager, &DiskManager::partitionDeleted,
            this, [this](bool success) {
                if (success) {
                    ui->statusBar->showMessage(tr("Partition deleted successfully"), 3000);
                } else {
                    ui->statusBar->showMessage(tr("Failed to delete partition"), 3000);
                }
            });
    connect(m_diskManager, &DiskManager::partitionFormatted,
            this, [this](bool success) {
                if (success) {
                    ui->statusBar->showMessage(tr("Partition formatted successfully"), 3000);
                } else {
                    ui->statusBar->showMessage(tr("Failed to format partition"), 3000);
                }
            });
    connect(m_diskManager, &DiskManager::deviceMounted,
                this, &MainWindow::onDeviceMounted);
    connect(m_diskManager, &DiskManager::deviceUnmounted,
                this, &MainWindow::onDeviceUnmounted);

    connect(m_diskManager, &DiskManager::raidStopProgress,
            this, [this](const QString &message) {
                ui->statusBar->showMessage(message, 3000);
            });
    connect(m_diskManager, &DiskManager::raidStopCompleted,
            this, [this](bool success, const QString &raidDevice) {
                if (success) {
                    ui->statusBar->showMessage(tr("RAID %1 stopped successfully").arg(raidDevice), 3000);
                } else {
                    ui->statusBar->showMessage(tr("Failed to stop RAID %1").arg(raidDevice), 3000);
                }
            });
    connect(m_diskManager, &DiskManager::raidDeletionCompleted,
            this, [this](bool success, const QString &raidDevice) {
                if (success) {
                    ui->statusBar->showMessage(tr("RAID %1 deleted successfully").arg(raidDevice), 5000);
                    onRefreshDevices();
                } else {
                    ui->statusBar->showMessage(tr("Failed to delete RAID %1").arg(raidDevice), 5000);
                }
            });
    connect(m_diskManager, &DiskManager::deviceMarkedFaulty,
            this, [this](bool success, const QString &raidDevice, const QString &memberDevice) {
                if (success) {
                    ui->statusBar->showMessage(tr("Device %1 marked as faulty in RAID %2")
                                              .arg(memberDevice).arg(raidDevice), 5000);
                    onRefreshDevices();
                } else {
                    ui->statusBar->showMessage(tr("Failed to mark device %1 as faulty")
                                              .arg(memberDevice), 5000);
                }
            });
    connect(m_diskManager, &DiskManager::deviceRemovedFromRaid,
            this, [this](bool success, const QString &raidDevice, const QString &memberDevice) {
                if (success) {
                    ui->statusBar->showMessage(tr("Device %1 removed from RAID %2 successfully")
                                              .arg(memberDevice).arg(raidDevice), 5000);
                    onRefreshDevices();
                } else {
                    ui->statusBar->showMessage(tr("Failed to remove device %1 from RAID %2")
                                              .arg(memberDevice).arg(raidDevice), 5000);
                }
            });
    connect(m_diskManager, &DiskManager::deviceAddedToRaid,
            this, [this](bool success, const QString &raidDevice, const QString &memberDevice) {
                if (success) {
                    ui->statusBar->showMessage(tr("Device %1 added to RAID %2 successfully. "
                                                  "Rebuilding/syncing started.")
                                              .arg(memberDevice).arg(raidDevice), 5000);
                    onRefreshDevices();
                } else {
                    ui->statusBar->showMessage(tr("Failed to add device %1 to RAID %2")
                                              .arg(memberDevice).arg(raidDevice), 5000);
                }
            });
    connect(m_diskManager, &DiskManager::spareActivationCompleted,
            this, [this](bool success, const QString &raidDevice, const QString &spareDevice) {
                if (success) {
                    ui->statusBar->showMessage(tr("Spare device %1 activated in RAID %2 successfully")
                                              .arg(spareDevice).arg(raidDevice), 5000);
                    onRefreshDevices();
                } else {
                    ui->statusBar->showMessage(tr("Failed to activate spare device %1 in RAID %2")
                                              .arg(spareDevice).arg(raidDevice), 5000);
                }
            });

    // Подключаем сигналы интерфейса
    connect(ui->btnRefreshDevices, &QPushButton::clicked,
            this, &MainWindow::onRefreshDevices);
    connect(ui->actionRefresh, &QAction::triggered,
            this, &MainWindow::onRefreshDevices);
    connect(ui->cmbViewMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onViewModeChanged);
    connect(ui->actionCreatePartitionTable, &QAction::triggered,
            this, &MainWindow::onCreatePartitionTableClicked);
    connect(ui->actionCreatePartition, &QAction::triggered,
            this, &MainWindow::onCreatePartitionClicked);
    connect(ui->actionDeletePartition, &QAction::triggered,
            this, &MainWindow::onDeletePartitionClicked);
    connect(ui->actionFormatPartition, &QAction::triggered,
            this, &MainWindow::onFormatPartitionClicked);
    connect(ui->actionMountPartition, &QAction::triggered,
            this, &MainWindow::onMountPartitionClicked);
    connect(ui->actionUnmountPartition, &QAction::triggered,
            this, &MainWindow::onUnmountPartitionClicked);
    connect(ui->actionCreateRaid, &QAction::triggered,
            this, &MainWindow::onCreateRaidClicked);
    connect(ui->actionDestroyRaid, &QAction::triggered,
            this, &MainWindow::onDeleteRaidClicked);

    // Подключаем кнопки управления RAID
    connect(ui->btnMarkFaulty, &QPushButton::clicked,
            this, &MainWindow::onMarkFaultyClicked);
    connect(ui->btnAddToRaid, &QPushButton::clicked,
            this, &MainWindow::onAddToRaidClicked);
    connect(ui->btnRemoveFromRaid, &QPushButton::clicked,
            this, &MainWindow::onRemoveFromRaidClicked);
    connect(ui->btnActivateSpare, &QPushButton::clicked,
            this, &MainWindow::onActivateSpareClicked);

    // Подключаем сигнал изменения выбора в дереве для обновления кнопок
    connect(ui->treeDevices, &QTreeWidget::currentItemChanged,
            this, &MainWindow::updateButtonState);

    // Настраиваем интерфейс
    setupInitialInterface();

    // Обновляем список устройств при запуске
    onRefreshDevices();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupInitialInterface()
{
    ui->treeDevices->setHeaderLabels(QStringList()
                                    << tr("Device") << tr("Size") << tr("Filesystem")
                                    << tr("Used") << tr("Free") << tr("Mount Point")
                                    << tr("Flags") << tr("Status"));
    updateButtonState();
}

void MainWindow::onActivateSpareClicked()
{
    // Получаем выбранный элемент
    QTreeWidgetItem *item = ui->treeDevices->currentItem();
    if (!item) {
        QMessageBox::warning(this, tr("No Device Selected"),
                            tr("Please select a spare device to activate."));
        return;
    }

    // Проверяем, является ли выбранное устройство членом RAID
    if (!isSelectedItemRaidMember()) {
        QMessageBox::warning(this, tr("Invalid Selection"),
                            tr("Please select a spare device that is a member of a RAID array."));
        return;
    }

    QString memberDevice = getSelectedDevicePath();
    QString raidDevice = getSelectedRaidDevice();

    if (memberDevice.isEmpty() || raidDevice.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Device"),
                            tr("Cannot determine device or RAID array path."));
        return;
    }

    // Проверяем, что устройство действительно spare
    QString deviceStatus = item->text(3); // Колонка Status
    if (!deviceStatus.contains("spare", Qt::CaseInsensitive)) {
        QMessageBox::warning(this, tr("Not a Spare Device"),
                            tr("Selected device %1 is not a spare device.\n\n"
                               "Only spare devices can be activated.")
                               .arg(memberDevice));
        return;
    }

    // Определяем тип RAID для специального предупреждения для RAID5
    QString raidType = item->parent()->text(2); // Тип RAID родительского элемента

    // Формируем сообщение подтверждения
    QString confirmMessage = tr("Activate spare device %1 in RAID array %2?\n\n"
                               "This will:\n"
                               "1) Promote the spare device to an active member\n"
                               "2) Start automatic rebuilding/syncing process\n"
                               "3) The RAID array will operate normally during rebuild\n\n")
                               .arg(memberDevice)
                               .arg(raidDevice);

    // Добавляем специальное предупреждение для RAID5
    if (raidType == "RAID5") {
        confirmMessage += tr("<b>Important for RAID5:</b>\n"
                           "After the rebuild is complete, you will need to manually "
                           "expand the filesystem to use the additional space. "
                           "The RAID array size will increase, but the filesystem "
                           "will remain at its original size until manually resized.\n\n");
    }

    confirmMessage += tr("Do you want to continue?");

    // Запрашиваем подтверждение
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Confirm Activate Spare"));
    msgBox.setText(confirmMessage);
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    // Для RAID5 делаем текст более заметным
    if (raidType == "RAID5") {
        msgBox.setStyleSheet("QMessageBox { background-color: #fff3cd; }");
    }

    if (msgBox.exec() == QMessageBox::Yes) {
        // Активируем spare устройство
        m_diskManager->activateSpareDevice(raidDevice, memberDevice);

        ui->statusBar->showMessage(tr("Activating spare device %1...")
                                  .arg(memberDevice), 3000);
    }
}

void MainWindow::onCreateRaidClicked()
{
    // Проверяем, есть ли доступные устройства для RAID
    DiskStructure diskStructure = m_diskManager->getDiskStructure();

    // Подсчитываем доступные устройства
    int availableDevices = 0;

    // Считаем разделы, которые не смонтированы и не в RAID
    for (const DiskInfo &disk : diskStructure.getDisks()) {
        for (const PartitionInfo &partition : disk.partitions) {
            if (partition.mountPoint.isEmpty() && !partition.isRaidMember) {
                availableDevices++;
            }
        }
    }

    // Считаем неразмеченные диски
    for (const DiskInfo &disk : diskStructure.getDisks()) {
        if (disk.partitions.isEmpty()) {
            // Проверяем, не используется ли диск в RAID
            bool isInRaid = false;
            for (const RaidInfo &raid : diskStructure.getRaids()) {
                for (const RaidMemberInfo &member : raid.members) {
                    if (member.devicePath == disk.devicePath) {
                        isInRaid = true;
                        break;
                    }
                }
                if (isInRaid) break;
            }

            if (!isInRaid) {
                availableDevices++;
            }
        }
    }

    if (availableDevices < 2) {
        QMessageBox::warning(this, tr("Insufficient Devices"),
                            tr("At least 2 unmounted devices or partitions are required to create a RAID array.\n\n"
                               "Available devices: %1\n"
                               "Required: 2 or more").arg(availableDevices));
        return;
    }

    // Создаем и показываем диалог создания RAID
    CreateRaidArrayDialog *dialog = new CreateRaidArrayDialog(diskStructure, this);

    // Подключаем сигналы диалога к DiskManager
    connect(dialog, &CreateRaidArrayDialog::createRaidRequested,
            m_diskManager, &DiskManager::createRaidArray);

    // Подключаем сигналы DiskManager к диалогу
    connect(m_diskManager, &DiskManager::deviceWipeCompleted,
            dialog, &CreateRaidArrayDialog::onWipeProgressUpdate);
    connect(m_diskManager, &DiskManager::raidCreationProgress,
            dialog, &CreateRaidArrayDialog::onRaidCreationProgress);
    connect(m_diskManager, &DiskManager::raidCreationCompleted,
            dialog, &CreateRaidArrayDialog::onRaidCreationCompleted);

    // Показываем диалог
    if (dialog->exec() == QDialog::Accepted) {
        ui->statusBar->showMessage(tr("RAID array creation completed"), 5000);

        // Обновляем отображение устройств
        onRefreshDevices();
    }

    dialog->deleteLater();
}

void MainWindow::onDeleteRaidClicked()
{
    // Получаем выбранный элемент
    QTreeWidgetItem *item = ui->treeDevices->currentItem();
    if (!item) {
        QMessageBox::warning(this, tr("No RAID Selected"),
                            tr("Please select a RAID array to delete."));
        return;
    }

    bool isMounted = m_diskManager->isDeviceMounted(item->text(0));
    // Проверяем, является ли выбранное устройство RAID массивом
    if (!isSelectedItemRaid()) {
        QMessageBox::warning(this, tr("Invalid Selection"),
                            tr("Please select a RAID array, not a disk or partition."));
        return;
    }

    if (isMounted) {
        QMessageBox::warning(this, tr("Device is mounted"),
                            tr("Please unmount device."));
        return;
    }

    QString raidPath = getSelectedDevicePath();
    if (raidPath.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid RAID"),
                            tr("Cannot determine RAID array path."));
        return;
    }

    // Находим информацию о RAID массиве
    const DiskStructure& diskStructure = m_diskManager->getDiskStructure();
    RaidInfo selectedRaid;
    bool raidFound = false;

    for (const RaidInfo &raid : diskStructure.getRaids()) {
        if (raid.devicePath == raidPath) {
            selectedRaid = raid;
            raidFound = true;
            break;
        }
    }

    if (!raidFound) {
        QMessageBox::warning(this, tr("RAID Not Found"),
                            tr("Could not find information about RAID array %1.")
                               .arg(raidPath));
        return;
    }

    // Создаем и показываем диалог удаления RAID
    DeleteRaidDialog *dialog = new DeleteRaidDialog(selectedRaid, this);

    // Подключаем сигналы диалога к DiskManager
    connect(dialog, &DeleteRaidDialog::deleteRaidRequested,
            m_diskManager, &DiskManager::deleteRaidArray);

    // Подключаем сигналы DiskManager к диалогу
    connect(m_diskManager, &DiskManager::raidStopProgress,
            dialog, &DeleteRaidDialog::onRaidStopProgress);
    connect(m_diskManager, &DiskManager::raidStopCompleted,
            dialog, &DeleteRaidDialog::onRaidStopCompleted);
    connect(m_diskManager, &DiskManager::superblockCleanProgress,
            dialog, &DeleteRaidDialog::onSuperblockCleanProgress);
    connect(m_diskManager, &DiskManager::deviceWipeCompleted,
            dialog, &DeleteRaidDialog::onWipeProgressUpdate);
    connect(m_diskManager, &DiskManager::raidDeletionCompleted,
            dialog, &DeleteRaidDialog::onRaidDeletionCompleted);

    // Показываем диалог
    if (dialog->exec() == QDialog::Accepted) {
        ui->statusBar->showMessage(tr("RAID array deletion completed"), 5000);
    }

    dialog->deleteLater();
}


void MainWindow::onMountPartitionClicked()
{
    // Получаем выбранный элемент
    QTreeWidgetItem *item = ui->treeDevices->currentItem();
    if (!item) {
        QMessageBox::warning(this, tr("No Device Selected"),
                            tr("Please select a partition or RAID array to mount."));
        return;
    }

    // Проверяем, можно ли монтировать выбранное устройство
    if (!isSelectedItemMountable()) {
        QMessageBox::warning(this, tr("Invalid Selection"),
                            tr("Please select a partition or RAID array, not a disk."));
        return;
    }

    QString devicePath = getSelectedDevicePath();
    if (devicePath.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Device"),
                            tr("Cannot determine device path."));
        return;
    }

    // Проверяем, не смонтировано ли уже устройство
    if (m_diskManager->isDeviceMounted(devicePath)) {
        QMessageBox::information(this, tr("Already Mounted"),
                                tr("Device %1 is already mounted at %2.")
                                   .arg(devicePath)
                                   .arg(m_diskManager->getMountPoint(devicePath)));
        return;
    }

    // Проверяем наличие файловой системы
    QString filesystem = m_diskManager->getDeviceFilesystem(devicePath);
    if (filesystem.isEmpty() || filesystem == "unknown") {
        QMessageBox::warning(this, tr("No Filesystem"),
                            tr("Device %1 has no recognizable filesystem.\n\n"
                               "Please format the device before mounting.")
                               .arg(devicePath));
        return;
    }

    // Создаем диалог выбора папки для монтирования
    FlyDirDialog *dirDialog = new FlyDirDialog(this,
                                              tr("Select Mount Point"),
                                              "/mnt");

    // Подключаем сигналы диалога
    connect(dirDialog, &FlyDirDialog::accepted, [this, dirDialog, devicePath, filesystem]() {
        QString mountPoint = dirDialog->directory();

        if (mountPoint.isEmpty()) {
            QMessageBox::warning(this, tr("No Mount Point"),
                                tr("Please select a valid mount point."));
            dirDialog->deleteLater();
            return;
        }

        // Запрашиваем подтверждение
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, tr("Confirm Mount"),
                                     tr("Mount device %1 (%2) at %3?")
                                         .arg(devicePath)
                                         .arg(filesystem.toUpper())
                                         .arg(mountPoint),
                                     QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            // Монтируем устройство
            m_diskManager->mountDevice(devicePath, mountPoint);
        }

        dirDialog->deleteLater();
    });

    connect(dirDialog, &FlyDirDialog::rejected, [dirDialog]() {
        dirDialog->deleteLater();
    });

    // Показываем диалог
    dirDialog->exec();
}

void MainWindow::onUnmountPartitionClicked()
{
    // Получаем выбранный элемент
    QTreeWidgetItem *item = ui->treeDevices->currentItem();
    if (!item) {
        QMessageBox::warning(this, tr("No Device Selected"),
                            tr("Please select a partition or RAID array to unmount."));
        return;
    }

    // Проверяем, можно ли размонтировать выбранное устройство
    if (!isSelectedItemMountable()) {
        QMessageBox::warning(this, tr("Invalid Selection"),
                            tr("Please select a partition or RAID array, not a disk."));
        return;
    }

    QString devicePath = getSelectedDevicePath();
    if (devicePath.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Device"),
                            tr("Cannot determine device path."));
        return;
    }

    // Проверяем, смонтировано ли устройство
    if (!m_diskManager->isDeviceMounted(devicePath)) {
        QMessageBox::information(this, tr("Not Mounted"),
                                tr("Device %1 is not mounted.")
                                   .arg(devicePath));
        return;
    }

    QString mountPoint = m_diskManager->getMountPoint(devicePath);

    // Запрашиваем подтверждение
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("Confirm Unmount"),
                                 tr("Unmount device %1 from %2?\n\n"
                                    "Make sure all applications using this device are closed.")
                                     .arg(devicePath)
                                     .arg(mountPoint),
                                 QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        // Размонтируем устройство
        m_diskManager->unmountDevice(devicePath);
    }
}

void MainWindow::onDeviceMounted(bool success, const QString &devicePath, const QString &mountPoint)
{
    if (success) {
        ui->statusBar->showMessage(tr("Device %1 mounted successfully at %2")
                                  .arg(devicePath)
                                  .arg(mountPoint), 5000);
    } else {
        ui->statusBar->showMessage(tr("Failed to mount device %1")
                                  .arg(devicePath), 5000);
    }
}

void MainWindow::onDeviceUnmounted(bool success, const QString &devicePath)
{
    if (success) {
        ui->statusBar->showMessage(tr("Device %1 unmounted successfully")
                                  .arg(devicePath), 5000);
    } else {
        ui->statusBar->showMessage(tr("Failed to unmount device %1")
                                  .arg(devicePath), 5000);
    }
}

bool MainWindow::isSelectedItemRaid() const
{
    QTreeWidgetItem *item = ui->treeDevices->currentItem();
    if (!item) {
        return false;
    }

    QString devicePath = item->text(0);
    return devicePath.contains("/dev/md") && item->parent() == nullptr;
}

bool MainWindow::isSelectedItemMountable() const
{
    return isSelectedItemPartition() || isSelectedItemRaid();
}

QString MainWindow::getSelectedDevicePath() const
{
    QTreeWidgetItem *item = ui->treeDevices->currentItem();
    if (!item) {
        return QString();
    }

    return item->text(0);
}

void MainWindow::onRefreshDevices()
{
    // Показываем сообщение о начале обновления в статусной строке
    ui->statusBar->showMessage(tr("Refreshing device list..."));

    // Очищаем таблицу
    ui->treeDevices->clear();

    // Запускаем обновление списка устройств
    m_diskManager->refreshDevices();
}

void MainWindow::onViewModeChanged(int index)
{
    // Обновляем режим отображения
    m_showAllDevices = (index == 0); // 0 = All Devices, 1 = RAID Only

    // Обновляем интерфейс
    updateInterface();
}

void MainWindow::onDevicesRefreshed(bool success)
{
    if (success) {
        ui->statusBar->showMessage(tr("Device list updated successfully"), 3000);

        // Обновляем таблицу
        updateInterface();
    } else {
        ui->statusBar->showMessage(tr("Failed to update device list"), 3000);
    }
}

void MainWindow::onCommandOutput(const QString &output)
{
    // Добавляем вывод в лог
    ui->txtOperationLog->append(output);
}

void MainWindow::onMarkFaultyClicked()
{
    // Получаем выбранный элемент
    QTreeWidgetItem *item = ui->treeDevices->currentItem();
    if (!item) {
        QMessageBox::warning(this, tr("No Device Selected"),
                            tr("Please select a RAID member device to mark as faulty."));
        return;
    }

    // Проверяем, является ли выбранное устройство членом RAID
    if (!isSelectedItemRaidMember()) {
        QMessageBox::warning(this, tr("Invalid Selection"),
                            tr("Please select a device that is a member of a RAID array."));
        return;
    }

    if (item->parent()->text(2) == "RAID0") {
        QMessageBox::warning(this, tr("Invalid Selection"),
                            tr("Cannot mark faulty a RAID0 member."));
        return;
    }

    QString memberDevice = getSelectedDevicePath();
    QString raidDevice = getSelectedRaidDevice();

    if (memberDevice.isEmpty() || raidDevice.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Device"),
                            tr("Cannot determine device or RAID array path."));
        return;
    }

    // Запрашиваем подтверждение
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("Confirm Mark Faulty"),
                                 tr("Mark device %1 as faulty in RAID array %2?\n\n"
                                    "This will cause the RAID array to operate in degraded mode "
                                    "and the device will be excluded from the array.\n\n")
                                     .arg(memberDevice)
                                     .arg(raidDevice),
                                 QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        // Помечаем устройство как сбойное
        m_diskManager->markDeviceAsFaulty(raidDevice, memberDevice);
    }
}

void MainWindow::onAddToRaidClicked()
{
    // Получаем выбранный элемент
    QTreeWidgetItem *item = ui->treeDevices->currentItem();
    if (!item) {
        QMessageBox::warning(this, tr("No Device Selected"),
                            tr("Please select a partition or disk to add to a RAID array."));
        return;
    }

    QString devicePath = getSelectedDevicePath();
    if (devicePath.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Device"),
                            tr("Cannot determine device path."));
        return;
    }

    bool isDisk = (devicePath.contains("/dev/sd") || devicePath.contains("/dev/nvme") ||
                   devicePath.contains("/dev/vd") || devicePath.contains("/dev/hd")) &&
                  item->parent() == nullptr;

    // Проверяем, можно ли добавить выбранное устройство в RAID
    if (!isSelectedItemPartition() && !isDisk) {
        QMessageBox::warning(this, tr("Invalid Selection"),
                            tr("Please select a partition or unpartitioned disk."));
        return;
    }

    // Проверяем, что устройство не смонтировано и не в RAID
    if (m_diskManager->isDeviceMounted(devicePath)) {
        QMessageBox::warning(this, tr("Device Mounted"),
                            tr("Device %1 is currently mounted. Please unmount it first.")
                               .arg(devicePath));
        return;
    }

    const DiskStructure& diskStructure = m_diskManager->getDiskStructure();
    // Для разделов проверяем, не является ли он членом RAID
    if (isSelectedItemPartition()) {
        for (const DiskInfo &disk : diskStructure.getDisks()) {
            for (const PartitionInfo &partition : disk.partitions) {
                if (partition.devicePath == devicePath && partition.isRaidMember) {
                    QMessageBox::warning(this, tr("Already in RAID"),
                                        tr("Device %1 is already a member of a RAID array.")
                                           .arg(devicePath));
                    return;
                }
            }
        }
    }

    // Получаем список доступных RAID массивов
    QStringList availableRaids;

    for (const RaidInfo &raid : diskStructure.getRaids()) {
        // Добавляем только активные RAID массивы
        if ((raid.state.contains("active", Qt::CaseInsensitive) || raid.state.contains("clean", Qt::CaseInsensitive)) && raid.type != RaidType::RAID0) {
            QString raidInfo = tr("%1 (%2, %3)")
                              .arg(raid.devicePath)
                              .arg(DiskUtils::raidTypeToString(raid.type))
                              .arg(raid.size);
            availableRaids.append(raidInfo);
        }
    }

    if (availableRaids.isEmpty()) {
        QMessageBox::information(this, tr("No RAID Arrays"),
                                tr("No active RAID arrays available to add devices to."));
        return;
    }

    // Показываем диалог выбора RAID массива
    bool ok;
    QString selectedRaidInfo = QInputDialog::getItem(this, tr("Select RAID Array"),
                                                    tr("Select RAID array to add device %1 to:")
                                                       .arg(devicePath),
                                                    availableRaids, 0, false, &ok);

    if (!ok || selectedRaidInfo.isEmpty()) {
        return;
    }

    // Извлекаем путь к RAID устройству из выбранной строки
    QString raidDevice = selectedRaidInfo.split(' ').first();

    // Запрашиваем финальное подтверждение
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("Confirm Add Device"),
                                 tr("Add device %1 to RAID array %2?\n\n"
                                    "This will:\n"
                                    "1) Wipe all data from %1\n"
                                    "2) Add it as a member of the RAID array\n"
                                    "3) Start automatic rebuilding/syncing process\n\n"
                                    "All data on %1 will be permanently lost!")
                                     .arg(devicePath)
                                     .arg(raidDevice),
                                 QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        // Добавляем устройство в RAID
        m_diskManager->addDeviceToRaid(raidDevice, devicePath);
    }
}

void MainWindow::onRemoveFromRaidClicked()
{
    // Получаем выбранный элемент
    QTreeWidgetItem *item = ui->treeDevices->currentItem();
    if (!item) {
        QMessageBox::warning(this, tr("No Device Selected"),
                            tr("Please select a RAID member device to remove from the array."));
        return;
    }

    // Проверяем, является ли выбранное устройство членом RAID
    if (!isSelectedItemRaidMember()) {
        QMessageBox::warning(this, tr("Invalid Selection"),
                            tr("Please select a device that is a member of a RAID array."));
        return;
    }

    if (item->parent()->text(2) == "RAID0") {
        QMessageBox::warning(this, tr("Invalid Selection"),
                            tr("Cannot remove a RAID0 member."));
        return;
    }

    QString memberDevice = getSelectedDevicePath();
    QString raidDevice = getSelectedRaidDevice();

    if (memberDevice.isEmpty() || raidDevice.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Device"),
                            tr("Cannot determine device or RAID array path."));
        return;
    }

    // Запрашиваем подтверждение
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("Confirm Remove Device"),
                                 tr("Remove device %1 from RAID array %2?\n\n"
                                    "This will:\n"
                                    "1) Remove it from the RAID array\n"
                                    "2) The device will become available for other uses\n\n"
                                    "The RAID array will continue operating in degraded mode "
                                    "until a replacement device is added.")
                                     .arg(memberDevice)
                                     .arg(raidDevice),
                                 QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        // Удаляем устройство из RAID
        m_diskManager->removeDeviceFromRaid(raidDevice, memberDevice);
    }
}

void MainWindow::onCreatePartitionTableClicked()
{
    // Получаем выбранный элемент
    QTreeWidgetItem *item = ui->treeDevices->currentItem();
    if (!item) {
        QMessageBox::warning(this, tr("No Device Selected"),
                            tr("Please select a disk to create a partition table."));
        return;
    }

    // Получаем путь к устройству
    QString devicePath = item->text(0);

    // Проверяем, является ли выбранное устройство диском
    bool isDisk = devicePath.contains("/dev/sd") || devicePath.contains("/dev/nvme") ||
                  devicePath.contains("/dev/vd") || devicePath.contains("/dev/hd");

    if (!isDisk || item->parent() != nullptr) {
        QMessageBox::warning(this, tr("Invalid Selection"),
                            tr("Please select a disk, not a partition or RAID array."));
        return;
    }

    // Создаем и показываем диалог
    PartitionTableDialog *dialog = new PartitionTableDialog(devicePath, this);
    if (dialog->exec() == QDialog::Accepted) {
        // Получаем выбранный тип таблицы разделов
        QString tableType = dialog->selectedTableType();

        // Запрашиваем подтверждение
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, tr("Confirm Action"),
                                     tr("Are you sure you want to create a new %1 partition table on %2?\n\n"
                                        "All existing data on the disk will be lost!")
                                         .arg(tableType.toUpper())
                                         .arg(devicePath),
                                     QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            // Создаем таблицу разделов
            m_diskManager->createPartitionTable(devicePath, tableType);
        }
    }
    dialog->deleteLater();
}

void MainWindow::onCreatePartitionClicked()
{
    // Получаем выбранный элемент
    QTreeWidgetItem *item = ui->treeDevices->currentItem();
    if (!item) {
        QMessageBox::warning(this, tr("No Device Selected"),
                            tr("Please select a disk to create a partition."));
        return;
    }

    // Получаем путь к устройству
    QString devicePath = item->text(0);

    // Проверяем, является ли выбранное устройство диском
    bool isDisk = devicePath.contains("/dev/sd") || devicePath.contains("/dev/nvme") ||
                  devicePath.contains("/dev/vd") || devicePath.contains("/dev/hd");

    if (!isDisk || item->parent() != nullptr) {
        QMessageBox::warning(this, tr("Invalid Selection"),
                            tr("Please select a disk, not a partition or RAID array."));
        return;
    }

    // Создаем диалог создания раздела
    CreatePartitionDialog *dialog = new CreatePartitionDialog(devicePath, this);

    // Подключаем сигнал для получения информации о свободном пространстве
    connect(m_diskManager, &DiskManager::freeSpaceOnDeviceInfoReceived,
            dialog, &CreatePartitionDialog::onFreeSpaceInfoOnDeviceReceived);

    // Запрашиваем информацию о свободном пространстве
    m_diskManager->getFreeSpaceOnDeviceInfo(devicePath);

    // Показываем диалог
    if (dialog->exec() == QDialog::Accepted) {
        // Дополнительная валидация перед созданием раздела
        if (dialog->validateInput()) {
            // Получаем данные из диалога
            QString partitionType = dialog->getPartitionType();
            QString filesystemType = dialog->getFilesystemType();
            QString startSize = dialog->getStartSize();
            QString endSize = dialog->getEndSize();

            // Запрашиваем подтверждение
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this, tr("Confirm Partition Creation"),
                                         tr("Create %1 partition from %2 to %3?\n\n"
                                            "Partition type: %4\n"
                                            "Filesystem: %5")
                                             .arg(partitionType)
                                             .arg(startSize)
                                             .arg(endSize)
                                             .arg(partitionType)
                                             .arg(filesystemType),
                                         QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                // Создаем раздел
                m_diskManager->createPartition(devicePath, partitionType, filesystemType,
                                              startSize, endSize);
            }
        }
    }

    dialog->deleteLater();
}

void MainWindow::onDeletePartitionClicked()
{
    // Получаем выбранный элемент
    QTreeWidgetItem *item = ui->treeDevices->currentItem();
    if (!item) {
        QMessageBox::warning(this, tr("No Partition Selected"),
                            tr("Please select a partition to delete."));
        return;
    }

    // Проверяем, является ли выбранное устройство разделом
    if (!isSelectedItemPartition()) {
        QMessageBox::warning(this, tr("Invalid Selection"),
                            tr("Please select a partition, not a disk or RAID array."));
        return;
    }

    QString partitionPath = getSelectedPartitionPath();
    if (partitionPath.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Partition"),
                            tr("Cannot determine partition path."));
        return;
    }

    // Проверяем, не смонтирован ли раздел
    QString mountPoint = item->text(5); // Колонка Mount Point
    if (!mountPoint.isEmpty()) {
        QMessageBox::warning(this, tr("Partition is Mounted"),
                            tr("Cannot delete partition %1 because it is currently mounted at %2.\n\n"
                               "Please unmount the partition first.")
                               .arg(partitionPath)
                               .arg(mountPoint));
        return;
    }

    // Создаем и показываем диалог подтверждения
    DeletePartitionDialog *dialog = new DeletePartitionDialog(partitionPath, this);
    if (dialog->exec() == QDialog::Accepted && dialog->isConfirmed()) {
        // Удаляем раздел
        m_diskManager->deletePartition(partitionPath);
    }
    dialog->deleteLater();
}

void MainWindow::onFormatPartitionClicked()
{
    // Получаем выбранный элемент
    QTreeWidgetItem *item = ui->treeDevices->currentItem();
    if (!item) {
        QMessageBox::warning(this, tr("No Partition Selected"),
                            tr("Please select a partition to format."));
        return;
    }

    // Проверяем, является ли выбранное устройство разделом
    if (!isSelectedItemPartition() && !isSelectedItemRaid()) {
        QMessageBox::warning(this, tr("Invalid Selection"),
                            tr("Please select a partition, not a disk or RAID array."));
        return;
    }

    QString partitionPath = getSelectedPartitionPath();
    if (partitionPath.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Partition"),
                            tr("Cannot determine partition path."));
        return;
    }

    bool isMounted = m_diskManager->isDeviceMounted(item->text(0));
    if (isMounted) {
        QMessageBox::warning(this, tr("Partition is Mounted"),
                            tr("Cannot format partition %1 because it is currently mounted.\n\n"
                               "Please unmount the partition first.")
                               .arg(partitionPath));
        return;
    }

    // Создаем и показываем диалог форматирования
    FormatPartitionDialog *dialog = new FormatPartitionDialog(partitionPath, this);
    if (dialog->exec() == QDialog::Accepted && dialog->isConfirmed()) {
        // Получаем параметры форматирования
        QString filesystem = dialog->getSelectedFilesystem();
        QString label = dialog->getVolumeLabel();

        // Запрашиваем финальное подтверждение
        QMessageBox::StandardButton reply;
        QString message = tr("Format partition %1 with %2 filesystem?")
                         .arg(partitionPath)
                         .arg(filesystem.toUpper());

        if (!label.isEmpty()) {
            message += tr("\nVolume label: %1").arg(label);
        }

        message += tr("\n\nThis will permanently erase all data on the partition!");

        reply = QMessageBox::question(this, tr("Confirm Format"),
                                     message,
                                     QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            // Форматируем раздел
            m_diskManager->formatPartition(partitionPath, filesystem, label);
        }
    }
    dialog->deleteLater();
}

bool MainWindow::isSelectedItemPartition() const
{
    QTreeWidgetItem *item = ui->treeDevices->currentItem();
    if (!item) {
        return false;
    }

    // Раздел должен иметь родительский элемент (диск) и содержать цифры в конце пути
    QString devicePath = item->text(0);
    return item->parent() != nullptr &&
           devicePath.contains(QRegularExpression("/dev/[a-z]+[0-9]+$"));
}

QString MainWindow::getSelectedPartitionPath() const
{
    QTreeWidgetItem *item = ui->treeDevices->currentItem();
    if (!item) {
        return QString();
    }

    return item->text(0);
}

void MainWindow::updateInterface()
{
    const DiskStructure& diskStructure = m_diskManager->getDiskStructure();

    ui->treeDevices->clear();

    if (m_showAllDevices) {
        populateAllDevicesView(diskStructure);
    } else {
        populateRaidView(diskStructure);
    }

    updateButtonState();
}

void MainWindow::populateAllDevicesView(const DiskStructure& diskStructure)
{
    // Заголовки колонок для режима "Все устройства"
    ui->treeDevices->setHeaderLabels(QStringList()
                                    << tr("Device")
                                    << tr("Size")
                                    << tr("Filesystem")
                                    << tr("Used")
                                    << tr("Free")
                                    << tr("Mount Point")
                                    << tr("Flags")
                                    << tr("Status"));

    // Получаем список дисков
    for (const DiskInfo &disk : diskStructure.getDisks()) {
        // Добавляем диск в таблицу
        QTreeWidgetItem *diskItem = addDiskToTree(disk);

        // Добавляем разделы
        for (const PartitionInfo &partition : disk.partitions) {
            addPartitionToTree(diskItem, partition);
        }
    }

    // Получаем список RAID-массивов
    for (const RaidInfo &raid : diskStructure.getRaids()) {
        // Добавляем RAID в таблицу
        addRaidToTree(raid);
    }

    // Разворачиваем верхний уровень
    ui->treeDevices->expandToDepth(0);
}

void MainWindow::populateRaidView(const DiskStructure& diskStructure)
{
    // Заголовки колонок для режима "Только RAID"
    ui->treeDevices->setHeaderLabels(QStringList()
                                    << tr("Device")
                                    << tr("Size")
                                    << tr("Type")
                                    << tr("Status")
                                    << tr("Mount Point")
                                    << tr("Filesystem")
                                    << tr("Sync")
                                    << tr("State"));

    // Получаем список RAID-массивов
    for (const RaidInfo &raid : diskStructure.getRaids()) {
        // Добавляем RAID в таблицу
        QTreeWidgetItem *raidItem = addRaidToTree(raid);

        // Добавляем устройства
        for (const RaidMemberInfo &member : raid.members) {
            addRaidMemberToTree(raidItem, member);
        }
    }

    // Разворачиваем весь список
    ui->treeDevices->expandAll();
}

QTreeWidgetItem* MainWindow::addDiskToTree(const DiskInfo &disk)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeDevices);

    // Устанавливаем данные
    item->setText(0, disk.devicePath);

    // Форматируем размер через DiskUtils
    qint64 sizeBytes = DiskUtils::parseSizeString(disk.size);
    QString formattedSize = DiskUtils::formatByteSize(sizeBytes);
    item->setText(1, formattedSize);

    // Иконка диска
    item->setIcon(0, QIcon::fromTheme("drive-harddisk"));

    return item;
}

QTreeWidgetItem* MainWindow::addPartitionToTree(QTreeWidgetItem* parentItem, const PartitionInfo &partition)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(parentItem);

    // Устанавливаем данные
    item->setText(0, partition.devicePath);

    // Форматируем размер через DiskUtils
    qint64 sizeBytes = DiskUtils::parseSizeString(partition.size);
    QString formattedSize = DiskUtils::formatByteSize(sizeBytes);
    item->setText(1, formattedSize);

    item->setText(2, partition.filesystem);

    // Форматируем используемое пространство
    if (!partition.used.isEmpty() && partition.used != "-") {
        qint64 usedBytes = DiskUtils::parseSizeString(partition.used);
        QString formattedUsed = DiskUtils::formatByteSize(usedBytes);
        item->setText(3, formattedUsed);
    } else {
        item->setText(3, "-");
    }

    // Форматируем свободное пространство
    if (!partition.available.isEmpty() && partition.available != "-") {
        qint64 availableBytes = DiskUtils::parseSizeString(partition.available);
        QString formattedAvailable = DiskUtils::formatByteSize(availableBytes);
        item->setText(4, formattedAvailable);
    } else {
        item->setText(4, "-");
    }

    item->setText(5, partition.mountPoint);
    item->setText(6, partition.flags.join(", "));

    // Для разделов, входящих в RAID, устанавливаем специальный флаг
    if (partition.isRaidMember) {
        item->setText(7, tr("RAID member"));
        // Иконка для RAID-члена
        item->setIcon(0, QIcon::fromTheme("drive-removable-media"));
    } else {
        // Иконка для раздела
        item->setIcon(0, QIcon::fromTheme("drive-harddisk-partition"));
    }

    return item;
}

QTreeWidgetItem* MainWindow::addRaidToTree(const RaidInfo &raid)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeDevices);

    // Устанавливаем данные
    item->setText(0, raid.devicePath);

    // Форматируем размер через DiskUtils
    qint64 sizeBytes = DiskUtils::parseSizeString(raid.size);
    QString formattedSize = DiskUtils::formatByteSize(sizeBytes);
    item->setText(1, formattedSize);

    // В зависимости от режима отображения устанавливаем разные колонки
    if (m_showAllDevices) {
        item->setText(2, raid.filesystem);
        item->setText(5, raid.mountPoint);
        item->setText(7, raid.state);
    } else {
        item->setText(2, DiskUtils::raidTypeToString(raid.type));
        item->setText(3, raid.state);
        item->setText(4, raid.mountPoint);
        item->setText(5, raid.filesystem);
        item->setText(6, tr("%1%").arg(raid.syncPercent));
    }

    // Иконка для RAID
    item->setIcon(0, QIcon::fromTheme("drive-harddisk-raid"));

    return item;
}

QTreeWidgetItem* MainWindow::addRaidMemberToTree(QTreeWidgetItem* parentItem, const RaidMemberInfo &member)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(parentItem);

    // Устанавливаем данные
    item->setText(0, member.devicePath);

    // В режиме RAID отображаем статус устройства
    if (!m_showAllDevices) {
        item->setText(3, DiskUtils::deviceStatusToString(member.status));
    }

    // Иконка для устройства RAID
    item->setIcon(0, QIcon::fromTheme("drive-removable-media"));

    // Для неисправных устройств устанавливаем специальную иконку
    if (member.status == DeviceStatus::FAULTY) {
        item->setIcon(0, QIcon::fromTheme("dialog-warning"));
    }

    return item;
}

bool MainWindow::isSelectedItemRaidMember() const
{
    QTreeWidgetItem *item = ui->treeDevices->currentItem();
    if (!item) {
        return false;
    }

    QTreeWidgetItem *parent = item->parent();
    if (!parent) {
        return false;
    }

    QString parentPath = parent->text(0);
    QString childFsText = item->text(7);
    if (parentPath.contains("/dev/md") || childFsText.contains("member", Qt::CaseInsensitive)) {
        return true;
    }
    return false;

    // Если элемент родитель (диск), проверяем всех его детей
   /* for (int i = 0; i < item->childCount(); ++i) {
        QTreeWidgetItem *child = item->child(i);
        QString childStatusText = child->text(2); // Колонка Status
        if (childStatusText.contains("member", Qt::CaseInsensitive)) {
            return true; // Хотя бы один дочерний элемент является членом RAID
        }
    }*/
}

QString MainWindow::getSelectedRaidDevice() const
{
    QTreeWidgetItem *item = ui->treeDevices->currentItem();
    if (!item) {
        return QString();
    }

    // Если выбрано устройство-член RAID, получаем путь к родительскому RAID
    QTreeWidgetItem *parent = item->parent();
    if (parent) {
        QString parentPath = parent->text(0);
        if (parentPath.contains("/dev/md")) {
            return parentPath;
        }
    }

    return QString();
}

void MainWindow::updateButtonState()
{
    // Получаем выбранный элемент
    QTreeWidgetItem *item = ui->treeDevices->currentItem();

    // По умолчанию все кнопки недоступны
    ui->btnMarkFaulty->setEnabled(false);
    ui->btnAddToRaid->setEnabled(false);
    ui->btnRemoveFromRaid->setEnabled(false);
    ui->btnActivateSpare->setEnabled(false);

    // Обновляем доступность действий в меню
    ui->actionDeletePartition->setEnabled(false);
    ui->actionFormatPartition->setEnabled(false);
    ui->actionCreatePartition->setEnabled(false);
    ui->actionCreatePartitionTable->setEnabled(false);
    ui->actionMountPartition->setEnabled(false);
    ui->actionUnmountPartition->setEnabled(false);

    if (!item || !m_diskManager) {
        return;
    }

    // Получаем путь к устройству
    QString devicePath = item->text(0);

    // Проверяем тип выбранного элемента
    bool isDisk = (devicePath.contains("/dev/sd") || devicePath.contains("/dev/nvme") ||
                   devicePath.contains("/dev/vd") || devicePath.contains("/dev/hd")) &&
                  item->parent() == nullptr;
    bool isPartition = isSelectedItemPartition();
    bool isRaid = isSelectedItemRaid();
    bool isRaidMember = isSelectedItemRaidMember(); // Используем новый метод

    // ОТЛАДОЧНАЯ ИНФОРМАЦИЯ
    qDebug() << "=== DEBUG updateButtonState ===";
    qDebug() << "Device path:" << devicePath;
    qDebug() << "isDisk:" << isDisk;
    qDebug() << "isPartition:" << isPartition;
    qDebug() << "isRaid:" << isRaid;
    qDebug() << "isRaidMember:" << isRaidMember;

    // Активируем действия для дисков
    if (isDisk) {
        ui->actionCreatePartition->setEnabled(true);
        ui->actionCreatePartitionTable->setEnabled(true);
    }

    // Активируем действия для разделов (только в режиме "Все устройства")
    if (isPartition && m_showAllDevices) {
        QString mountPoint = item->text(5); // Колонка Mount Point
        bool isMounted = !mountPoint.isEmpty();

        // Запрещаем операции для партиций, входящих в RAID
        if (!isRaidMember) {
            ui->actionDeletePartition->setEnabled(!isMounted);
            ui->actionFormatPartition->setEnabled(!isMounted);
        }
    }

    // Форматирование доступно для RAID массивов (но не их членов)
    if (isRaid && !isRaidMember) {
        ui->actionFormatPartition->setEnabled(true);
    }

    // Логика монтирования (существующая)
    bool isMounted = m_diskManager->isDeviceMounted(devicePath);
    QString filesystem = m_diskManager->getDeviceFilesystem(devicePath);
    bool hasFilesystem = !filesystem.isEmpty() &&
                        filesystem != "unknown" &&
                        filesystem != "-" &&
                        !filesystem.isNull();

    QString uiFilesystem = item->text(2);
    bool hasUiFilesystem = !uiFilesystem.isEmpty() &&
                          uiFilesystem != "-" &&
                          uiFilesystem != "unknown" &&
                          !uiFilesystem.isNull();

    if (!hasFilesystem && hasUiFilesystem) {
        hasFilesystem = hasUiFilesystem;
    }

    if (m_showAllDevices) {
        if (isPartition && !isRaidMember) {
            ui->actionMountPartition->setEnabled(!isMounted && hasFilesystem);
            ui->actionUnmountPartition->setEnabled(isMounted);
        } else if (isRaid && !isRaidMember) {
            ui->actionMountPartition->setEnabled(!isMounted && hasFilesystem);
            ui->actionUnmountPartition->setEnabled(isMounted);
        }
    } else {
        if (isRaid && !isRaidMember) {
            ui->actionMountPartition->setEnabled(!isMounted && hasFilesystem);
            ui->actionUnmountPartition->setEnabled(isMounted);
        }
    }

    // Кнопка "Пометить как неисправное" доступна только для членов RAID
    ui->btnMarkFaulty->setEnabled(isRaidMember && item->parent()->text(2) != "RAID0");
    // Кнопка "Добавить в RAID" доступна только для разделов, не входящих в RAID
    ui->btnAddToRaid->setEnabled(isPartition && !isRaidMember);

    // Кнопка "Удалить из RAID" доступна только для устройств в RAID
    ui->btnRemoveFromRaid->setEnabled(isRaidMember && item->parent()->text(2) != "RAID0");

    // Обновляем доступность действия удаления RAID
    bool isRaidArray = isSelectedItemRaid();
    ui->actionDestroyRaid->setEnabled(isRaidArray);

    if (isRaidMember) {
        // Проверяем статус устройства в RAID
        QString deviceStatus = item->text(3); // Колонка Status
        bool isFaulty = deviceStatus.contains("faulty", Qt::CaseInsensitive);

        // Разрешаем удаление только для неактивных членов RAID
        ui->btnRemoveFromRaid->setEnabled(isFaulty);

        if (!isFaulty) {
            ui->btnRemoveFromRaid->setToolTip(tr("Cannot working device. First mark it as faulty."));
        } else {
            ui->btnRemoveFromRaid->setToolTip(tr("Remove device from RAID array"));
        }
    } else {
        ui->btnRemoveFromRaid->setEnabled(false);
        ui->btnRemoveFromRaid->setToolTip(tr("Select a RAID member device to remove"));
    }

    bool canAddToRaid = false;

    if (isPartition && !isRaidMember && !isMounted) {
        canAddToRaid = true;
    } else if (isDisk && !isRaidMember && !isMounted) {
        canAddToRaid = (item->childCount() == 0);
    }

    ui->btnAddToRaid->setEnabled(canAddToRaid);

    ui->btnActivateSpare->setEnabled(item->text(3).contains("spare", Qt::CaseInsensitive));

    if (canAddToRaid) {
        ui->btnAddToRaid->setToolTip(tr("Add device to existing RAID array"));
    } else if (isMounted) {
        ui->btnAddToRaid->setToolTip(tr("Cannot add mounted device to RAID"));
    } else if (isRaidMember) {
        ui->btnAddToRaid->setToolTip(tr("Device is already in RAID"));
    } else {
        ui->btnAddToRaid->setToolTip(tr("Select an available partition or unpartitioned disk"));
    }

    qDebug() << "===============================";
}
