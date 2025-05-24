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

    // Подключаем кнопки управления RAID
    connect(ui->btnMarkFaulty, &QPushButton::clicked,
            this, &MainWindow::onMarkFaultyClicked);
    connect(ui->btnAddToRaid, &QPushButton::clicked,
            this, &MainWindow::onAddToRaidClicked);
    connect(ui->btnRemoveFromRaid, &QPushButton::clicked,
            this, &MainWindow::onRemoveFromRaidClicked);

    // Подключаем сигнал изменения выбора в дереве для обновления кнопок
    connect(ui->treeDevices, &QTreeWidget::currentItemChanged,
            this, &MainWindow::updateButtonState);

    // Настраиваем интерфейс
    updateInterface();

    // Обновляем список устройств при запуске
    onRefreshDevices();
}

MainWindow::~MainWindow()
{
    delete ui;
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
        return;
    }

    // В базовой версии просто показываем сообщение
    QString devicePath = item->text(0);
    QMessageBox::information(this, tr("Mark Faulty"),
                           tr("Would mark device %1 as faulty").arg(devicePath));

    // Тут в будущем будет функциональность пометки устройства как неисправного
}

void MainWindow::onAddToRaidClicked()
{
    // Получаем выбранный элемент
    QTreeWidgetItem *item = ui->treeDevices->currentItem();
    if (!item) {
        return;
    }

    // В базовой версии просто показываем сообщение
    QString devicePath = item->text(0);
    QMessageBox::information(this, tr("Add to RAID"),
                           tr("Would add device %1 to RAID").arg(devicePath));

    // Тут в будущем будет функциональность добавления устройства в RAID
}

void MainWindow::onRemoveFromRaidClicked()
{
    // Получаем выбранный элемент
    QTreeWidgetItem *item = ui->treeDevices->currentItem();
    if (!item) {
        return;
    }

    // В базовой версии просто показываем сообщение
    QString devicePath = item->text(0);
    QMessageBox::information(this, tr("Remove from RAID"),
                           tr("Would remove device %1 from RAID").arg(devicePath));

    // Тут в будущем будет функциональность удаления устройства из RAID
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
    // Получаем текущую структуру дисков
    DiskStructure diskStructure = m_diskManager->getDiskStructure();

    // Очищаем таблицу
    ui->treeDevices->clear();

    // В зависимости от режима отображения заполняем таблицу
    if (m_showAllDevices) {
        // Показываем все устройства
        populateAllDevicesView();
    } else {
        // Показываем только RAID
        populateRaidView();
    }

    // Обновляем доступность кнопок
    updateButtonState();
}

void MainWindow::populateAllDevicesView()
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
    for (const DiskInfo &disk : m_diskManager->getDiskStructure().getDisks()) {
        // Добавляем диск в таблицу
        QTreeWidgetItem *diskItem = addDiskToTree(disk);

        // Добавляем разделы
        for (const PartitionInfo &partition : disk.partitions) {
            addPartitionToTree(diskItem, partition);
        }
    }

    // Получаем список RAID-массивов
    for (const RaidInfo &raid : m_diskManager->getDiskStructure().getRaids()) {
        // Добавляем RAID в таблицу
        addRaidToTree(raid);
    }

    // Разворачиваем верхний уровень
    ui->treeDevices->expandToDepth(0);
}

void MainWindow::populateRaidView()
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
    for (const RaidInfo &raid : m_diskManager->getDiskStructure().getRaids()) {
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
    item->setText(1, disk.size);

    // Иконка диска
    item->setIcon(0, QIcon::fromTheme("drive-harddisk"));

    return item;
}

QTreeWidgetItem* MainWindow::addPartitionToTree(QTreeWidgetItem* parentItem, const PartitionInfo &partition)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(parentItem);

    // Устанавливаем данные
    item->setText(0, partition.devicePath);
    item->setText(1, partition.size);
    item->setText(2, partition.filesystem);
    item->setText(3, partition.used);
    item->setText(4, partition.available);
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
    item->setText(1, raid.size);

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

void MainWindow::updateButtonState()
{
    // Получаем выбранный элемент
    QTreeWidgetItem *item = ui->treeDevices->currentItem();

    // По умолчанию все кнопки недоступны
    ui->btnMarkFaulty->setEnabled(false);
    ui->btnAddToRaid->setEnabled(false);
    ui->btnRemoveFromRaid->setEnabled(false);

    // Обновляем доступность действий в меню
    ui->actionDeletePartition->setEnabled(false);
    ui->actionCreatePartition->setEnabled(false);
    ui->actionCreatePartitionTable->setEnabled(false);

    if (!item) {
        return;
    }

    // Получаем путь к устройству
    QString devicePath = item->text(0);

    // Проверяем тип выбранного элемента
    bool isDisk = (devicePath.contains("/dev/sd") || devicePath.contains("/dev/nvme") ||
                   devicePath.contains("/dev/vd") || devicePath.contains("/dev/hd")) &&
                  item->parent() == nullptr;
    bool isPartition = isSelectedItemPartition();
    bool isRaid = devicePath.contains("md");
    bool isRaidMember = item->parent() && item->parent()->text(0).contains("md");

    // Активируем действия для дисков
    if (isDisk) {
        ui->actionCreatePartition->setEnabled(true);
        ui->actionCreatePartitionTable->setEnabled(true);
    }

    // Активируем действие удаления для разделов (не смонтированных)
    if (isPartition) {
        QString mountPoint = item->text(5); // Колонка Mount Point
        ui->actionDeletePartition->setEnabled(mountPoint.isEmpty());
    }

    // Кнопка "Пометить как неисправное" доступна только для устройств в RAID
    ui->btnMarkFaulty->setEnabled(isRaidMember);

    // Кнопка "Добавить в RAID" доступна только для разделов, не входящих в RAID
    ui->btnAddToRaid->setEnabled(isPartition && !isRaidMember);

    // Кнопка "Удалить из RAID" доступна только для устройств в RAID
    ui->btnRemoveFromRaid->setEnabled(isRaidMember);
}
