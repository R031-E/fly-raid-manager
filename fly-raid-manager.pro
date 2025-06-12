TEMPLATE = app
TARGET = fly-admin-raid-manager

QT       += core gui
QT += widgets

LIBS += -lflyintegration -lflyqtconfig -lflycore -lflyuiaux -lflyuiextra -lflypty -lflysu -lflyfiledialog
INCLUDEPATH += /usr/include/fly

SOURCES += \
    commandexecutor.cpp \
    createpartitiondialog.cpp \
    createraidarraydialog.cpp \
    deletepartitiondialog.cpp \
    deleteraiddialog.cpp \
    diskmanager.cpp \
    diskstructures.cpp \
    formatpartitiondialog.cpp \
    main.cpp \
    mainwindow.cpp \
    partitiontabledialog.cpp

HEADERS += \
    commandexecutor.h \
    createpartitiondialog.h \
    createraidarraydialog.h \
    deletepartitiondialog.h \
    deleteraiddialog.h \
    diskmanager.h \
    diskstructures.h \
    formatpartitiondialog.h \
    mainwindow.h \
    partitiontabledialog.h

FORMS += \
    createraidarraydialog.ui \
    deleteraiddialog.ui \
    mainwindow.ui

TRANSLATIONS += \
    fly-raid-manager_ru_RU.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
