#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidgetItem>
#include "diskmanager.h"
#include "partitiontabledialog.h"
#include "createpartitiondialog.h"
#include "deletepartitiondialog.h"
#include "formatpartitiondialog.h"
#include "createraidarraydialog.h"
#include "deleteraiddialog.h"
#include "flydirdialog.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Слот для обновления списка устройств
    void onRefreshDevices();

    // Слот для обработки изменения режима отображения
    void onViewModeChanged(int index);

    // Слот для обработки завершения обновления устройств
    void onDevicesRefreshed(bool success);

    // Слот для обработки вывода команды
    void onCommandOutput(const QString &output);

    // Слоты для кнопок управления RAID
    void onMarkFaultyClicked();
    void onAddToRaidClicked();
    void onRemoveFromRaidClicked();

    // Слоты для операций с разделами
    void onCreatePartitionTableClicked();
    void onCreatePartitionClicked();
    void onDeletePartitionClicked();
    void onFormatPartitionClicked();

    // Новые слоты для монтирования/размонтирования
    void onMountPartitionClicked();
    void onUnmountPartitionClicked();

    // Слоты для обработки результатов монтирования/размонтирования
    void onDeviceMounted(bool success, const QString &devicePath, const QString &mountPoint);
    void onDeviceUnmounted(bool success, const QString &devicePath);

    //Слоты для RAID
    void onCreateRaidClicked();
    void onDeleteRaidClicked();

private:
    Ui::MainWindow *ui;
    DiskManager *m_diskManager;
    bool m_showAllDevices; // true = показывать все устройства, false = только RAID

    // Обновление интерфейса в соответствии с текущим режимом
    void updateInterface();

    void setupInitialInterface();

    // ОБНОВИТЬ сигнатуры существующих методов:
    void populateAllDevicesView(const DiskStructure& diskStructure);
    void populateRaidView(const DiskStructure& diskStructure);

    // Добавление диска в таблицу
    QTreeWidgetItem* addDiskToTree(const DiskInfo &disk);

    // Добавление раздела в таблицу
    QTreeWidgetItem* addPartitionToTree(QTreeWidgetItem* parentItem, const PartitionInfo &partition);

    // Добавление RAID-массива в таблицу
    QTreeWidgetItem* addRaidToTree(const RaidInfo &raid);

    // Добавление устройства RAID в таблицу
    QTreeWidgetItem* addRaidMemberToTree(QTreeWidgetItem* parentItem, const RaidMemberInfo &member);

    // Обновление доступности кнопок в зависимости от выбранного элемента
    void updateButtonState();

    // Проверка, является ли выбранный элемент разделом или RAID-массивом
    bool isSelectedItemPartition() const;
    bool isSelectedItemRaid() const;
    bool isSelectedItemMountable() const;
    bool isSelectedItemRaidMember() const;

    // Получение пути к выбранному устройству
    QString getSelectedDevicePath() const;
    QString getSelectedPartitionPath() const;
};

#endif // MAINWINDOW_H
