#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <flyintegration.h>

/* TODO for MVP:
     1) Все RAID операции
     2) Форматирование разделов
     4) Создание файловой системы
     5) Вывод информации об устройствах в поле информации
     6) Контекстное меню при клике ПКМ по строчке в таблице
     7) Динамическая доступность кнопок для нажатия
     8) Перевод на русский
*/

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    flyInit("2.0.0", QT_TRANSLATE_NOOP("Fly", "Управление RAID-массивами"));

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "fly-raid-manager_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    MainWindow w;
    w.show();
    return a.exec();
}
