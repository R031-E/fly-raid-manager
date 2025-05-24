#ifndef DELETEPARTITIONDIALOG_H
#define DELETEPARTITIONDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QIcon>

class DeletePartitionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DeletePartitionDialog(const QString &partitionPath, QWidget *parent = nullptr);
    bool isConfirmed() const;

private:
    QString m_partitionPath;
    QCheckBox *m_confirmCheckBox;
    void setupUi();
};

#endif // DELETEPARTITIONDIALOG_H
