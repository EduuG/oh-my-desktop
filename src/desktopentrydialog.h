#pragma once

#include <QDialog>
#include <QString>

class QCheckBox;
class QComboBox;
class QLineEdit;

class DesktopEntryDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DesktopEntryDialog(QWidget *parent = nullptr);
    QString entryName() const;
    QString executable() const;
    QString icon() const;
    QString comment() const;
    QString path() const;
    QString categories() const;
    QString entryType() const;
    QString destinationDirectory() const;
    QString fileName() const;
    bool runInTerminal() const;

private:
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_execEdit = nullptr;
    QLineEdit *m_iconEdit = nullptr;
    QLineEdit *m_commentEdit = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    QLineEdit *m_categoriesEdit = nullptr;
    QLineEdit *m_destinationEdit = nullptr;
    QLineEdit *m_fileNameEdit = nullptr;
    QComboBox *m_typeCombo = nullptr;
    QCheckBox *m_terminalCheck = nullptr;
};
