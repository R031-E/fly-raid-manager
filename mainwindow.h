#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidgetItem>
#include "diskmanager.h"
#include "partitiontabledialog.h"
#include "createpartitiondialog.h"
#include "deletepartitiondialog.h"

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

private:
    Ui::MainWindow *ui;
    DiskManager *m_diskManager;
    bool m_showAllDevices; // true = показывать все устройства, false = только RAID

    // Обновление интерфейса в соответствии с текущим режимом
    void updateInterface();

    // Заполнение таблицы всеми устройствами
    void populateAllDevicesView();

    // Заполнение таблицы только RAID-устройствами
    void populateRaidView();

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

    // Проверка, является ли выбранный элемент разделом
    bool isSelectedItemPartition() const;

    // Получение пути к выбранному разделу
    QString getSelectedPartitionPath() const;
};

#endif // MAINWINDOW_H
