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

    // Подключаем кнопки управления RAID
    connect(ui->btnMarkFaulty, &QPushButton::clicked,
            this, &MainWindow::onMarkFaultyClicked);
    connect(ui->btnAddToRaid, &QPushButton::clicked,
            this, &MainWindow::onAddToRaidClicked);
    connect(ui->btnRemoveFromRaid, &QPushButton::clicked,
            this, &MainWindow::onRemoveFromRaidClicked);

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
        item->setText(6, QString::number(raid.syncPercent) + "%");
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
    PartitionTableDialog dialog(devicePath, this);
    if (dialog.exec() == QDialog::Accepted) {
        // Получаем выбранный тип таблицы разделов
        QString tableType = dialog.selectedTableType();

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

    // Определение типа устройства для создания раздела
    bool canCreatePartition = false;

    // Проверяем, является ли выбранное устройство диском
    bool isDisk = devicePath.contains("/dev/sd") || devicePath.contains("/dev/nvme") ||
                 devicePath.contains("/dev/vd") || devicePath.contains("/dev/hd");

    // Проверяем, является ли выбранное устройство разделом типа "extended"
    bool isExtendedPartition = false;
    if (item->parent() && item->text(6).contains("extended", Qt::CaseInsensitive)) {
        isExtendedPartition = true;
    }

    // Можно создать раздел на диске или в расширенном разделе
    canCreatePartition = (isDisk && item->parent() == nullptr) || isExtendedPartition;

    if (!canCreatePartition) {
        QMessageBox::warning(this, tr("Invalid Selection"),
                           tr("Partitions can only be created on disks or inside extended partitions."));
        return;
    }

    // Создаем и показываем диалог
    CreatePartitionDialog dialog(devicePath, this);
    if (dialog.exec() == QDialog::Accepted) {
        // Получаем параметры раздела
        QString partType = dialog.partitionType();
        QString start = dialog.startPosition();
        QString end = dialog.endPosition();
        QString fs = dialog.fileSystem();

        // Если это разодел на расширенном разделе, то тип должен быть logical
        if (isExtendedPartition) {
            partType = "logical";
        }

        // Запрашиваем подтверждение
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, tr("Confirm Action"),
                                     tr("Are you sure you want to create a new %1 partition on %2?\n\n"
                                        "Start: %3\nEnd: %4\nFilesystem: %5")
                                      .arg(partType)
                                      .arg(devicePath)
                                      .arg(start)
                                      .arg(end)
                                      .arg(fs),
                                     QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            // Создаем раздел
            m_diskManager->createPartition(devicePath, partType, start, end, fs);
        }
    }
}

void MainWindow::updateButtonState()
{
    // Получаем выбранный элемент
    QTreeWidgetItem *item = ui->treeDevices->currentItem();

    // По умолчанию все кнопки недоступны
    ui->btnMarkFaulty->setEnabled(false);
    ui->btnAddToRaid->setEnabled(false);
    ui->btnRemoveFromRaid->setEnabled(false);

    if (!item) {
        return;
    }

    // Получаем путь к устройству
    QString devicePath = item->text(0);

    // Проверяем, является ли выбранное устройство разделом
    bool isPartition = devicePath.contains("sd") && devicePath.length() > 8;

    // Проверяем, является ли выбранное устройство RAID-массивом
    bool isRaid = devicePath.contains("md");

    // Проверяем, является ли выбранное устройство частью RAID
    bool isRaidMember = item->parent() && item->parent()->text(0).contains("md");

    // Кнопка "Пометить как неисправное" доступна только для устройств в RAID
    ui->btnMarkFaulty->setEnabled(isRaidMember);

    // Кнопка "Добавить в RAID" доступна только для разделов, не входящих в RAID
    ui->btnAddToRaid->setEnabled(isPartition && !isRaidMember);

    // Кнопка "Удалить из RAID" доступна только для устройств в RAID
    ui->btnRemoveFromRaid->setEnabled(isRaidMember);
}
