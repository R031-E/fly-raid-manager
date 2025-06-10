#ifndef FORMATPARTITIONDIALOG_H
#define FORMATPARTITIONDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QTextEdit>

class FormatPartitionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FormatPartitionDialog(const QString &partitionPath, QWidget *parent = nullptr);

    QString getSelectedFilesystem() const;
    QString getVolumeLabel() const;
    bool isConfirmed() const;

private slots:
    void onFilesystemChanged();
    void onConfirmationChanged();
    void updateFormatButton();

private:
    QString m_partitionPath;

    // Элементы интерфейса
    QComboBox *m_filesystemCombo;
    //QLineEdit *m_volumeLabelEdit;
    QCheckBox *m_confirmCheckBox;
    QLabel *m_warningLabel;
    QTextEdit *m_descriptionText;
    QPushButton *m_formatButton;

    void setupUi();
    void setupFilesystemTypes();
    void updateDescription();
    QString getFilesystemDescription(const QString &filesystem) const;
};

#endif // FORMATPARTITIONDIALOG_H
