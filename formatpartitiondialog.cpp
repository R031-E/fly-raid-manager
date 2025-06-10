#include "formatpartitiondialog.h"
#include <QMessageBox>
#include <QIcon>

FormatPartitionDialog::FormatPartitionDialog(const QString &partitionPath, QWidget *parent)
    : QDialog(parent), m_partitionPath(partitionPath)
{
    setWindowTitle(tr("Format Partition"));
    setWindowIcon(QIcon::fromTheme("edit-clear"));
    setFixedSize(500, 600);

    setupUi();
}

void FormatPartitionDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Заголовок с иконкой
    QHBoxLayout *headerLayout = new QHBoxLayout();

    QLabel *iconLabel = new QLabel();
    iconLabel->setPixmap(QIcon::fromTheme("edit-clear").pixmap(48, 48));
    iconLabel->setAlignment(Qt::AlignTop);
    headerLayout->addWidget(iconLabel);

    QLabel *titleLabel = new QLabel(tr("<b>Format Partition</b>"));
    titleLabel->setStyleSheet("QLabel { font-size: 14pt; color: #2980b9; }");
    titleLabel->setAlignment(Qt::AlignTop);
    headerLayout->addWidget(titleLabel);

    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // Информация о разделе
    QLabel *partitionLabel = new QLabel(tr("Partition: <b>%1</b>").arg(m_partitionPath));
    partitionLabel->setStyleSheet("QLabel { font-size: 11pt; padding: 5px; }");
    mainLayout->addWidget(partitionLabel);

    // Предупреждение
    m_warningLabel = new QLabel(tr("<b>Warning:</b> Formatting will permanently erase all data on this partition."));
    m_warningLabel->setWordWrap(true);
    m_warningLabel->setStyleSheet("QLabel { color: #e74c3c; background-color: #fdeaea; "
                                 "border: 1px solid #f5b7b1; border-radius: 5px; "
                                 "padding: 10px; font-size: 10pt; }");
    mainLayout->addWidget(m_warningLabel);

    // Параметры форматирования
    QGroupBox *optionsGroup = new QGroupBox(tr("Format Options"));
    optionsGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 11pt; }");
    QFormLayout *optionsLayout = new QFormLayout(optionsGroup);
    optionsLayout->setVerticalSpacing(10);

    // Выбор файловой системы
    m_filesystemCombo = new QComboBox();
    m_filesystemCombo->setStyleSheet("QComboBox { font-size: 10pt; padding: 5px; }");
    setupFilesystemTypes();
    optionsLayout->addRow(tr("Filesystem:"), m_filesystemCombo);

    /* Метка тома
    m_volumeLabelEdit = new QLineEdit();
    m_volumeLabelEdit->setStyleSheet("QLineEdit { font-size: 10pt; padding: 5px; }");
    m_volumeLabelEdit->setPlaceholderText(tr("Optional volume label"));
    m_volumeLabelEdit->setMaxLength(32); // Ограничение для совместимости
    optionsLayout->addRow(tr("Volume Label:"), m_volumeLabelEdit);*/

    mainLayout->addWidget(optionsGroup);

    // Описание выбранной файловой системы
    QGroupBox *descriptionGroup = new QGroupBox(tr("Filesystem Description"));
    descriptionGroup->setStyleSheet("QGroupBox { font-weight: bold; font-size: 11pt; }");
    QVBoxLayout *descriptionLayout = new QVBoxLayout(descriptionGroup);

    m_descriptionText = new QTextEdit();
    m_descriptionText->setReadOnly(true);
    m_descriptionText->setMaximumHeight(80);
    m_descriptionText->setStyleSheet("QTextEdit { font-size: 9pt; background-color: #f8f9fa; "
                                    "border: 1px solid #dee2e6; }");
    descriptionLayout->addWidget(m_descriptionText);

    mainLayout->addWidget(descriptionGroup);

    // Чекбокс подтверждения
    m_confirmCheckBox = new QCheckBox(tr("I understand that all data on this partition will be permanently lost"));
    m_confirmCheckBox->setStyleSheet("QCheckBox { font-size: 10pt; font-weight: bold; color: #e74c3c; }");
    mainLayout->addWidget(m_confirmCheckBox);

    // Кнопки
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox->setStyleSheet("QPushButton { font-size: 11pt; padding: 8px 20px; }");

    m_formatButton = buttonBox->button(QDialogButtonBox::Ok);
    m_formatButton->setText(tr("Format"));
    m_formatButton->setIcon(QIcon::fromTheme("edit-clear"));
    m_formatButton->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-weight: bold; }");
    m_formatButton->setEnabled(false);

    QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel);
    cancelButton->setText(tr("Cancel"));
    cancelButton->setIcon(QIcon::fromTheme("dialog-cancel"));

    // Подключаем сигналы
    connect(m_filesystemCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatPartitionDialog::onFilesystemChanged);
    connect(m_confirmCheckBox, &QCheckBox::toggled,
            this, &FormatPartitionDialog::onConfirmationChanged);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);

    // Инициализируем описание
    updateDescription();
}

void FormatPartitionDialog::setupFilesystemTypes()
{
    // Популярные файловые системы
    m_filesystemCombo->addItem(tr("ext4 (Recommended for Linux)"), "ext4");
    m_filesystemCombo->addItem(tr("ext3 (Linux, older)"), "ext3");
    m_filesystemCombo->addItem(tr("ext2 (Linux, legacy)"), "ext2");
    m_filesystemCombo->addItem(tr("XFS (High performance)"), "xfs");
    m_filesystemCombo->addItem(tr("Btrfs (Modern, experimental)"), "btrfs");
    m_filesystemCombo->addItem(tr("FAT32 (Universal compatibility)"), "fat32");
    m_filesystemCombo->addItem(tr("FAT16 (Old systems)"), "fat16");
    m_filesystemCombo->addItem(tr("NTFS (Windows)"), "ntfs");
    m_filesystemCombo->addItem(tr("Linux Swap"), "linux-swap");

    // По умолчанию выбираем ext4
    m_filesystemCombo->setCurrentIndex(0);
}

void FormatPartitionDialog::onFilesystemChanged()
{
    updateDescription();
    updateFormatButton();
}

void FormatPartitionDialog::onConfirmationChanged()
{
    updateFormatButton();
}

void FormatPartitionDialog::updateFormatButton()
{
    m_formatButton->setEnabled(m_confirmCheckBox->isChecked());
}

void FormatPartitionDialog::updateDescription()
{
    QString filesystem = getSelectedFilesystem();
    QString description = getFilesystemDescription(filesystem);
    m_descriptionText->setHtml(description);
}

QString FormatPartitionDialog::getFilesystemDescription(const QString &filesystem) const
{
    if (filesystem == "ext4") {
        return tr("<b>ext4</b> - Fourth extended filesystem. The default and recommended "
                 "filesystem for most Linux distributions. Supports journaling, large files, "
                 "and is very stable and mature.");
    } else if (filesystem == "ext3") {
        return tr("<b>ext3</b> - Third extended filesystem. Older version of ext4 with "
                 "journaling support. Good compatibility but fewer features than ext4.");
    } else if (filesystem == "ext2") {
        return tr("<b>ext2</b> - Second extended filesystem. Legacy filesystem without "
                 "journaling. Fast but less safe in case of power failures.");
    } else if (filesystem == "xfs") {
        return tr("<b>XFS</b> - High-performance 64-bit journaling filesystem. "
                 "Excellent for large files and high I/O workloads. Used by default "
                 "in Red Hat Enterprise Linux.");
    } else if (filesystem == "btrfs") {
        return tr("<b>Btrfs</b> - Modern copy-on-write filesystem with advanced features "
                 "like snapshots, checksums, and built-in RAID. Still considered "
                 "experimental for some use cases.");
    } else if (filesystem == "fat32") {
        return tr("<b>FAT32</b> - File Allocation Table 32-bit. Universal compatibility "
                 "with all operating systems. Good for USB drives and small partitions. "
                 "4GB maximum file size limit.");
    } else if (filesystem == "fat16") {
        return tr("<b>FAT16</b> - File Allocation Table 16-bit. Legacy filesystem for "
                 "very old systems. 2GB maximum partition size limit.");
    } else if (filesystem == "ntfs") {
        return tr("<b>NTFS</b> - New Technology File System. Native Windows filesystem "
                 "with support for large files and advanced permissions. Good Linux "
                 "support through ntfs-3g driver.");
    } else if (filesystem == "linux-swap") {
        return tr("<b>Linux Swap</b> - Virtual memory swap space for Linux systems. "
                 "Used when physical RAM is full. Size usually 1-2 times the amount of RAM.");
    }

    return tr("Unknown filesystem type.");
}

QString FormatPartitionDialog::getSelectedFilesystem() const
{
    return m_filesystemCombo->currentData().toString();
}

QString FormatPartitionDialog::getVolumeLabel() const
{
    return "";
}

bool FormatPartitionDialog::isConfirmed() const
{
    return m_confirmCheckBox->isChecked();
}
