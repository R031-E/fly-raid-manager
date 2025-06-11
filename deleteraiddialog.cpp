#include "deleteraiddialog.h"
#include "ui_deleteraiddialog.h"
#include <QMessageBox>
#include <QIcon>
#include <QDebug>

DeleteRaidDialog::DeleteRaidDialog(const RaidInfo &raidInfo, QWidget *parent)
    : QDialog(parent), ui(new Ui::DeleteRaidDialog), m_raidInfo(raidInfo),
      m_operationInProgress(false), m_currentStep(0), m_totalSteps(0)
{
    ui->setupUi(this);
    setupDialog();
    populateRaidInfo();
    setupConnections();

    // Устанавливаем иконку диалога
    ui->iconLabel->setPixmap(QIcon::fromTheme("dialog-warning").pixmap(48, 48));

    // Извлекаем устройства-члены RAID
    for (const RaidMemberInfo &member : m_raidInfo.members) {
        m_memberDevices.append(member.devicePath);
    }
}

DeleteRaidDialog::~DeleteRaidDialog()
{
    delete ui;
}

void DeleteRaidDialog::setupDialog()
{
    // Настройка диалога
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setModal(true);

    // Скрываем область прогресса изначально
    showProgress(false);
}

void DeleteRaidDialog::populateRaidInfo()
{
    // Заполняем информацию о RAID массиве
    ui->raidDeviceLabel->setText(tr("RAID Device: <b>%1</b>").arg(m_raidInfo.devicePath));
    ui->raidTypeLabel->setText(tr("RAID Type: <b>%1</b>").arg(DiskUtils::raidTypeToString(m_raidInfo.type)));
    ui->raidSizeLabel->setText(tr("Size: <b>%1</b>").arg(m_raidInfo.size));
    ui->raidStateLabel->setText(tr("State: <b>%1</b>").arg(m_raidInfo.state));

    // Заполняем список устройств-членов
    ui->memberDevicesList->clear();
    for (const RaidMemberInfo &member : m_raidInfo.members) {
        QString deviceInfo = tr("%1 (%2)")
                           .arg(member.devicePath)
                           .arg(DiskUtils::deviceStatusToString(member.status));
        ui->memberDevicesList->addItem(deviceInfo);
    }

    // Проверяем, смонтирован ли RAID массив
    if (!m_raidInfo.mountPoint.isEmpty()) {
        ui->warningLabel->setText(tr("<b>Warning:</b> RAID array is currently mounted at %1. "
                                   "Please unmount it before deletion. This operation will "
                                   "permanently destroy all data on the RAID array and cannot be undone.")
                                   .arg(m_raidInfo.mountPoint));
        ui->warningLabel->setStyleSheet("QLabel { color: #e74c3c; background-color: #fdeaea; "
                                       "border: 1px solid #f5b7b1; border-radius: 5px; "
                                       "padding: 10px; font-size: 10pt; }");
    } else {
        ui->warningLabel->setText(tr("<b>Warning:</b> This operation will permanently destroy "
                                   "all data on the RAID array and cannot be undone. "
                                   "Make sure you have backed up all important data."));
        ui->warningLabel->setStyleSheet("QLabel { color: #e74c3c; background-color: #fdeaea; "
                                       "border: 1px solid #f5b7b1; border-radius: 5px; "
                                       "padding: 10px; font-size: 10pt; }");
    }
}

void DeleteRaidDialog::setupConnections()
{
    // Переопределяем стандартные кнопки
    QPushButton *deleteButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    deleteButton->setText(tr("Delete RAID"));
    deleteButton->setIcon(QIcon::fromTheme("edit-delete"));

    QPushButton *cancelButton = ui->buttonBox->button(QDialogButtonBox::Cancel);
    cancelButton->setText(tr("Cancel"));
    cancelButton->setIcon(QIcon::fromTheme("dialog-cancel"));

    // Отключаем кнопку удаления если RAID смонтирован
    if (!m_raidInfo.mountPoint.isEmpty()) {
        deleteButton->setEnabled(false);
        deleteButton->setToolTip(tr("Please unmount the RAID array first"));
    }
}

QString DeleteRaidDialog::getRaidDevicePath() const
{
    return m_raidInfo.devicePath;
}

QStringList DeleteRaidDialog::getRaidMemberDevices() const
{
    return m_memberDevices;
}

bool DeleteRaidDialog::validateDeletion(QString &errorMessage) const
{
    // Проверяем, что RAID не смонтирован
    if (!m_raidInfo.mountPoint.isEmpty()) {
        errorMessage = tr("RAID array is currently mounted at %1. Please unmount it first.")
                       .arg(m_raidInfo.mountPoint);
        return false;
    }

    // Проверяем, что есть устройства для удаления
    if (m_memberDevices.isEmpty()) {
        errorMessage = tr("No member devices found for RAID array %1.")
                       .arg(m_raidInfo.devicePath);
        return false;
    }

    return true;
}

void DeleteRaidDialog::accept()
{
    // Валидируем операцию удаления
    QString errorMessage;
    if (!validateDeletion(errorMessage)) {
        QMessageBox::warning(this, tr("Cannot Delete RAID"), errorMessage);
        return;
    }

    // Запрашиваем подтверждение
    QMessageBox::StandardButton reply;
    QString confirmMessage = tr("Are you sure you want to delete RAID array %1?\n\n"
                               "This operation will:\n"
                               "• Stop the RAID array\n"
                               "• Clear superblocks from all member devices\n"
                               "• Wipe filesystem signatures from all member devices\n\n"
                               "The following devices will be affected:\n")
                            .arg(m_raidInfo.devicePath);

    for (const QString &device : m_memberDevices) {
        confirmMessage += tr("• %1\n").arg(device);
    }

    confirmMessage += tr("\nAll data will be permanently lost and cannot be recovered!");

    reply = QMessageBox::question(this, tr("Confirm RAID Deletion"), confirmMessage,
                                 QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    // Начинаем процесс удаления RAID
    startRaidDeletion();
}

void DeleteRaidDialog::reject()
{
    if (m_operationInProgress) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, tr("Cancel Operation"),
                                     tr("RAID deletion is in progress. Are you sure you want to cancel?\n\n"
                                        "Note: This may leave the system in an inconsistent state."),
                                     QMessageBox::Yes | QMessageBox::No);

        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    QDialog::reject();
}

void DeleteRaidDialog::startRaidDeletion()
{
    setOperationInProgress(true);
    showProgress(true);

    // Рассчитываем общее количество шагов
    // 1 - остановка RAID + количество устройств для очистки суперблоков + количество устройств для wipefs
    m_totalSteps = 1 + (m_memberDevices.size() * 2);
    m_currentStep = 0;

    updateProgress(0, tr("Stopping RAID array..."));

    // Отправляем сигнал для начала удаления RAID
    emit deleteRaidRequested(m_raidInfo.devicePath, m_memberDevices);
}

void DeleteRaidDialog::showProgress(bool show)
{
    ui->progressGroup->setVisible(show);

    if (show) {
        // Переводим диалог в режим прогресса
        ui->raidInfoGroup->setEnabled(false);
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));

        // Изменяем размер диалога
        setFixedSize(600, 650);
    } else {
        // Возвращаем диалог в обычный режим
        ui->raidInfoGroup->setEnabled(true);
        ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));

        // Изменяем размер диалога обратно
        setFixedSize(600, 500);
    }
}

void DeleteRaidDialog::updateProgress(int step, const QString &operation)
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

void DeleteRaidDialog::logOperation(const QString &message)
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

void DeleteRaidDialog::setOperationInProgress(bool inProgress)
{
    m_operationInProgress = inProgress;

    if (!inProgress) {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Close"));
    }
}

// Слоты для обработки сигналов от DiskManager

void DeleteRaidDialog::onRaidStopProgress(const QString &message)
{
    logOperation(message);
}

void DeleteRaidDialog::onRaidStopCompleted(bool success, const QString &raidDevice)
{
    m_currentStep++;

    if (success) {
        updateProgress(m_currentStep, tr("RAID array stopped successfully"));
        logOperation(tr("✓ RAID array %1 stopped successfully").arg(raidDevice));
    } else {
        updateProgress(m_currentStep, tr("Failed to stop RAID array"));
        logOperation(tr("✗ Failed to stop RAID array %1").arg(raidDevice));
    }
}

void DeleteRaidDialog::onSuperblockCleanProgress(const QString &device, bool success)
{
    m_currentStep++;

    if (success) {
        updateProgress(m_currentStep, tr("Superblock cleared from %1").arg(device));
        logOperation(tr("✓ Superblock cleared from %1").arg(device));
    } else {
        updateProgress(m_currentStep, tr("Failed to clear superblock from %1").arg(device));
        logOperation(tr("✗ Failed to clear superblock from %1").arg(device));
    }
}

void DeleteRaidDialog::onWipeProgressUpdate(const QString &device, bool success)
{
    m_currentStep++;

    if (success) {
        updateProgress(m_currentStep, tr("Device %1 wiped successfully").arg(device));
        logOperation(tr("✓ wipefs completed successfully for %1").arg(device));
    } else {
        updateProgress(m_currentStep, tr("Failed to wipe device %1").arg(device));
        logOperation(tr("✗ wipefs failed for %1").arg(device));
    }
}

void DeleteRaidDialog::onRaidDeletionCompleted(bool success, const QString &raidDevice)
{
    setOperationInProgress(false);

    if (success) {
        updateProgress(m_totalSteps, tr("RAID array deleted successfully"));
        logOperation(tr("✓ RAID array %1 deleted successfully!").arg(raidDevice));

        // Показываем сообщение об успехе
        QMessageBox::information(this, tr("RAID Deleted"),
                                tr("RAID array %1 has been deleted successfully!\n\n"
                                   "All member devices are now available for other uses.")
                                   .arg(raidDevice));

        // Закрываем диалог с успехом
        QDialog::accept();
    } else {
        updateProgress(m_currentStep, tr("RAID deletion failed"));
        logOperation(tr("✗ Failed to delete RAID array"));

        // Показываем сообщение об ошибке
        QMessageBox::critical(this, tr("RAID Deletion Failed"),
                             tr("Failed to delete RAID array. Please check the operation log "
                                "for details. The system may be in an inconsistent state."));

        // Возвращаем диалог в рабочее состояние, но оставляем кнопки неактивными
        showProgress(false);
        ui->raidInfoGroup->setEnabled(false);
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
}
