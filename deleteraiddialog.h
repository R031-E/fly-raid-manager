#ifndef DELETERAIDDIALOG_H
#define DELETERAIDDIALOG_H

#include <QDialog>
#include <QTimer>
#include <QPushButton>
#include "diskstructures.h"

namespace Ui {
class DeleteRaidDialog;
}

/**
 * @brief Диалог для удаления RAID массивов
 */
class DeleteRaidDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DeleteRaidDialog(const RaidInfo &raidInfo, QWidget *parent = nullptr);
    ~DeleteRaidDialog();

    // Геттеры для получения параметров
    QString getRaidDevicePath() const;
    QStringList getRaidMemberDevices() const;

signals:
    // Сигналы для взаимодействия с DiskManager
    void deleteRaidRequested(const QString &raidDevice, const QStringList &memberDevices);

public slots:
    // Слоты для получения обновлений от DiskManager
    void onRaidStopProgress(const QString &message);
    void onRaidStopCompleted(bool success, const QString &raidDevice);
    void onSuperblockCleanProgress(const QString &device, bool success);
    void onWipeProgressUpdate(const QString &device, bool success);
    void onRaidDeletionCompleted(bool success, const QString &raidDevice);

protected:
    void accept() override;
    void reject() override;

private slots:
    void startRaidDeletion();

private:
    Ui::DeleteRaidDialog *ui;

    // Данные
    RaidInfo m_raidInfo;
    QStringList m_memberDevices;
    bool m_operationInProgress;
    int m_currentStep;
    int m_totalSteps;

    // Методы инициализации
    void setupDialog();
    void setupConnections();
    void populateRaidInfo();

    // Управление прогрессом операций
    void showProgress(bool show);
    void updateProgress(int step, const QString &operation);
    void logOperation(const QString &message);
    void setOperationInProgress(bool inProgress);

    // Валидация
    bool validateDeletion(QString &errorMessage) const;
};

#endif // DELETERAIDDIALOG_H
