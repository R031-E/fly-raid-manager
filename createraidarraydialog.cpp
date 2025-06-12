#include "createraidarraydialog.h"
#include "ui_createraidarraydialog.h"
#include <QMessageBox>
#include <QIcon>
#include <QDebug>

CreateRaidArrayDialog::CreateRaidArrayDialog(const DiskStructure &diskStructure, QWidget *parent)
    : QDialog(parent), ui(new Ui::CreateRaidArrayDialog), m_operationInProgress(false), m_currentStep(0), m_totalSteps(0)
{
    ui->setupUi(this);
    setupDialog();
    populateDeviceTree(diskStructure);
    setupRaidLevelCombo();
    setupConnections();

    // Устанавливаем иконку диалога
    ui->iconLabel->setPixmap(QIcon::fromTheme("drive-harddisk-raid").pixmap(48, 48));

    // Первоначальная валидация
    onRaidLevelChanged();
    updateSelectedDevicesInfo();
}

CreateRaidArrayDialog::~CreateRaidArrayDialog()
{
    delete ui;
}

void CreateRaidArrayDialog::setupDialog()
{
    // Настройка диалога
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setModal(true);

    // Скрываем область прогресса изначально
    showProgress(false);

    // Настройка дерева устройств
    ui->deviceTree->setRootIsDecorated(true);
    ui->deviceTree->setAlternatingRowColors(true);
    ui->deviceTree->header()->setStretchLastSection(true);
    ui->deviceTree->expandAll();

    // Настройка полей
    ui->arrayNameEdit->setMaxLength(50);
}

void CreateRaidArrayDialog::populateDeviceTree(const DiskStructure &diskStructure)
{
    m_availableDevices.clear();
    ui->deviceTree->clear();

    // Создаем элементы дисков и их разделы
    for (const DiskInfo &disk : diskStructure.getDisks()) {
        // Создаем элемент диска (только для группировки)
        QTreeWidgetItem *diskItem = new QTreeWidgetItem(ui->deviceTree);
        diskItem->setText(0, disk.devicePath);

        // Форматируем размер диска через DiskUtils
        qint64 diskSizeBytes = DiskUtils::parseSizeString(disk.size);
        QString formattedDiskSize = DiskUtils::formatByteSize(diskSizeBytes);
        diskItem->setText(1, formattedDiskSize);

        diskItem->setText(2, tr("Disk"));
        diskItem->setText(3, "-");
        diskItem->setText(4, tr("Not selectable"));
        diskItem->setIcon(0, QIcon::fromTheme("drive-harddisk"));

        // Диск не выбираемый, но остается включенным для отображения дочерних элементов
        diskItem->setFlags(Qt::ItemIsEnabled);
        diskItem->setForeground(4, QBrush(QColor("#666666")));

        bool hasSelectablePartitions = false;

        // Добавляем разделы как дочерние элементы
        for (const PartitionInfo &partition : disk.partitions) {
            QTreeWidgetItem *partitionItem = new QTreeWidgetItem(diskItem);
            partitionItem->setText(0, partition.devicePath);

            // Форматируем размер раздела через DiskUtils
            qint64 partitionSizeBytes = DiskUtils::parseSizeString(partition.size);
            QString formattedPartitionSize = DiskUtils::formatByteSize(partitionSizeBytes);
            partitionItem->setText(1, formattedPartitionSize);

            partitionItem->setText(2, tr("Partition"));
            partitionItem->setText(3, partition.filesystem.isEmpty() ? tr("No filesystem") : partition.filesystem);
            partitionItem->setIcon(0, QIcon::fromTheme("drive-harddisk-partition"));

            bool isMounted = !partition.mountPoint.isEmpty();
            bool isInRaid = partition.isRaidMember;
            // ИЗМЕНЕНИЕ: Добавляем проверку на отсутствие файловой системы
            bool hasFilesystem = !partition.filesystem.isEmpty() &&
                               partition.filesystem != "unknown" &&
                               partition.filesystem != tr("No filesystem");

            // Для RAID доступны только немонтированные партиции без файловой системы, не входящие в другие RAID
            //bool isSelectable = !isMounted && !isInRaid && !hasFilesystem;
            bool isSelectable = !isMounted && !isInRaid;
            QString status;
            if (isMounted) {
                status = tr("Mounted");
                partitionItem->setForeground(4, QBrush(QColor("#e74c3c"))); // Красный
            } else if (isInRaid) {
                status = tr("In RAID");
                partitionItem->setForeground(4, QBrush(QColor("#e74c3c"))); // Красный
            } else if (hasFilesystem) {
                status = tr("Has filesystem");
                partitionItem->setForeground(4, QBrush(QColor("#f39c12"))); // Оранжевый
                hasSelectablePartitions = true;
            } else {
                status = tr("Available");
                partitionItem->setForeground(4, QBrush(QColor("#27ae60"))); // Зеленый
                hasSelectablePartitions = true;
            }

            partitionItem->setText(4, status);

            // Устанавливаем флаги для доступных разделов
            if (isSelectable) {
                partitionItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
                partitionItem->setCheckState(0, Qt::Unchecked);
            } else {
                partitionItem->setFlags(Qt::ItemIsEnabled);
            }

            // Добавляем в список кандидатов
            RaidDeviceCandidate candidate(partition.devicePath, formattedPartitionSize,
                                        "partition", partition.filesystem);
            candidate.isMounted = isMounted;
            candidate.isInRaid = isInRaid;
            m_availableDevices.append(candidate);
        }

        // Скрываем диски без доступных партиций
        if (!hasSelectablePartitions && disk.partitions.size() > 0) {
            diskItem->setHidden(true);
        }
    }

    // Добавляем неразмеченные диски (без разделов) как отдельные элементы
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

            QTreeWidgetItem *wholeDiskItem = new QTreeWidgetItem(ui->deviceTree);
            wholeDiskItem->setText(0, disk.devicePath);

            // Форматируем размер диска через DiskUtils
            qint64 wholeDiskSizeBytes = DiskUtils::parseSizeString(disk.size);
            QString formattedWholeDiskSize = DiskUtils::formatByteSize(wholeDiskSizeBytes);
            wholeDiskItem->setText(1, formattedWholeDiskSize);

            wholeDiskItem->setText(2, tr("Whole Disk"));
            wholeDiskItem->setText(3, tr("Unpartitioned"));
            wholeDiskItem->setIcon(0, QIcon::fromTheme("drive-harddisk"));

            bool isSelectable = !isInRaid;
            QString status = isInRaid ? tr("In RAID") : tr("Available");
            wholeDiskItem->setText(4, status);

            if (isSelectable) {
                wholeDiskItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
                wholeDiskItem->setCheckState(0, Qt::Unchecked);
                wholeDiskItem->setForeground(4, QBrush(QColor("#27ae60"))); // Зеленый
            } else {
                wholeDiskItem->setFlags(Qt::ItemIsEnabled);
                wholeDiskItem->setForeground(4, QBrush(QColor("#e74c3c"))); // Красный
            }

            RaidDeviceCandidate candidate(disk.devicePath, formattedWholeDiskSize, "disk", "");
            candidate.isMounted = false;
            candidate.isInRaid = isInRaid;
            m_availableDevices.append(candidate);
        }
    }

    ui->deviceTree->expandAll();
}

void CreateRaidArrayDialog::setupRaidLevelCombo()
{
    // Очищаем и заполняем комбобокс с типами RAID
    ui->raidLevelCombo->clear();
    ui->raidLevelCombo->addItem(tr("RAID 0 (Stripe) - No redundancy, high performance"),
                               static_cast<int>(RaidType::RAID0));
    ui->raidLevelCombo->addItem(tr("RAID 1 (Mirror) - Full redundancy, good performance"),
                               static_cast<int>(RaidType::RAID1));
    ui->raidLevelCombo->addItem(tr("RAID 5 (Stripe with parity) - Single drive fault tolerance"),
                               static_cast<int>(RaidType::RAID5));

    // По умолчанию выбираем RAID 1
    ui->raidLevelCombo->setCurrentIndex(1);
}

void CreateRaidArrayDialog::setupConnections()
{
    // Подключаем сигналы элементов управления
    connect(ui->raidLevelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CreateRaidArrayDialog::onRaidLevelChanged);

    connect(ui->deviceTree, &QTreeWidget::itemChanged,
            this, &CreateRaidArrayDialog::onDeviceSelectionChanged);

    connect(ui->arrayNameEdit, &QLineEdit::textChanged,
            this, &CreateRaidArrayDialog::onArrayNameChanged);

    // Переопределяем стандартные кнопки
    QPushButton *createButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    createButton->setText(tr("Create RAID"));
    createButton->setIcon(QIcon::fromTheme("list-add"));

    QPushButton *cancelButton = ui->buttonBox->button(QDialogButtonBox::Cancel);
    cancelButton->setText(tr("Cancel"));
    cancelButton->setIcon(QIcon::fromTheme("dialog-cancel"));
}

void CreateRaidArrayDialog::onRaidLevelChanged()
{
    RaidType raidType = getSelectedRaidType();
    int minDevices = getMinimumDevicesForRaidLevel(raidType);

    // Обновляем информацию о требованиях
    QString info = tr("Selected devices: %1 (minimum required: %2)")
                   .arg(getSelectedDeviceCount())
                   .arg(minDevices);

    ui->selectedDevicesLabel->setText(info);

    // Обновляем доступность кнопки создания
    QPushButton *createButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    QString errorMessage;
    createButton->setEnabled(validateSelection(errorMessage));

    // Показываем описание выбранного типа RAID
    QString description;
    switch (raidType) {
    case RaidType::RAID0:
        description = tr("RAID 0 provides high performance by striping data across drives, "
                        "but offers no redundancy. If any drive fails, all data is lost.");
        break;
    case RaidType::RAID1:
        description = tr("RAID 1 mirrors data across drives, providing full redundancy. "
                        "Can survive failure of any single drive.");
        break;
    case RaidType::RAID5:
        description = tr("RAID 5 stripes data with distributed parity, providing good performance "
                        "and fault tolerance. Can survive failure of any single drive.");
        break;
    default:
        description = tr("Unknown RAID type.");
        break;
    }

    ui->deviceSelectionInfo->setText(tr("Select devices/partitions to include in the RAID array:\n%1")
                                    .arg(description));
}

void CreateRaidArrayDialog::onDeviceSelectionChanged()
{
    updateDeviceSelection();
    updateSelectedDevicesInfo();

    // Обновляем доступность кнопки создания
    QPushButton *createButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    QString errorMessage;
    createButton->setEnabled(validateSelection(errorMessage));
}

void CreateRaidArrayDialog::updateSelectedDevicesInfo()
{
    RaidType raidType = getSelectedRaidType();
    int minDevices = getMinimumDevicesForRaidLevel(raidType);
    int selectedDevices = getSelectedDeviceCount();

    QString info = tr("Selected devices: %1 (minimum required: %2)")
                   .arg(selectedDevices)
                   .arg(minDevices);

    if (selectedDevices >= minDevices) {
        ui->selectedDevicesLabel->setStyleSheet("QLabel { color: #27ae60; }");
    } else {
        ui->selectedDevicesLabel->setStyleSheet("QLabel { color: #e74c3c; }");
    }

    ui->selectedDevicesLabel->setText(info);
}

void CreateRaidArrayDialog::onArrayNameChanged()
{
    // Валидируем имя массива
    QString arrayName = ui->arrayNameEdit->text();

    // Проверяем на недопустимые символы
    QRegularExpression validName("^[a-zA-Z0-9_-]*$");
    if (!arrayName.isEmpty() && !validName.match(arrayName).hasMatch()) {
        ui->arrayNameEdit->setStyleSheet("QLineEdit { border: 2px solid #e74c3c; }");
    } else {
        ui->arrayNameEdit->setStyleSheet("");
    }
}

void CreateRaidArrayDialog::updateDeviceSelection()
{
    m_selectedDevices.clear();

    // Рекурсивная функция для обхода всех элементов дерева
    std::function<void(QTreeWidgetItem*)> processItem = [&](QTreeWidgetItem *item) {
        // Проверяем, есть ли у элемента чекбокс и отмечен ли он
        if ((item->flags() & Qt::ItemIsUserCheckable) &&
            item->checkState(0) == Qt::Checked) {
            m_selectedDevices.append(item->text(0));
        }

        // Обрабатываем дочерние элементы
        for (int i = 0; i < item->childCount(); ++i) {
            processItem(item->child(i));
        }
    };

    // Проходим по всем элементам верхнего уровня
    for (int i = 0; i < ui->deviceTree->topLevelItemCount(); ++i) {
        processItem(ui->deviceTree->topLevelItem(i));
    }

    // Обновляем состояние в списке кандидатов
    for (RaidDeviceCandidate &device : m_availableDevices) {
        device.isSelected = m_selectedDevices.contains(device.devicePath);
    }
}

bool CreateRaidArrayDialog::validateSelection(QString &errorMessage) const
{
    RaidType raidType = getSelectedRaidType();
    int minDevices = getMinimumDevicesForRaidLevel(raidType);
    int selectedDevices = getSelectedDeviceCount();

    if (selectedDevices < minDevices) {
        errorMessage = tr("Not enough devices selected. RAID %1 requires at least %2 devices, but only %3 selected.")
                       .arg(static_cast<int>(raidType))
                       .arg(minDevices)
                       .arg(selectedDevices);
        return false;
    }

    // Проверяем имя массива
    QString arrayName = ui->arrayNameEdit->text();
    if (!arrayName.isEmpty()) {
        QRegularExpression validName("^[a-zA-Z0-9_-]*$");
        if (!validName.match(arrayName).hasMatch()) {
            errorMessage = tr("Array name contains invalid characters. Only letters, numbers, underscore and dash are allowed.");
            return false;
        }
    }

    // Проверяем, что выбранные устройства доступны
    for (const QString &devicePath : m_selectedDevices) {
        bool found = false;
        for (const RaidDeviceCandidate &device : m_availableDevices) {
            if (device.devicePath == devicePath) {
                if (device.isMounted || device.isInRaid) {
                    errorMessage = tr("Device %1 is not available (mounted or already in RAID).")
                                   .arg(devicePath);
                    return false;
                }
                found = true;
                break;
            }
        }
        if (!found) {
            errorMessage = tr("Device %1 is not in the available devices list.").arg(devicePath);
            return false;
        }
    }

    return true;
}

int CreateRaidArrayDialog::getMinimumDevicesForRaidLevel(RaidType raidType) const
{
    switch (raidType) {
    case RaidType::RAID0:
        return 2;
    case RaidType::RAID1:
        return 2;
    case RaidType::RAID5:
        return 3;
    default:
        return 2;
    }
}

int CreateRaidArrayDialog::getSelectedDeviceCount() const
{
    return m_selectedDevices.size();
}

RaidType CreateRaidArrayDialog::getSelectedRaidType() const
{
    int index = ui->raidLevelCombo->currentIndex();
    return getRaidTypeFromIndex(index);
}

QString CreateRaidArrayDialog::getArrayName() const
{
    return ui->arrayNameEdit->text().trimmed();
}

QStringList CreateRaidArrayDialog::getSelectedDevices() const
{
    return m_selectedDevices;
}

RaidType CreateRaidArrayDialog::getRaidTypeFromIndex(int index) const
{
    switch (index) {
    case 0: return RaidType::RAID0;
    case 1: return RaidType::RAID1;
    case 2: return RaidType::RAID5;
    default: return RaidType::RAID1;
    }
}

QString CreateRaidArrayDialog::raidTypeToString(RaidType type) const
{
    switch (type) {
    case RaidType::RAID0: return "raid0";
    case RaidType::RAID1: return "raid1";
    case RaidType::RAID5: return "raid5";
    default: return "raid1";
    }
}

QString CreateRaidArrayDialog::formatDeviceInfo(const RaidDeviceCandidate &device) const
{
    return tr("%1 (%2, %3)").arg(device.devicePath).arg(device.size).arg(device.type);
}

void CreateRaidArrayDialog::accept()
{
    // Валидируем выбор
    QString errorMessage;
    if (!validateSelection(errorMessage)) {
        QMessageBox::warning(this, tr("Invalid Selection"), errorMessage);
        return;
    }

    // Запрашиваем подтверждение
    QMessageBox::StandardButton reply;
    QString confirmMessage = tr("Create RAID %1 array using the following devices?\n\n")
                            .arg(static_cast<int>(getSelectedRaidType()));

    for (const QString &device : m_selectedDevices) {
        confirmMessage += tr("• %1\n").arg(device);
    }

    confirmMessage += tr("\nWarning: All data on these devices will be permanently lost!");

    reply = QMessageBox::question(this, tr("Confirm RAID Creation"), confirmMessage,
                                 QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    // Начинаем процесс создания RAID
    startRaidCreation();
}

void CreateRaidArrayDialog::reject()
{
    if (m_operationInProgress) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, tr("Cancel Operation"),
                                     tr("RAID creation is in progress. Are you sure you want to cancel?\n\n"
                                        "Note: This may leave devices in an inconsistent state."),
                                     QMessageBox::Yes | QMessageBox::No);

        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    QDialog::reject();
}

void CreateRaidArrayDialog::startRaidCreation()
{
    setOperationInProgress(true);
    showProgress(true);

    // Рассчитываем общее количество шагов
    m_totalSteps = m_selectedDevices.size() + 1; // wipefs для каждого устройства + создание RAID
    m_currentStep = 0;

    updateProgress(0, tr("Preparing devices..."));

    // Отправляем сигнал для начала создания RAID
    emit createRaidRequested(getSelectedRaidType(), getSelectedDevices(), getArrayName());
}

void CreateRaidArrayDialog::showProgress(bool show)
{
    ui->progressGroup->setVisible(show);

    if (show) {
        // Переводим диалог в режим прогресса
        ui->raidSettingsGroup->setEnabled(false);
        ui->deviceSelectionGroup->setEnabled(false);
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));

        // Изменяем размер диалога
        setFixedSize(700, 700);
    } else {
        // Возвращаем диалог в обычный режим
        ui->raidSettingsGroup->setEnabled(true);
        ui->deviceSelectionGroup->setEnabled(true);
        ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));

        // Изменяем размер диалога обратно
        setFixedSize(700, 600);
    }
}

void CreateRaidArrayDialog::updateProgress(int step, const QString &operation)
{
    m_currentStep = step;

    // Обновляем прогресс-бар
    if (m_totalSteps > 0) {
        int progressPercent = (m_currentStep * 100) / m_totalSteps;
        ui->operationProgressBar->setValue(progressPercent);
    }

    // Обновляем текущую операцию
    ui->currentOperationLabel->setText(operation);

    // Логируем операцию
    logOperation(tr("[Step %1/%2] %3").arg(m_currentStep).arg(m_totalSteps).arg(operation));
}

void CreateRaidArrayDialog::logOperation(const QString &message)
{
    // Добавляем сообщение в лог
    ui->operationLogText->append(message);

    // Прокручиваем вниз
    QTextCursor cursor = ui->operationLogText->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->operationLogText->setTextCursor(cursor);

    // Принудительно обновляем интерфейс
    qApp->processEvents();
}

void CreateRaidArrayDialog::setOperationInProgress(bool inProgress)
{
    m_operationInProgress = inProgress;

    if (!inProgress) {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
        ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Close"));
    }
}

// Слоты для обработки сигналов от DiskManager

void CreateRaidArrayDialog::onWipeProgressUpdate(const QString &device, bool success)
{
    m_currentStep++;

    if (success) {
        updateProgress(m_currentStep, tr("Device %1 prepared successfully").arg(device));
        logOperation(tr("✓ wipefs completed successfully for %1").arg(device));
    } else {
        logOperation(tr("✗ wipefs failed for %1").arg(device));
        updateProgress(m_currentStep, tr("Failed to prepare device %1").arg(device));
    }
}

void CreateRaidArrayDialog::onRaidCreationProgress(const QString &message)
{
    logOperation(message);

    if (message.contains("mdadm", Qt::CaseInsensitive)) {
        updateProgress(m_totalSteps, tr("Creating RAID array..."));
    }
}

void CreateRaidArrayDialog::onRaidCreationCompleted(bool success, const QString &raidDevice)
{
    setOperationInProgress(false);

    if (success) {
        updateProgress(m_totalSteps, tr("RAID array created successfully"));
        logOperation(tr("✓ RAID array %1 created successfully!").arg(raidDevice));

        // Показываем сообщение об успехе
        QMessageBox::information(this, tr("RAID Created"),
                                tr("RAID array %1 has been created successfully!\n\n"
                                   "The array is now available for partitioning and formatting.")
                                   .arg(raidDevice));

        // Закрываем диалог с успехом
        QDialog::accept();
    } else {
        updateProgress(m_currentStep, tr("RAID creation failed"));
        logOperation(tr("✗ Failed to create RAID array"));

        // Показываем сообщение об ошибке
        QMessageBox::critical(this, tr("RAID Creation Failed"),
                             tr("Failed to create RAID array. Please check the operation log "
                                "for details and try again."));

        // Возвращаем диалог в рабочее состояние
        showProgress(false);
        ui->raidSettingsGroup->setEnabled(true);
        ui->deviceSelectionGroup->setEnabled(true);

        QString errorMessage;
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(validateSelection(errorMessage));
    }
}

