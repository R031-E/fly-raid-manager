#ifndef COMMANDEXECUTOR_H
#define COMMANDEXECUTOR_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <su/su.h>

class CommandExecutor : public QObject
{
    Q_OBJECT
public:
    explicit CommandExecutor(QObject *parent = nullptr);
    ~CommandExecutor();

    bool executeAsAdmin(const QString &command, const QStringList &args = QStringList());
    bool execute(const QString &command, const QStringList &args = QStringList());
    int executeAsAdminSync(const QString &command, const QStringList &args,
                          QString &output, QString &errorOutput);
    int executeSync(const QString &command, const QStringList &args,
                   QString &output, QString &errorOutput);
    void abort();
    static bool hasAdminRights();

signals:
    void started(const QString &command);
    void finished(int exitCode, QProcess::ExitStatus exitStatus);
    void outputAvailable(const QString &data);
    void errorOutputAvailable(const QString &data);
    void error(QProcess::ProcessError error);

private slots:
    void handleStandardOutput();
    void handleErrorOutput();
    void handleFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleError(QProcess::ProcessError error);

private:
    QProcess *m_process; // Процесс для выполнения команд
    void setupProcessConnections();
};

#endif // COMMANDEXECUTOR_H
