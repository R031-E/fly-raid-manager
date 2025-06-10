#ifndef CREATERAIDARRAYDIALOG_H
#define CREATERAIDARRAYDIALOG_H

#include <QDialog>
#include <QTreeWidgetItem>
#include <QCheckBox>
#include <QTimer>
#include <QPushButton>
#include "diskstructures.h"

namespace Ui {
class CreateRaidArrayDialog;
}

/**
 * @brief Структура для хранения информации об устройстве, доступном для RAID
 */
struct RaidDeviceCandidate {
    QString devicePath;
    QString size;
    QString type;          // "disk", "partition"
    QString filesystem;
    bool isMounted;
    bool isInRaid;
    bool isSelected;

    RaidDeviceCandidate(const QString &path = QString(), const QString &sz = QString(),
                       const QString &tp = QString(), const QString &fs = QString())
        : devicePath(path), size(sz), type(tp), filesystem(fs),
          isMounted(false), isInRaid(false), isSelected(false) {}
};

/**
 * @brief Диалог для создания RAID массивов
 */
class CreateRaidArrayDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CreateRaidArrayDialog(const DiskStructure &diskStructure, QWidget *parent = nullptr);
    ~CreateRaidArrayDialog();

    // Геттеры для получения выбранных параметров
    RaidType getSelectedRaidType() const;
    QString getArrayName() const;
    QStringList getSelectedDevices() const;

signals:
    // Сигналы для взаимодействия с DiskManager
    void createRaidRequested(RaidType raidType, const QStringList &devices, const QString &arrayName);

public slots:
    // Слоты для получения обновлений от DiskManager
    void onWipeProgressUpdate(const QString &device, bool success);
    void onRaidCreationProgress(const QString &message);
    void onRaidCreationCompleted(bool success, const QString &raidDevice);

protected:
    void accept() override;
    void reject() override;

private slots:
    void onRaidLevelChanged();
    void onDeviceSelectionChanged();
    void updateSelectedDevicesInfo();
    void onArrayNameChanged();

private:
    Ui::CreateRaidArrayDialog *ui;

    // Данные
    QList<RaidDeviceCandidate> m_availableDevices;
    bool m_operationInProgress;
    int m_currentStep;
    int m_totalSteps;
    QStringList m_selectedDevices;

    // Методы инициализации
    void setupDialog();
    void populateDeviceTree(const DiskStructure &diskStructure);
    void setupRaidLevelCombo();
    void setupConnections();

    // Методы для работы с устройствами
    /*void addDeviceToTree(const QString &devicePath, const QString &size,
                        const QString &type, const QString &filesystem,
                        bool isMounted, bool isInRaid);*/
    void updateDeviceSelection();

    // Валидация
    bool validateSelection(QString &errorMessage) const;
    int getMinimumDevicesForRaidLevel(RaidType raidType) const;
    int getSelectedDeviceCount() const;

    // Управление прогрессом операций
    void startRaidCreation();
    void showProgress(bool show);
    void updateProgress(int step, const QString &operation);
    void logOperation(const QString &message);
    void setOperationInProgress(bool inProgress);

    // Вспомогательные методы
    QString raidTypeToString(RaidType type) const;
    RaidType getRaidTypeFromIndex(int index) const;
    QString formatDeviceInfo(const RaidDeviceCandidate &device) const;
};

#endif // CREATERAIDARRAYDIALOG_H
