#include "deletepartitiondialog.h"
#include <QMessageBox>
#include <QSpacerItem>

DeletePartitionDialog::DeletePartitionDialog(const QString &partitionPath, QWidget *parent)
    : QDialog(parent), m_partitionPath(partitionPath)
{
    setWindowTitle(tr("Delete Partition"));
    setWindowIcon(QIcon::fromTheme("dialog-warning"));
    setFixedSize(450, 500);

    setupUi();
}

void DeletePartitionDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Иконка предупреждения и заголовок
    QHBoxLayout *headerLayout = new QHBoxLayout();

    QLabel *iconLabel = new QLabel();
    iconLabel->setPixmap(QIcon::fromTheme("dialog-warning").pixmap(48, 48));
    iconLabel->setAlignment(Qt::AlignTop);
    headerLayout->addWidget(iconLabel);

    QLabel *titleLabel = new QLabel(tr("<b>Delete Partition</b>"));
    titleLabel->setStyleSheet("QLabel { font-size: 14pt; color: #d32f2f; }");
    titleLabel->setAlignment(Qt::AlignTop);
    headerLayout->addWidget(titleLabel);

    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // Информация о разделе
    QLabel *partitionLabel = new QLabel(tr("Partition to delete: <b>%1</b>").arg(m_partitionPath));
    partitionLabel->setStyleSheet("QLabel { font-size: 11pt; padding: 5px; }");
    partitionLabel->setWordWrap(true);
    mainLayout->addWidget(partitionLabel);

    // Предупреждающее сообщение
    QLabel *warningLabel = new QLabel(tr("<b>Warning:</b> This action will permanently delete the partition "
                                       "and all data stored on it. This operation cannot be undone."));
    warningLabel->setWordWrap(true);
    warningLabel->setStyleSheet("QLabel { color: #d32f2f; background-color: #ffeaa7; "
                               "border: 1px solid #fdcb6e; border-radius: 5px; "
                               "padding: 10px; font-size: 10pt; }");
    mainLayout->addWidget(warningLabel);

    // Дополнительное предупреждение
    QLabel *dataWarningLabel = new QLabel(tr("Make sure you have backed up all important data before proceeding."));
    dataWarningLabel->setWordWrap(true);
    dataWarningLabel->setStyleSheet("QLabel { font-size: 10pt; color: #555; "
                                   "font-style: italic; padding: 5px; }");
    mainLayout->addWidget(dataWarningLabel);

    // Чекбокс подтверждения
    m_confirmCheckBox = new QCheckBox(tr("I understand that all data will be permanently lost"));
    m_confirmCheckBox->setStyleSheet("QCheckBox { font-size: 10pt; font-weight: bold; }");
    mainLayout->addWidget(m_confirmCheckBox);

    // Растягивающий элемент
    mainLayout->addStretch();

    // Кнопки
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox->setStyleSheet("QPushButton { font-size: 11pt; padding: 8px 20px; }");

    QPushButton *deleteButton = buttonBox->button(QDialogButtonBox::Ok);
    deleteButton->setText(tr("Delete Partition"));
    deleteButton->setIcon(QIcon::fromTheme("edit-delete"));
    deleteButton->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-weight: bold; }");
    deleteButton->setEnabled(false); // Изначально недоступна

    QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel);
    cancelButton->setText(tr("Cancel"));
    cancelButton->setIcon(QIcon::fromTheme("dialog-cancel"));

    // Подключаем сигналы
    connect(m_confirmCheckBox, &QCheckBox::toggled, deleteButton, &QPushButton::setEnabled);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);
}

bool DeletePartitionDialog::isConfirmed() const
{
    return m_confirmCheckBox->isChecked();
}
