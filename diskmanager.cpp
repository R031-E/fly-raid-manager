#include "diskmanager.h"
#include <QDir>
#include <QProcess>
#include <QDebug>
#include <QFile>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>

DiskManager::DiskManager(QObject *parent) : QObject(parent)
{

}

DiskManager::~DiskManager()
{
}


