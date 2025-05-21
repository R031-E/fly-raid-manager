#include "createpartitiondialog.h"
#include <QMessageBox>

CreatePartitionDialog::CreatePartitionDialog(const QString &devicePath, QWidget *parent)
    : QDialog(parent), m_devicePath(devicePath)
{
    setWindowTitle(tr("Create Partition"));
    setMinimumWidth(450);

    setupUi();
    updateInterface();
}

void CreatePartitionDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Информация об устройстве
    QLabel *deviceLabel = new QLabel(tr("Device: %1").arg(m_devicePath));
    mainLayout->addWidget(deviceLabel);

    // Форма для параметров
    QFormLayout *formLayout = new QFormLayout();

    // Тип раздела
    QLabel *partTypeLabel = new QLabel(tr("Partition type:"));
    m_partTypeCombo = new QComboBox();
    m_partTypeCombo->addItem(tr("Primary"), "primary");
    m_partTypeCombo->addItem(tr("Extended"), "extended");
    m_partTypeCombo->addItem(tr("Logical"), "logical");
    formLayout->addRow(partTypeLabel, m_partTypeCombo);

    // Способ задания размера
    QGroupBox *sizeGroupBox = new QGroupBox(tr("Partition size"));
    QVBoxLayout *sizeLayout = new QVBoxLayout(sizeGroupBox);

    m_rbUsePercent = new QRadioButton(tr("Use percentage of disk"));
    m_rbUseExact = new QRadioButton(tr("Use exact size"));
    m_rbUseRange = new QRadioButton(tr("Use start and end positions"));

    m_rbUsePercent->setChecked(true);

    sizeLayout->addWidget(m_rbUsePercent);
    sizeLayout->addWidget(m_rbUseExact);
    sizeLayout->addWidget(m_rbUseRange);

    // Поля для процентного размера
    QHBoxLayout *percentLayout = new QHBoxLayout();
    m_percentSpin = new QSpinBox();
    m_percentSpin->setMinimum(1);
    m_percentSpin->setMaximum(100);
    m_percentSpin->setValue(100);
    m_percentSpin->setSuffix(tr(" %"));
    percentLayout->addWidget(m_percentSpin);
    percentLayout->addStretch();
    sizeLayout->addLayout(percentLayout);

    // Поля для точного размера
    QHBoxLayout *exactLayout = new QHBoxLayout();
    m_exactSizeSpin = new QSpinBox();
    m_exactSizeSpin->setMinimum(1);
    m_exactSizeSpin->setMaximum(100000);
    m_exactSizeSpin->setValue(1024);

    m_exactSizeUnit = new QComboBox();
    m_exactSizeUnit->addItem(tr("MB"), "MB");
    m_exactSizeUnit->addItem(tr("GB"), "GB");
    m_exactSizeUnit->addItem(tr("TB"), "TB");
    m_exactSizeUnit->setCurrentIndex(1); // GB по умолчанию

    exactLayout->addWidget(m_exactSizeSpin);
    exactLayout->addWidget(m_exactSizeUnit);
    exactLayout->addStretch();
    sizeLayout->addLayout(exactLayout);

    // Поля для диапазона
    QGridLayout *rangeLayout = new QGridLayout();
    QLabel *startLabel = new QLabel(tr("Start:"));
    QLabel *endLabel = new QLabel(tr("End:"));
    m_startEdit = new QLineEdit("0%");
    m_endEdit = new QLineEdit("100%");

    rangeLayout->addWidget(startLabel, 0, 0);
    rangeLayout->addWidget(m_startEdit, 0, 1);
    rangeLayout->addWidget(endLabel, 1, 0);
    rangeLayout->addWidget(m_endEdit, 1, 1);

    sizeLayout->addLayout(rangeLayout);

    // Файловая система
    QLabel *fsLabel = new QLabel(tr("File system:"));
    m_fsCombo = new QComboBox();
    m_fsCombo->addItem(tr("ext4"), "ext4");
    m_fsCombo->addItem(tr("ext3"), "ext3");
    m_fsCombo->addItem(tr("ext2"), "ext2");
    m_fsCombo->addItem(tr("xfs"), "xfs");
    m_fsCombo->addItem(tr("btrfs"), "btrfs");
    m_fsCombo->addItem(tr("fat32"), "fat32");
    m_fsCombo->addItem(tr("ntfs"), "ntfs");
    m_fsCombo->addItem(tr("swap"), "linux-swap");
    m_fsCombo->addItem(tr("None"), "none");
    formLayout->addRow(fsLabel, m_fsCombo);

    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(sizeGroupBox);

    // Подключаем сигналы
    connect(m_partTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CreatePartitionDialog::onPartitionTypeChanged);
    connect(m_rbUsePercent, &QRadioButton::toggled, this, &CreatePartitionDialog::onSizeTypeChanged);
    connect(m_rbUseExact, &QRadioButton::toggled, this, &CreatePartitionDialog::onSizeTypeChanged);
    connect(m_rbUseRange, &QRadioButton::toggled, this, &CreatePartitionDialog::onSizeTypeChanged);

    // Кнопки
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(m_buttonBox);
}

void CreatePartitionDialog::onPartitionTypeChanged(int index)
{
    // Если выбран тип Extended, файловая система не нужна
    bool isExtended = (m_partTypeCombo->currentData().toString() == "extended");
    m_fsCombo->setEnabled(!isExtended);

    if (isExtended) {
        m_fsCombo->setCurrentText(tr("None"));
    }

    updateInterface();
}

void CreatePartitionDialog::onSizeTypeChanged()
{
    updateInterface();
}

void CreatePartitionDialog::updateInterface()
{
    // Включаем/выключаем элементы в зависимости от выбранного способа задания размера
    m_percentSpin->setEnabled(m_rbUsePercent->isChecked());

    m_exactSizeSpin->setEnabled(m_rbUseExact->isChecked());
    m_exactSizeUnit->setEnabled(m_rbUseExact->isChecked());

    m_startEdit->setEnabled(m_rbUseRange->isChecked());
    m_endEdit->setEnabled(m_rbUseRange->isChecked());
}

QString CreatePartitionDialog::partitionType() const
{
    return m_partTypeCombo->currentData().toString();
}

QString CreatePartitionDialog::startPosition() const
{
    if (m_rbUsePercent->isChecked()) {
        // Для процентов начало всегда с 0%
        return "0%";
    } else if (m_rbUseExact->isChecked()) {
        // Для точного размера - начало всегда с 0%
        return "0%";
    } else {
        // Для диапазона - возвращаем введенное значение
        return m_startEdit->text();
    }
}

QString CreatePartitionDialog::endPosition() const
{
    if (m_rbUsePercent->isChecked()) {
        // Для процентов конец определяется процентом
        return QString("%1%").arg(m_percentSpin->value());
    } else if (m_rbUseExact->isChecked()) {
        // Для точного размера - вычисляем размер в нужных единицах
        int size = m_exactSizeSpin->value();
        QString unit = m_exactSizeUnit->currentData().toString();
        return QString("%1%2").arg(size).arg(unit);
    } else {
        // Для диапазона - возвращаем введенное значение
        return m_endEdit->text();
    }
}

QString CreatePartitionDialog::fileSystem() const
{
    return m_fsCombo->currentData().toString();
}
