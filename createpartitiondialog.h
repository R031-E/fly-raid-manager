#ifndef CREATEPARTITIONDIALOG_H
#define CREATEPARTITIONDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QSpinBox>  // Вернулись к QSpinBox
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QTextEdit>
#include <QGroupBox>
#include <QScrollArea>

/**
 * @brief Структура для хранения информации о свободной области диска
 */
struct FreeSpaceInfo {
    qint64 startMiB;    // Начало в МиБ (вернулись к qint64)
    qint64 endMiB;      // Конец в МiБ (вернулись к qint64)
    qint64 sizeMiB;     // Размер в MiБ (вернулись к qint64)

    FreeSpaceInfo(qint64 start = 0, qint64 end = 0, qint64 size = 0)
        : startMiB(start), endMiB(end), sizeMiB(size) {}
};

/**
 * @brief Диалог для создания раздела с фиксированными размерами полей
 */
class CreatePartitionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CreatePartitionDialog(const QString &devicePath, QWidget *parent = nullptr);

    // Геттеры для получения введенных данных
    QString getPartitionType() const;
    QString getFilesystemType() const;
    QString getStartSize() const;
    QString getEndSize() const;
    qint64 getStartValue() const;
    qint64 getEndValue() const;
    qint64 getSizeValue() const;
    bool validateInput();

public slots:
    void onFreeSpaceInfoOnDeviceReceived(const QString &info);

protected:
    void accept() override;

private slots:
    void onStartPositionChanged();
    void onSizeChanged();
    void onEndPositionChanged();
    void onFreeSpaceAreaChanged();
    void updateEndPosition();
    void updateSizeFromPositions();
    void updateStartFromPositions();
    void updateValidation();

private:
    QString m_devicePath;
    QList<FreeSpaceInfo> m_freeSpaces;  // Список свободных областей
    QList<FreeSpaceInfo> m_validFreeSpaces;  // Только области >= 2 МиБ
    qint64 m_diskSizeMiB;               // Общий размер диска в МиБ

    // Элементы интерфейса
    QComboBox *m_partitionTypeCombo;
    QComboBox *m_filesystemCombo;
    QComboBox *m_freeSpaceAreaCombo;    // Выбор свободной области
    QLabel *m_diskInfoLabel;
    QLabel *m_freeSpacesLabel;

    // Поля для ввода параметров раздела (целые числа)
    QSpinBox *m_startPosSpinBox;        // Начальная позиция в МиБ
    QSpinBox *m_sizeSpinBox;            // Размер в МиБ
    QSpinBox *m_endPosSpinBox;          // Конечная позиция в МиБ (изменяемая)

    // Информационные метки с фиксированными размерами
    QLabel *m_validationLabel;          // Показывает ошибки валидации
    QScrollArea *m_validationScrollArea; // Скроллируемая область для длинных сообщений
    QGroupBox *m_validationGroup;       // Группа валидации для скрытия

    // Флаг для предотвращения рекурсивных обновлений
    bool m_updatingValues;

    /**
     * @brief Инициализация интерфейса диалога с фиксированными размерами
     */
    void setupUi();

    /**
     * @brief Заполнение доступных файловых систем
     */
    void setupFilesystemTypes();

    /**
     * @brief Заполнение списка доступных свободных областей (>= 2 МиБ)
     */
    void updateFreeSpaceCombo();

    /**
     * @brief Обновление ограничений на основе выбранной свободной области
     */
    void updateConstraintsForSelectedFreeSpace();

    /**
     * @brief Парсинг вывода команды parted print free (в МиБ)
     */
    void parseFreeSpaceInfo(const QString &output);

    /**
     * @brief Извлечение целого числа из строки МиБ с округлением вверх для дробных
     * Поддерживает запятую как разделитель дробной части
     * (например "5121,02MiB" -> 5122, "0,02MiB" -> 1, "10240MiB" -> 10240)
     */
    qint64 parseMiBValue(const QString &mibStr);

    /**
     * @brief Обновление видимости области валидации
     */
    void updateValidationVisibility();

    /**
     * @brief Получение текущей выбранной свободной области
     */
    FreeSpaceInfo* getCurrentFreeSpace();
};

#endif // CREATEPARTITIONDIALOG_H
