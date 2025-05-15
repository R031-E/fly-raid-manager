#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <flyintegration.h>

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
