#include "desktopentrydialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <functional>

namespace {
QWidget *pathRow(QWidget *parent, QLineEdit **edit, const QString &buttonText,
                 const std::function<QString()> &picker)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *line = new QLineEdit(row);
    layout->addWidget(line, 1);
    auto *button = new QPushButton(buttonText, row);
    layout->addWidget(button);
    QObject::connect(button, &QPushButton::clicked, row, [line, picker] {
        const QString value = picker();
        if (!value.isEmpty())
            line->setText(value);
    });
    *edit = line;
    return row;
}
}

DesktopEntryDialog::DesktopEntryDialog(QWidget *parent)
    : QDialog(parent, Qt::Dialog)
{
    setWindowTitle("New Desktop Entry");
    setObjectName("DesktopEntryDialog");
    setMinimumWidth(620);

    auto *root = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("Application name");
    m_execEdit = new QLineEdit(this);
    m_execEdit->setPlaceholderText("Command or executable");
    m_commentEdit = new QLineEdit(this);
    m_commentEdit->setPlaceholderText("Short description (optional)");
    m_categoriesEdit = new QLineEdit(this);
    m_categoriesEdit->setText("Utility;");
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItems({"Application", "Link", "Directory"});

    auto *iconRow = pathRow(this, &m_iconEdit, "Browse", [this] {
        return QFileDialog::getOpenFileName(this, "Select icon", QString(),
            "Images (*.png *.svg *.xpm *.jpg *.jpeg);;All files (*)");
    });
    m_iconEdit->setPlaceholderText("Icon name or file path (optional)");
    auto *pathRowWidget = pathRow(this, &m_pathEdit, "Browse", [this] {
        return QFileDialog::getExistingDirectory(this, "Select working directory");
    });
    m_pathEdit->setPlaceholderText("Working directory (optional)");
    const QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    auto *destinationRow = pathRow(this, &m_destinationEdit, "Browse", [this] {
        return QFileDialog::getExistingDirectory(this, "Select destination directory");
    });
    m_destinationEdit->setText(desktop);
    m_fileNameEdit = new QLineEdit(this);
    m_fileNameEdit->setText("Application.desktop");

    form->addRow("Type:", m_typeCombo);
    form->addRow("Name:", m_nameEdit);
    form->addRow("Exec:", m_execEdit);
    form->addRow("Icon:", iconRow);
    form->addRow("Path:", pathRowWidget);
    form->addRow("Comment:", m_commentEdit);
    form->addRow("Categories:", m_categoriesEdit);
    form->addRow("Destination:", destinationRow);
    form->addRow("Filename:", m_fileNameEdit);

    m_terminalCheck = new QCheckBox("Run in terminal", this);
    form->addRow(QString(), m_terminalCheck);
    root->addLayout(form);

    auto *hint = new QLabel(
        "The launcher is written atomically and marked executable. Creating it never runs the command.", this);
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        const QString name = m_nameEdit->text().trimmed();
        const QString type = m_typeCombo->currentText();
        const QString destination = m_destinationEdit->text().trimmed();
        QString fileName = m_fileNameEdit->text().trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(this, "Desktop Entry", "Name cannot be empty.");
            m_nameEdit->setFocus();
            return;
        }
        if (type == "Application" && m_execEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Desktop Entry", "Exec cannot be empty for an application.");
            m_execEdit->setFocus();
            return;
        }
        if (destination.isEmpty() || !QDir(destination).exists()) {
            QMessageBox::warning(this, "Desktop Entry", "Choose an existing destination directory.");
            return;
        }
        if (fileName.isEmpty())
            fileName = name + ".desktop";
        if (!fileName.endsWith(".desktop", Qt::CaseInsensitive))
            fileName += ".desktop";
        if (fileName.contains('/') || fileName.contains('\\')) {
            QMessageBox::warning(this, "Desktop Entry", "Filename must not contain path separators.");
            return;
        }
        m_fileNameEdit->setText(fileName);
        accept();
    });
    root->addWidget(buttons);
}

QString DesktopEntryDialog::entryName() const { return m_nameEdit->text().trimmed(); }
QString DesktopEntryDialog::executable() const { return m_execEdit->text().trimmed(); }
QString DesktopEntryDialog::icon() const { return m_iconEdit->text().trimmed(); }
QString DesktopEntryDialog::comment() const { return m_commentEdit->text().trimmed(); }
QString DesktopEntryDialog::path() const { return m_pathEdit->text().trimmed(); }
QString DesktopEntryDialog::categories() const { return m_categoriesEdit->text().trimmed(); }
QString DesktopEntryDialog::entryType() const { return m_typeCombo->currentText(); }
QString DesktopEntryDialog::destinationDirectory() const { return m_destinationEdit->text().trimmed(); }
QString DesktopEntryDialog::fileName() const { return m_fileNameEdit->text().trimmed(); }
bool DesktopEntryDialog::runInTerminal() const { return m_terminalCheck->isChecked(); }
