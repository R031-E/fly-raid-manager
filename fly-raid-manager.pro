TEMPLATE = app
TARGET = fly-raid-manager

QT       += core gui
QT += widgets

LIBS += -lflyintegration -lflyqtconfig -lflycore -lflyuiaux -lflyuiextra -lflypty -lflysu
INCLUDEPATH += /usr/include/fly

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

TRANSLATIONS += \
    fly-raid-manager_ru_RU.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
