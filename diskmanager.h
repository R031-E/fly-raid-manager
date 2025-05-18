#ifndef DISKMANAGER_H
#define DISKMANAGER_H

#include <QObject>
#include <QMap>
#include "diskstructures.h"
#include "commandexecutor.h"

class DiskManager : public QObject
{
    Q_OBJECT
public:
    explicit DiskManager(QObject *parent = nullptr);
    ~DiskManager();
};

#endif // DISKMANAGER_H
