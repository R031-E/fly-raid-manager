#include "commandexecutor.h"
#include <QDebug>
#include <QCoreApplication>
#include <unistd.h>

CommandExecutor::CommandExecutor(QObject *parent) : QObject(parent)
{
    m_process = new QProcess(this);
    setupProcessConnections();
}

CommandExecutor::~CommandExecutor()
{
    if (m_process->state() == QProcess::Running) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
        }
    }
}

void CommandExecutor::setupProcessConnections()
{
    connect(m_process, &QProcess::started, [this]() {
        emit started(m_process->program() + " " + m_process->arguments().join(" "));
    });

    connect(m_process, &QProcess::errorOccurred, this, &CommandExecutor::handleError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &CommandExecutor::handleFinished);

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &CommandExecutor::handleStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &CommandExecutor::handleErrorOutput);
}

bool CommandExecutor::executeAsAdmin(const QString &command, const QStringList &args)
{
    if (m_process->state() != QProcess::NotRunning) {
        qWarning() << "Попытка запустить команду, когда процесс уже выполняется";
        return false;
    }
    QStringList fullArgs;
    fullArgs << command << args;
    qDebug() << fullArgs;
    m_process->start("sudo", fullArgs);
    return m_process->waitForStarted();
}

bool CommandExecutor::execute(const QString &command, const QStringList &args)
{
    if (m_process->state() != QProcess::NotRunning) {
        qWarning() << "Попытка запустить команду, когда процесс уже выполняется";
        return false;
    }

    m_process->start(command, args);
    return m_process->waitForStarted();
}

int CommandExecutor::executeAsAdminSync(const QString &command, const QStringList &args,
                                      QString &output, QString &errorOutput)
{
    QProcess process;
    QStringList fullArgs;
    fullArgs << command << args;

    process.start("pkexec", fullArgs);

    if (!process.waitForStarted()) {
        qWarning() << "Не удалось запустить команду с повышенными привелегиями:" << command;
        return -1;
    }

    process.waitForFinished(-1); // Ждем завершения

    output = QString::fromUtf8(process.readAllStandardOutput());
    errorOutput = QString::fromUtf8(process.readAllStandardError());

    return process.exitCode();
}

int CommandExecutor::executeSync(const QString &command, const QStringList &args,
                               QString &output, QString &errorOutput)
{
    QProcess process;
    process.start(command, args);

    if (!process.waitForStarted()) {
        qWarning() << "Не удалось запустить команду:" << command;
        return -1;
    }

    process.waitForFinished(-1); // Ждем завершения

    output = QString::fromUtf8(process.readAllStandardOutput());
    errorOutput = QString::fromUtf8(process.readAllStandardError());

    return process.exitCode();
}

void CommandExecutor::abort()
{
    if (m_process->state() == QProcess::Running) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
        }
    }
}

bool CommandExecutor::hasAdminRights()
{
    // Проверяем, запущен ли процесс с правами root (uid = 0)
    return geteuid() == 0;
}

void CommandExecutor::handleStandardOutput()
{
    QByteArray data = m_process->readAllStandardOutput();
    qDebug() << "Есть вывод";
    emit outputAvailable(QString::fromUtf8(data));
}

void CommandExecutor::handleErrorOutput()
{
    QByteArray data = m_process->readAllStandardError();
    qDebug() << "Есть ошибка";
    emit errorOutputAvailable(QString::fromUtf8(data));
}

void CommandExecutor::handleFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "Процесс завершился";
    emit finished(exitCode, exitStatus);
}

void CommandExecutor::handleError(QProcess::ProcessError error)
{
    qDebug() << "Ошибка";
    emit this->error(error);
}
