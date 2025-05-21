#ifndef CREATEPARTITIONDIALOG_H
#define CREATEPARTITIONDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QRadioButton>
#include <QGroupBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>

class CreatePartitionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CreatePartitionDialog(const QString &devicePath, QWidget *parent = nullptr);
    QString partitionType() const;
    QString startPosition() const;
    QString endPosition() const;
    QString fileSystem() const;

private slots:
    void onPartitionTypeChanged(int index);
    void onSizeTypeChanged();
    void updateInterface();

private:
    QString m_devicePath;
    QComboBox *m_partTypeCombo;
    QRadioButton *m_rbUsePercent;
    QRadioButton *m_rbUseExact;
    QRadioButton *m_rbUseRange;
    QSpinBox *m_percentSpin;
    QSpinBox *m_exactSizeSpin;
    QComboBox *m_exactSizeUnit;
    QLineEdit *m_startEdit;
    QLineEdit *m_endEdit;
    QComboBox *m_fsCombo;
    QDialogButtonBox *m_buttonBox;
    void setupUi();
};

#endif // CREATEPARTITIONDIALOG_H
