#include "partitiontabledialog.h"
#include <QMessageBox>

PartitionTableDialog::PartitionTableDialog(const QString &devicePath, QWidget *parent)
    : QDialog(parent), m_devicePath(devicePath)
{
    setWindowTitle(tr("Create Partition Table"));
    setMinimumWidth(400);

    setupUi();
}

void PartitionTableDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Предупреждающее сообщение
    QLabel *warningLabel = new QLabel(tr("<b>Warning:</b> Creating a new partition table will destroy all existing data on the disk. "
                                       "This cannot be undone. Make sure you have backed up all important data."));
    warningLabel->setWordWrap(true);
    mainLayout->addWidget(warningLabel);

    // Информация об устройстве
    QLabel *deviceLabel = new QLabel(tr("Device: %1").arg(m_devicePath));
    mainLayout->addWidget(deviceLabel);

    // Выбор типа таблицы разделов
    QLabel *tableTypeLabel = new QLabel(tr("Partition table type:"));
    mainLayout->addWidget(tableTypeLabel);

    m_tableTypeCombo = new QComboBox();
    m_tableTypeCombo->addItem(tr("MSDOS (MBR)"), "msdos");
    m_tableTypeCombo->addItem(tr("GPT"), "gpt");
    m_tableTypeCombo->addItem(tr("AMOEBA"), "amoeba");
    m_tableTypeCombo->addItem(tr("BSD"), "bsd");
    m_tableTypeCombo->addItem(tr("DVH"), "dvh");
    m_tableTypeCombo->addItem(tr("PC98"), "pc98");
    m_tableTypeCombo->addItem(tr("SUN"), "sun");
    m_tableTypeCombo->addItem(tr("LOOP"), "loop");
    mainLayout->addWidget(m_tableTypeCombo);

    // Пояснения к разным типам таблиц
    QLabel *explanationLabel = new QLabel(tr("<b>MSDOS (MBR):</b> Standard partition table for most PCs. Limited to 2TB disks and 4 primary partitions.<br/>"
                                          "<b>GPT:</b> Modern partition table with no practical limitations. Required for UEFI booting and disks larger than 2TB."));
    explanationLabel->setWordWrap(true);
    mainLayout->addWidget(explanationLabel);

    // Кнопки
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);
}

QString PartitionTableDialog::selectedTableType() const
{
    return m_tableTypeCombo->currentData().toString();
}
