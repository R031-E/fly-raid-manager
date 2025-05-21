#ifndef PARTITIONTABLEDIALOG_H
#define PARTITIONTABLEDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>

class PartitionTableDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PartitionTableDialog(const QString &devicePath, QWidget *parent = nullptr);
    QString selectedTableType() const;

private:
    QString m_devicePath;
    QComboBox *m_tableTypeCombo;
    void setupUi();
};

#endif // PARTITIONTABLEDIALOG_H
