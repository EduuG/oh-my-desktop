#pragma once

#include <QDialog>
#include <QFileInfo>

class QLineEdit;
class QComboBox;
class QCheckBox;

class FilePropertiesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FilePropertiesDialog(const QFileInfo &info, QWidget *parent = nullptr);

private:
    void loadDesktopEntryFields();
    bool saveDesktopEntryFields();

    QFileInfo m_info;
    bool m_isDesktopEntry = false;
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_execEdit = nullptr;
    QLineEdit *m_iconEdit = nullptr;
    QLineEdit *m_commentEdit = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    QLineEdit *m_categoriesEdit = nullptr;
    QComboBox *m_typeCombo = nullptr;
    QCheckBox *m_terminalCheck = nullptr;
};
