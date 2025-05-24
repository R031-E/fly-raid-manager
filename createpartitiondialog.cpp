#include "createpartitiondialog.h"
#include <QMessageBox>
#include <QApplication>
#include <QRegularExpression>
#include <QDebug>
#include <QtMath>

CreatePartitionDialog::CreatePartitionDialog(const QString &devicePath, QWidget *parent)
    : QDialog(parent), m_devicePath(devicePath), m_diskSizeMiB(0), m_updatingValues(false)
{
    setWindowTitle(tr("Create Partition"));
    setFixedSize(600, 650);  // Увеличили размер окна

    setupUi();

    // Получаем информацию о свободном пространстве
    // Это должно быть сделано через DiskManager, сигнал подключится в MainWindow
}

void CreatePartitionDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Информация об устройстве
    QLabel *deviceLabel = new QLabel(tr("Creating partition on: <b>%1</b>").arg(m_devicePath));
    deviceLabel->setFixedHeight(30);
    deviceLabel->setStyleSheet("QLabel { font-size: 12pt; padding: 5px; }");
    mainLayout->addWidget(deviceLabel);

    // Информация о диске и свободном пространстве
    QGroupBox *diskInfoGroup = new QGroupBox(tr("Disk Information"));
    diskInfoGroup->setFixedHeight(100);
    diskInfoGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 11pt; }");
    QVBoxLayout *diskInfoLayout = new QVBoxLayout(diskInfoGroup);

    m_diskInfoLabel = new QLabel(tr("Loading disk information..."));
    m_diskInfoLabel->setFixedHeight(25);
    m_diskInfoLabel->setStyleSheet("QLabel { font-size: 10pt; padding: 3px; }");
    diskInfoLayout->addWidget(m_diskInfoLabel);

    m_freeSpacesLabel = new QLabel(tr("Loading free space information..."));
    m_freeSpacesLabel->setFixedHeight(25);
    m_freeSpacesLabel->setWordWrap(true);
    m_freeSpacesLabel->setStyleSheet("QLabel { font-size: 10pt; padding: 3px; }");
    diskInfoLayout->addWidget(m_freeSpacesLabel);

    mainLayout->addWidget(diskInfoGroup);

    // Настройки раздела
    QGroupBox *partitionGroup = new QGroupBox(tr("Partition Settings"));
    partitionGroup->setFixedHeight(140);
    partitionGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 11pt; }");
    QFormLayout *partitionLayout = new QFormLayout(partitionGroup);
    partitionLayout->setLabelAlignment(Qt::AlignRight);
    partitionLayout->setVerticalSpacing(10);

    // Тип раздела
    m_partitionTypeCombo = new QComboBox();
    m_partitionTypeCombo->setFixedWidth(250);
    m_partitionTypeCombo->setFixedHeight(30);
    m_partitionTypeCombo->setStyleSheet("QComboBox { font-size: 10pt; padding: 3px; }");
    m_partitionTypeCombo->addItem(tr("Primary"), "primary");
    m_partitionTypeCombo->addItem(tr("Extended"), "extended");
    m_partitionTypeCombo->addItem(tr("Logical"), "logical");
    partitionLayout->addRow(tr("Partition Type:"), m_partitionTypeCombo);

    // Файловая система
    m_filesystemCombo = new QComboBox();
    m_filesystemCombo->setFixedWidth(250);
    m_filesystemCombo->setFixedHeight(30);
    m_filesystemCombo->setStyleSheet("QComboBox { font-size: 10pt; padding: 3px; }");
    setupFilesystemTypes();
    partitionLayout->addRow(tr("Filesystem:"), m_filesystemCombo);

    // Выбор свободной области
    m_freeSpaceAreaCombo = new QComboBox();
    m_freeSpaceAreaCombo->setFixedWidth(250);
    m_freeSpaceAreaCombo->setFixedHeight(30);
    m_freeSpaceAreaCombo->setStyleSheet("QComboBox { font-size: 10pt; padding: 3px; }");
    partitionLayout->addRow(tr("Free Space Area:"), m_freeSpaceAreaCombo);

    mainLayout->addWidget(partitionGroup);

    // Размер и позиция (целые числа в МиБ)
    QGroupBox *sizeGroup = new QGroupBox(tr("Size and Position (MiB, integers only)"));
    sizeGroup->setFixedHeight(160);
    sizeGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 11pt; }");
    QFormLayout *sizeLayout = new QFormLayout(sizeGroup);
    sizeLayout->setLabelAlignment(Qt::AlignRight);
    sizeLayout->setVerticalSpacing(10);

    // Начальная позиция в МиБ (QSpinBox)
    m_startPosSpinBox = new QSpinBox();
    m_startPosSpinBox->setFixedWidth(150);
    m_startPosSpinBox->setFixedHeight(30);
    m_startPosSpinBox->setStyleSheet("QSpinBox { font-size: 10pt; padding: 3px; }");
    m_startPosSpinBox->setRange(1, 999999999);
    m_startPosSpinBox->setValue(1);
    m_startPosSpinBox->setSuffix(tr(" MiB"));
    sizeLayout->addRow(tr("Start Position:"), m_startPosSpinBox);

    // Размер в МиБ (QSpinBox)
    m_sizeSpinBox = new QSpinBox();
    m_sizeSpinBox->setFixedWidth(150);
    m_sizeSpinBox->setFixedHeight(30);
    m_sizeSpinBox->setStyleSheet("QSpinBox { font-size: 10pt; padding: 3px; }");
    m_sizeSpinBox->setRange(1, 999999999);
    m_sizeSpinBox->setValue(100);
    m_sizeSpinBox->setSuffix(tr(" MiB"));
    sizeLayout->addRow(tr("Size:"), m_sizeSpinBox);

    // Конечная позиция в МиБ (QSpinBox - изменяемая)
    m_endPosSpinBox = new QSpinBox();
    m_endPosSpinBox->setFixedWidth(150);
    m_endPosSpinBox->setFixedHeight(30);
    m_endPosSpinBox->setStyleSheet("QSpinBox { font-size: 10pt; padding: 3px; }");
    m_endPosSpinBox->setRange(2, 999999999);
    m_endPosSpinBox->setValue(101);
    m_endPosSpinBox->setSuffix(tr(" MiB"));
    sizeLayout->addRow(tr("End Position:"), m_endPosSpinBox);

    mainLayout->addWidget(sizeGroup);

    // Валидация с прокруткой для длинных сообщений (изначально скрыта)
    m_validationGroup = new QGroupBox(tr("Validation"));
    m_validationGroup->setFixedHeight(120);
    m_validationGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 11pt; color: red; }");
    QVBoxLayout *validationLayout = new QVBoxLayout(m_validationGroup);

    m_validationScrollArea = new QScrollArea();
    m_validationScrollArea->setFixedHeight(90);
    m_validationScrollArea->setWidgetResizable(true);
    m_validationScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_validationScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_validationLabel = new QLabel();
    m_validationLabel->setWordWrap(true);
    m_validationLabel->setStyleSheet("QLabel { color: red; padding: 8px; font-size: 10pt; }");
    m_validationLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_validationScrollArea->setWidget(m_validationLabel);

    validationLayout->addWidget(m_validationScrollArea);
    mainLayout->addWidget(m_validationGroup);

    // Кнопки
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox->setFixedHeight(50);
    buttonBox->setStyleSheet("QPushButton { font-size: 11pt; padding: 5px 15px; }");
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);

    // Подключаем сигналы
    connect(m_startPosSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &CreatePartitionDialog::onStartPositionChanged);
    connect(m_sizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &CreatePartitionDialog::onSizeChanged);
    connect(m_endPosSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &CreatePartitionDialog::onEndPositionChanged);
    connect(m_freeSpaceAreaCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CreatePartitionDialog::onFreeSpaceAreaChanged);

    // Обновляем интерфейс
    updateEndPosition();
    updateValidation();
}

void CreatePartitionDialog::setupFilesystemTypes()
{
    m_filesystemCombo->addItem(tr("Unformatted"), "unformatted");
    m_filesystemCombo->addItem(tr("ext4"), "ext4");
    m_filesystemCombo->addItem(tr("ext3"), "ext3");
    m_filesystemCombo->addItem(tr("ext2"), "ext2");
    m_filesystemCombo->addItem(tr("xfs"), "xfs");
    m_filesystemCombo->addItem(tr("btrfs"), "btrfs");
    m_filesystemCombo->addItem(tr("ntfs"), "ntfs");
    m_filesystemCombo->addItem(tr("fat32"), "fat32");
    m_filesystemCombo->addItem(tr("fat16"), "fat16");
    m_filesystemCombo->addItem(tr("linux-swap"), "linux-swap");

    // По умолчанию выбираем ext4
    m_filesystemCombo->setCurrentIndex(1);
}

void CreatePartitionDialog::updateFreeSpaceCombo()
{
    m_freeSpaceAreaCombo->clear();
    m_validFreeSpaces.clear();

    // Фильтруем области >= 2 МиБ
    for (const FreeSpaceInfo &freeSpace : m_freeSpaces) {
        if (freeSpace.sizeMiB >= 2) {
            m_validFreeSpaces.append(freeSpace);

            QString areaText = tr("Area %1: %2-%3 MiB (Size: %4 MiB)")
                              .arg(m_validFreeSpaces.size())
                              .arg(freeSpace.startMiB)
                              .arg(freeSpace.endMiB)
                              .arg(freeSpace.sizeMiB);

            m_freeSpaceAreaCombo->addItem(areaText);
        }
    }

    if (m_validFreeSpaces.isEmpty()) {
        m_freeSpaceAreaCombo->addItem(tr("No suitable free space (>= 2 MiB) available"));
        m_freeSpaceAreaCombo->setEnabled(false);
    } else {
        m_freeSpaceAreaCombo->setEnabled(true);
        // Выбираем первую область по умолчанию
        m_freeSpaceAreaCombo->setCurrentIndex(0);
        updateConstraintsForSelectedFreeSpace();
    }
}

void CreatePartitionDialog::updateConstraintsForSelectedFreeSpace()
{
    FreeSpaceInfo *currentArea = getCurrentFreeSpace();
    if (!currentArea) {
        return;
    }

    m_updatingValues = true;

    // Обновляем ограничения для полей ввода
    m_startPosSpinBox->setRange(static_cast<int>(currentArea->startMiB),
                               static_cast<int>(currentArea->endMiB - 1));
    m_startPosSpinBox->setValue(static_cast<int>(currentArea->startMiB));

    m_sizeSpinBox->setRange(1, static_cast<int>(currentArea->sizeMiB));
    m_sizeSpinBox->setValue(qMin(100, static_cast<int>(currentArea->sizeMiB)));

    m_endPosSpinBox->setRange(static_cast<int>(currentArea->startMiB + 1),
                             static_cast<int>(currentArea->endMiB));
    m_endPosSpinBox->setValue(static_cast<int>(currentArea->startMiB + qMin(100, static_cast<int>(currentArea->sizeMiB))));

    m_updatingValues = false;

    qDebug() << "Updated constraints for area:" << currentArea->startMiB << "-" << currentArea->endMiB
             << "size:" << currentArea->sizeMiB;
}

FreeSpaceInfo* CreatePartitionDialog::getCurrentFreeSpace()
{
    int currentIndex = m_freeSpaceAreaCombo->currentIndex();
    if (currentIndex >= 0 && currentIndex < m_validFreeSpaces.size()) {
        return &m_validFreeSpaces[currentIndex];
    }
    return nullptr;
}

void CreatePartitionDialog::onFreeSpaceInfoOnDeviceReceived(const QString &info)
{
    parseFreeSpaceInfo(info);
}

void CreatePartitionDialog::onStartPositionChanged()
{
    if (m_updatingValues) return;
    updateEndPosition();
    updateValidation();
}

void CreatePartitionDialog::onSizeChanged()
{
    if (m_updatingValues) return;
    updateEndPosition();
    updateValidation();
}

void CreatePartitionDialog::onEndPositionChanged()
{
    if (m_updatingValues) return;
    updateSizeFromPositions();
    updateValidation();
}

void CreatePartitionDialog::onFreeSpaceAreaChanged()
{
    updateConstraintsForSelectedFreeSpace();
    updateEndPosition();
    updateValidation();
}

void CreatePartitionDialog::updateEndPosition()
{
    if (m_updatingValues) return;

    m_updatingValues = true;
    qint64 start = m_startPosSpinBox->value();
    qint64 size = m_sizeSpinBox->value();
    qint64 end = start + size;

    m_endPosSpinBox->setValue(static_cast<int>(end));
    m_updatingValues = false;
}

void CreatePartitionDialog::updateSizeFromPositions()
{
    if (m_updatingValues) return;

    m_updatingValues = true;
    qint64 start = m_startPosSpinBox->value();
    qint64 end = m_endPosSpinBox->value();
    qint64 size = end - start;

    if (size > 0) {
        m_sizeSpinBox->setValue(static_cast<int>(size));
    }
    m_updatingValues = false;
}

void CreatePartitionDialog::updateStartFromPositions()
{
    if (m_updatingValues) return;

    m_updatingValues = true;
    qint64 end = m_endPosSpinBox->value();
    qint64 size = m_sizeSpinBox->value();
    qint64 start = end - size;

    if (start > 0) {
        m_startPosSpinBox->setValue(static_cast<int>(start));
    }
    m_updatingValues = false;
}

void CreatePartitionDialog::updateValidation()
{
    QString error;

    if (m_validFreeSpaces.isEmpty()) {
        error = tr("No suitable free space (>= 2 MiB) available for partition creation.");
    } else {
        FreeSpaceInfo *currentArea = getCurrentFreeSpace();
        if (!currentArea) {
            error = tr("No free space area selected.");
        } else {
            qint64 start = m_startPosSpinBox->value();
            qint64 size = m_sizeSpinBox->value();
            qint64 end = m_endPosSpinBox->value();

            // Проверяем согласованность значений
            if (end != start + size) {
                error = tr("Inconsistent values: Start (%1) + Size (%2) ≠ End (%3). Expected end: %4 MiB.")
                       .arg(start).arg(size).arg(end).arg(start + size);
            }
            // Проверяем, помещается ли раздел в выбранную область
            else if (start < currentArea->startMiB) {
                error = tr("Start position (%1 MiB) is before the beginning of selected free space area (%2 MiB).")
                       .arg(start).arg(currentArea->startMiB);
            } else if (end > currentArea->endMiB) {
                error = tr("Partition end (%1 MiB) exceeds the end of selected free space area (%2 MiB).")
                       .arg(end).arg(currentArea->endMiB);
            } else if (size > currentArea->sizeMiB) {
                error = tr("Partition size (%1 MiB) exceeds maximum available space in selected area (%2 MiB).")
                       .arg(size).arg(currentArea->sizeMiB);
            } else if (end > m_diskSizeMiB) {
                error = tr("Partition end (%1 MiB) exceeds total disk size (%2 MiB).")
                       .arg(end).arg(m_diskSizeMiB);
            } else if (size <= 0) {
                error = tr("Partition size must be greater than 0 MiB.");
            }

            qDebug() << "Validating: start=" << start << ", size=" << size << ", end=" << end;
            qDebug() << "Current area:" << currentArea->startMiB << "-" << currentArea->endMiB
                     << "(size:" << currentArea->sizeMiB << "MiB)";
        }
    }

    m_validationLabel->setText(error);

    // Обновляем видимость области валидации
    updateValidationVisibility();

    // Автоматически подгоняем размер содержимого области прокрутки
    m_validationLabel->adjustSize();

    // Включаем/отключаем кнопку OK
    QDialogButtonBox *buttonBox = findChild<QDialogButtonBox*>();
    if (buttonBox) {
        QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
        if (okButton) {
            okButton->setEnabled(error.isEmpty());
        }
    }
}

void CreatePartitionDialog::updateValidationVisibility()
{
    bool hasError = !m_validationLabel->text().isEmpty();
    m_validationGroup->setVisible(hasError);

    // Обновляем размер окна в зависимости от видимости области валидации
    if (hasError) {
        setFixedSize(600, 770);  // Увеличиваем высоту когда показываем валидацию
    } else {
        setFixedSize(600, 650);  // Стандартная высота без валидации
    }
}

void CreatePartitionDialog::parseFreeSpaceInfo(const QString &output)
{
    m_freeSpaces.clear();
    m_diskSizeMiB = 0;

    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    // Отладочная информация
    qDebug() << "Parsing parted output:";
    for (int i = 0; i < lines.size(); ++i) {
        qDebug() << i << ":" << lines[i];
    }

    // Парсим информацию о диске
    for (const QString &line : lines) {
        if (line.startsWith("Disk /dev/") && line.contains(":")) {
            QStringList parts = line.split(':');
            if (parts.size() >= 2) {
                QString diskSizeStr = parts.last().trimmed();
                m_diskSizeMiB = parseMiBValue(diskSizeStr);
                m_diskInfoLabel->setText(tr("Disk size: %1 MiB (%2)")
                                        .arg(m_diskSizeMiB)
                                        .arg(diskSizeStr));
                qDebug() << "Found disk size:" << diskSizeStr << "=" << m_diskSizeMiB << "MiB";
            }
            break;
        }
    }

    // Если размер диска не найден, попробуем найти максимальное значение End
    if (m_diskSizeMiB == 0) {
        qint64 maxEnd = 0;
        for (const QString &line : lines) {
            if (line.contains("Free Space") || (line.contains("primary") || line.contains("logical") || line.contains("extended"))) {
                QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    qint64 endPos = parseMiBValue(parts[1]);
                    maxEnd = qMax(maxEnd, endPos);
                    qDebug() << "Found end position:" << parts[1] << "=" << endPos << "MiB";
                }
            }
        }
        if (maxEnd > 0) {
            m_diskSizeMiB = maxEnd;
            m_diskInfoLabel->setText(tr("Disk size: approximately %1 MiB").arg(m_diskSizeMiB));
            qDebug() << "Estimated disk size:" << m_diskSizeMiB << "MiB";
        }
    }

    // Парсим таблицу разделов для поиска свободных областей
    bool inTable = false;
    for (const QString &line : lines) {
        // Ищем заголовок таблицы
        if (line.contains("Number") && line.contains("Start") && line.contains("End")) {
            inTable = true;
            qDebug() << "Found table header, entering table mode";
            continue;
        }

        if (inTable && line.trimmed().isEmpty()) {
            qDebug() << "Empty line, exiting table mode";
            break;
        }

        // Парсим строки со свободным пространством
        if (inTable && line.contains("Free Space")) {
            qDebug() << "Parsing free space line:" << line;
            QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            qDebug() << "Parts:" << parts;

            // Ищем позицию "Free Space"
            bool hasFreeSpace = false;
            if (parts.size() >= 5) {
                if (parts[parts.size()-2] == "Free" && parts[parts.size()-1] == "Space") {
                    hasFreeSpace = true;
                } else if (parts.contains("Free") && parts.contains("Space")) {
                    hasFreeSpace = true;
                }
            }

            if (hasFreeSpace && parts.size() >= 3) {
                QString startStr = parts[0];
                QString endStr = parts[1];

                qint64 startMiB = parseMiBValue(startStr);
                qint64 endMiB = parseMiBValue(endStr);
                qint64 sizeMiB = endMiB - startMiB;

                qDebug() << "Free space:" << startStr << "(" << startMiB << "MiB) to"
                         << endStr << "(" << endMiB << "MiB)";
                qDebug() << "Calculated size:" << sizeMiB << "MiB";

                // Добавляем все свободные области (фильтрация будет в updateFreeSpaceCombo)
                if (sizeMiB >= 1) {
                    m_freeSpaces.append(FreeSpaceInfo(startMiB, endMiB, sizeMiB));
                    qDebug() << "Added free space:" << startMiB << "-" << endMiB << "MiB (size:" << sizeMiB << "MiB)";
                }
            } else {
                qDebug() << "Could not parse free space line:" << line;
            }
        }
    }

    qDebug() << "Found" << m_freeSpaces.size() << "total free spaces";

    // Обновляем информацию о свободных областях
    if (m_freeSpaces.isEmpty()) {
        m_freeSpacesLabel->setText(tr("No free space available"));
    } else {
        QStringList freeSpaceList;
        int suitableCount = 0;
        for (const FreeSpaceInfo &freeSpace : m_freeSpaces) {
            if (freeSpace.sizeMiB >= 2) {
                suitableCount++;
            }
            freeSpaceList.append(tr("%1-%2 MiB (%3 MiB)")
                                .arg(freeSpace.startMiB)
                                .arg(freeSpace.endMiB)
                                .arg(freeSpace.sizeMiB));
        }
        m_freeSpacesLabel->setText(tr("Found %1 areas, %2 suitable (>= 2 MiB)")
                                  .arg(m_freeSpaces.size())
                                  .arg(suitableCount));
    }

    // Обновляем выпадающий список областей
    updateFreeSpaceCombo();

    updateEndPosition();
    updateValidation();
}

qint64 CreatePartitionDialog::parseMiBValue(const QString &mibStr)
{
    // Парсим значения с округлением вверх для дробных чисел
    // Поддерживаем запятую как разделитель дробной части
    QRegularExpression rx("(\\d+(?:,\\d+)?)MiB");
    QRegularExpressionMatch match = rx.match(mibStr.trimmed());

    if (match.hasMatch()) {
        QString numberStr = match.captured(1);

        // Заменяем запятую на точку для корректного парсинга
        numberStr.replace(',', '.');

        double doubleValue = numberStr.toDouble();
        qint64 result = static_cast<qint64>(qCeil(doubleValue));  // Округляем вверх

        qDebug() << "parseMiBValue:" << mibStr << "=> numberStr:" << numberStr
                 << "=> doubleValue:" << doubleValue << "=> result:" << result << "MiB";
        return result;
    }

    qDebug() << "parseMiBValue: could not parse" << mibStr;
    return 0;
}

QString CreatePartitionDialog::getPartitionType() const
{
    return m_partitionTypeCombo->currentData().toString();
}

QString CreatePartitionDialog::getFilesystemType() const
{
    return m_filesystemCombo->currentData().toString();
}

QString CreatePartitionDialog::getStartSize() const
{
    return QString::number(m_startPosSpinBox->value()) + "MiB";
}

QString CreatePartitionDialog::getEndSize() const
{
    return QString::number(m_endPosSpinBox->value()) + "MiB";
}

qint64 CreatePartitionDialog::getStartValue() const
{
    return m_startPosSpinBox->value();
}

qint64 CreatePartitionDialog::getEndValue() const
{
    return m_endPosSpinBox->value();
}

qint64 CreatePartitionDialog::getSizeValue() const
{
    return m_sizeSpinBox->value();
}

bool CreatePartitionDialog::validateInput()
{
    if (m_validFreeSpaces.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Input"),
                           tr("No suitable free space (>= 2 MiB) available for partition creation."));
        return false;
    }

    FreeSpaceInfo *currentArea = getCurrentFreeSpace();
    if (!currentArea) {
        QMessageBox::warning(this, tr("Invalid Input"),
                           tr("No free space area selected."));
        return false;
    }

    qint64 start = m_startPosSpinBox->value();
    qint64 size = m_sizeSpinBox->value();
    qint64 end = m_endPosSpinBox->value();

    // Проверяем согласованность значений
    if (end != start + size) {
        QMessageBox::warning(this, tr("Invalid Input"),
                           tr("Inconsistent values: Start (%1) + Size (%2) ≠ End (%3). Expected end: %4 MiB.")
                           .arg(start).arg(size).arg(end).arg(start + size));
        return false;
    }

    // Проверяем, помещается ли раздел в выбранную область
    if (start < currentArea->startMiB || end > currentArea->endMiB) {
        QMessageBox::warning(this, tr("Invalid Input"),
                           tr("Partition (start: %1 MiB, end: %2 MiB) does not fit in selected free space area (%3-%4 MiB).")
                           .arg(start).arg(end)
                           .arg(currentArea->startMiB).arg(currentArea->endMiB));
        return false;
    }

    if (end > m_diskSizeMiB) {
        QMessageBox::warning(this, tr("Invalid Input"),
                           tr("Partition end (%1 MiB) exceeds disk size (%2 MiB).")
                           .arg(end).arg(m_diskSizeMiB));
        return false;
    }

    if (size <= 0) {
        QMessageBox::warning(this, tr("Invalid Input"),
                           tr("Partition size must be greater than 0 MiB."));
        return false;
    }

    return true;
}

void CreatePartitionDialog::accept()
{
    if (validateInput()) {
        QDialog::accept();
    }
}
