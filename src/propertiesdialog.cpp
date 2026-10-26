#include "propertiesdialog.h"
#include "desktopconstants.h"
#include "thememanager.h"
#include "soundmanager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSlider>
#include <QScrollArea>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextStream>
#include <QVBoxLayout>

namespace {
struct DesktopApplicationEntry {
    QString name;
    QString icon;
    QString desktopFilePath;
};

DesktopApplicationEntry readDesktopApplication(const QString &filePath)
{
    DesktopApplicationEntry entry;
    entry.desktopFilePath = filePath;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return entry;

    QTextStream in(&file);
    bool inDesktopEntry = false;
    bool hidden = false;
    bool noDisplay = false;
    QString type = "Application";

    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        if (line.startsWith('[')) {
            inDesktopEntry = (line == "[Desktop Entry]");
            continue;
        }
        if (!inDesktopEntry || !line.contains('='))
            continue;

        const int split = line.indexOf('=');
        const QString key = line.left(split).trimmed();
        const QString value = line.mid(split + 1).trimmed();

        if (key == "Name" && entry.name.isEmpty()) {
            entry.name = value;
        } else if (key == "Icon" && entry.icon.isEmpty()) {
            entry.icon = value;
        } else if (key == "NoDisplay") {
            noDisplay = (value.compare("true", Qt::CaseInsensitive) == 0);
        } else if (key == "Hidden") {
            hidden = (value.compare("true", Qt::CaseInsensitive) == 0);
        } else if (key == "Type") {
            type = value;
        }
    }

    if (hidden || noDisplay || type.compare("Application", Qt::CaseInsensitive) != 0)
        return {};

    if (entry.name.isEmpty())
        entry.name = QFileInfo(filePath).completeBaseName();

    return entry;
}
}

PropertiesDialog::PropertiesDialog(const Settings &s, QWidget *parent,
                                   bool openDesktopEntryTab)
    : QDialog(parent, Qt::Dialog)
{
    setWindowTitle("Sakura Desktop Icons Settings");
    setObjectName("PropertiesDialog");
    setMinimumSize(720, 480);
    resize(qMax(720, s.settingsWidth), qMax(480, s.settingsHeight));
    setSizeGripEnabled(false);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *banner = new QFrame(this);
    banner->setObjectName("PropBanner");
    banner->setFixedHeight(kPropertiesDialogBannerHeight);
    auto *bannerLayout = new QHBoxLayout(banner);
    bannerLayout->setContentsMargins(10, 6, 10, 6);
    bannerLayout->setSpacing(10);

    auto *bannerIcon = new QLabel(banner);
    bannerIcon->setPixmap(QIcon(":/icons/sakura.svg").pixmap(32, 32));

    auto *bannerText = new QLabel(QString::fromUtf8("🌸sakura deskto icon setting🌸"), banner);
    bannerText->setObjectName("PropBannerText");
    bannerText->setAlignment(Qt::AlignCenter);

    bannerLayout->addWidget(bannerIcon);
    bannerLayout->addWidget(bannerText, 1);
    bannerLayout->addSpacing(32);
    root->addWidget(banner);

    auto *content = new QWidget(this);
    content->setObjectName("PropContent");
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(kPropertiesDialogContentMargins, kPropertiesDialogContentMargins, kPropertiesDialogContentMargins, kPropertiesDialogContentMargins);
    contentLayout->setSpacing(8);

    auto *tabs = new QTabWidget(content);

    auto *generalTab = new QWidget(tabs);
    auto *generalLayout = new QVBoxLayout(generalTab);
    generalLayout->setContentsMargins(8, 8, 8, 8);
    generalLayout->setSpacing(8);

    auto *fileGroup = new QGroupBox("File Display", generalTab);
    auto *fileLayout = new QVBoxLayout(fileGroup);
    fileLayout->setSpacing(6);
    m_showHiddenCheck = new QCheckBox("Show hidden files", fileGroup);
    m_showHiddenCheck->setChecked(s.showHidden);
    fileLayout->addWidget(m_showHiddenCheck);
    generalLayout->addWidget(fileGroup);

    auto *integrationGroup = new QGroupBox("Desktop Integration", generalTab);
    auto *integrationLayout = new QVBoxLayout(integrationGroup);
    integrationLayout->setSpacing(6);
    m_trayEnabledCheck = new QCheckBox("Show System Tray icon", integrationGroup);
    m_trayEnabledCheck->setChecked(s.trayEnabled);
    integrationLayout->addWidget(m_trayEnabledCheck);
    generalLayout->addWidget(integrationGroup);

    auto *windowGroup = new QGroupBox("Settings Window", generalTab);
    auto *windowLayout = new QGridLayout(windowGroup);
    windowLayout->setColumnStretch(1, 1);
    windowLayout->addWidget(new QLabel("Width:"), 0, 0);
    m_settingsWidthSpin = new QSpinBox(windowGroup);
    m_settingsWidthSpin->setRange(720, 3840);
    m_settingsWidthSpin->setValue(qBound(720, s.settingsWidth, 3840));
    m_settingsWidthSpin->setSuffix(" px");
    windowLayout->addWidget(m_settingsWidthSpin, 0, 1);
    windowLayout->addWidget(new QLabel("Height:"), 1, 0);
    m_settingsHeightSpin = new QSpinBox(windowGroup);
    m_settingsHeightSpin->setRange(480, 2160);
    m_settingsHeightSpin->setValue(qBound(480, s.settingsHeight, 2160));
    m_settingsHeightSpin->setSuffix(" px");
    windowLayout->addWidget(m_settingsHeightSpin, 1, 1);
    generalLayout->addWidget(windowGroup);

    auto *desktopGroup = new QGroupBox("Desktop Icons", generalTab);
    auto *desktopLayout = new QVBoxLayout(desktopGroup);
    desktopLayout->setSpacing(6);
    m_showTrashCheck = new QCheckBox("Trash", desktopGroup);
    m_showTrashCheck->setChecked(s.showTrash);
    desktopLayout->addWidget(m_showTrashCheck);
    m_showComputerCheck = new QCheckBox("Root", desktopGroup);
    m_showComputerCheck->setChecked(s.showComputer);
    desktopLayout->addWidget(m_showComputerCheck);
    m_showHomeCheck = new QCheckBox("Home", desktopGroup);
    m_showHomeCheck->setChecked(s.showHome);
    desktopLayout->addWidget(m_showHomeCheck);
    generalLayout->addWidget(desktopGroup);

    auto *arrGroup = new QGroupBox("Arrangement", generalTab);
    auto *arrLayout = new QGridLayout(arrGroup);
    arrLayout->setColumnStretch(1, 1);
    arrLayout->setVerticalSpacing(8);
    m_autoArrangeCheck = new QCheckBox("Auto arrange icons", arrGroup);
    m_autoArrangeCheck->setChecked(s.autoArrange);
    arrLayout->addWidget(m_autoArrangeCheck, 0, 0, 1, 2);
    arrLayout->addWidget(new QLabel("Sort icons by:"), 1, 0);
    m_sortCombo = new QComboBox(arrGroup);
    m_sortCombo->addItems({"Name", "Type", "Date Modified"});
    m_sortCombo->setCurrentIndex(s.sortOrder);
    arrLayout->addWidget(m_sortCombo, 1, 1);
    generalLayout->addWidget(arrGroup);

    generalLayout->addStretch();

    auto *appearanceTab = new QWidget(tabs);
    auto *appearanceLayout = new QVBoxLayout(appearanceTab);
    appearanceLayout->setContentsMargins(8, 8, 8, 8);
    appearanceLayout->setSpacing(8);

    auto *themeGroup = new QGroupBox("Theme", appearanceTab);
    auto *themeLayout = new QGridLayout(themeGroup);
    themeLayout->setColumnStretch(1, 1);
    themeLayout->addWidget(new QLabel("Theme:"), 0, 0);
    m_themeCombo = new QComboBox(themeGroup);
    m_themeCombo->addItems(ThemeManager::availableThemes());
    const int themeIndex = m_themeCombo->findText(s.themeName, Qt::MatchFixedString);
    m_themeCombo->setCurrentIndex(themeIndex >= 0 ? themeIndex : 0);
    themeLayout->addWidget(m_themeCombo, 0, 1);
    themeLayout->addWidget(new QLabel("Custom theme JSON:"), 1, 0);
    auto *customThemeRow = new QWidget(themeGroup);
    auto *customThemeLayout = new QHBoxLayout(customThemeRow);
    customThemeLayout->setContentsMargins(0, 0, 0, 0);
    m_customThemePathEdit = new QLineEdit(s.customThemePath, customThemeRow);
    m_customThemePathEdit->setPlaceholderText("Select a JSON theme file");
    auto *customBrowse = new QPushButton("Choose Custom Theme File...", customThemeRow);
    customThemeLayout->addWidget(m_customThemePathEdit, 1);
    customThemeLayout->addWidget(customBrowse);
    themeLayout->addWidget(customThemeRow, 1, 1);
    connect(customBrowse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, "Select custom theme", QString(), "JSON files (*.json);;All files (*)");
        if (!path.isEmpty()) {
            SakuraTheme preview;
            QString error;
            if (!ThemeManager::loadCustomTheme(path, &preview, &error)) {
                QMessageBox::warning(this, "Invalid custom theme",
                    QString("The selected theme could not be loaded: %1").arg(error));
            } else {
                m_customThemePathEdit->setText(path);
            }
        }
    });
    m_useCustomThemeCheck = new QCheckBox("Use custom theme instead", themeGroup);
    m_useCustomThemeCheck->setChecked(s.useCustomTheme);
    themeLayout->addWidget(m_useCustomThemeCheck, 2, 0, 1, 2);
    connect(m_useCustomThemeCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        if (!enabled) return;
        SakuraTheme preview;
        QString error;
        if (m_customThemePathEdit->text().trimmed().isEmpty()
            || !ThemeManager::loadCustomTheme(m_customThemePathEdit->text().trimmed(), &preview, &error)) {
            m_useCustomThemeCheck->setChecked(false);
            QMessageBox::warning(this, "Custom theme unavailable",
                error.isEmpty() ? "Choose a valid custom theme file first." : error);
        }
    });
    themeLayout->addWidget(new QLabel("Context menu opacity:"), 3, 0);
    m_contextMenuOpacitySpin = new QSpinBox(themeGroup);
    m_contextMenuOpacitySpin->setRange(20, 100);
    m_contextMenuOpacitySpin->setValue(qBound(20, s.contextMenuOpacity, 100));
    m_contextMenuOpacitySpin->setSuffix(" %");
    themeLayout->addWidget(m_contextMenuOpacitySpin, 3, 1);
    themeLayout->addWidget(new QLabel("Settings opacity:"), 4, 0);
    m_settingsOpacitySpin = new QSpinBox(themeGroup);
    m_settingsOpacitySpin->setRange(20, 100);
    m_settingsOpacitySpin->setValue(qBound(20, s.settingsOpacity, 100));
    m_settingsOpacitySpin->setSuffix(" %");
    themeLayout->addWidget(m_settingsOpacitySpin, 4, 1);
    appearanceLayout->addWidget(themeGroup);

    auto *labelGroup = new QGroupBox("Icon Labels", appearanceTab);
    auto *labelLayout = new QGridLayout(labelGroup);
    labelLayout->setColumnStretch(1, 1);
    labelLayout->addWidget(new QLabel("Text color (hex, blank = theme):"), 0, 0);
    m_labelTextColorEdit = new QLineEdit(labelGroup);
    m_labelTextColorEdit->setText(s.labelTextColor);
    m_labelTextColorEdit->setPlaceholderText("#ffffff");
    labelLayout->addWidget(m_labelTextColorEdit, 0, 1);
    labelLayout->addWidget(new QLabel("Outline color:"), 1, 0);
    m_labelOutlineColorEdit = new QLineEdit(labelGroup);
    m_labelOutlineColorEdit->setText(s.labelOutlineColor);
    m_labelOutlineColorEdit->setPlaceholderText("#20151d");
    labelLayout->addWidget(m_labelOutlineColorEdit, 1, 1);
    labelLayout->addWidget(new QLabel("Outline width:"), 2, 0);
    m_labelOutlineWidthSpin = new QSpinBox(labelGroup);
    m_labelOutlineWidthSpin->setRange(0, 6);
    m_labelOutlineWidthSpin->setValue(qBound(0, s.labelOutlineWidth, 6));
    m_labelOutlineWidthSpin->setSuffix(" px");
    labelLayout->addWidget(m_labelOutlineWidthSpin, 2, 1);
    appearanceLayout->addWidget(labelGroup);

    auto *animationGroup = new QGroupBox("Animations", appearanceTab);
    auto *animationLayout = new QGridLayout(animationGroup);
    m_animationsEnabledCheck = new QCheckBox("Enable animations", animationGroup);
    m_animationsEnabledCheck->setChecked(s.animationsEnabled);
    animationLayout->addWidget(m_animationsEnabledCheck, 0, 0, 1, 2);
    m_animateAppearanceCheck = new QCheckBox("Icon appearance", animationGroup);
    m_animateAppearanceCheck->setChecked(s.animateAppearance);
    animationLayout->addWidget(m_animateAppearanceCheck, 1, 0);
    m_animateSelectionCheck = new QCheckBox("Selection feedback", animationGroup);
    m_animateSelectionCheck->setChecked(s.animateSelection);
    animationLayout->addWidget(m_animateSelectionCheck, 1, 1);
    animationLayout->addWidget(new QLabel("Speed:"), 2, 0);
    m_animationSpeedSpin = new QSpinBox(animationGroup);
    m_animationSpeedSpin->setRange(25, 300);
    m_animationSpeedSpin->setValue(qBound(25, s.animationSpeed, 300));
    m_animationSpeedSpin->setSuffix(" %");
    animationLayout->addWidget(m_animationSpeedSpin, 2, 1);
    animationLayout->addWidget(new QLabel("Intensity:"), 3, 0);
    m_animationIntensitySpin = new QSpinBox(animationGroup);
    m_animationIntensitySpin->setRange(25, 200);
    m_animationIntensitySpin->setValue(qBound(25, s.animationIntensity, 200));
    m_animationIntensitySpin->setSuffix(" %");
    animationLayout->addWidget(m_animationIntensitySpin, 3, 1);
    auto updateAnimationControls = [this](bool enabled) {
        m_animateAppearanceCheck->setEnabled(enabled);
        m_animateSelectionCheck->setEnabled(enabled);
        m_animationSpeedSpin->setEnabled(enabled);
        m_animationIntensitySpin->setEnabled(enabled);
    };
    connect(m_animationsEnabledCheck, &QCheckBox::toggled, this, updateAnimationControls);
    updateAnimationControls(m_animationsEnabledCheck->isChecked());
    appearanceLayout->addWidget(animationGroup);

    auto *iconGroup = new QGroupBox("Icon Appearance", appearanceTab);
    auto *iconLayout = new QGridLayout(iconGroup);
    iconLayout->setColumnStretch(1, 1);
    iconLayout->setVerticalSpacing(8);
    iconLayout->addWidget(new QLabel("Icon size:"), 0, 0);
    auto *sizeRow = new QHBoxLayout;
    sizeRow->setSpacing(6);
    m_sizeSlider = new QSlider(Qt::Horizontal, iconGroup);
    m_sizeSlider->setRange(kIconSizeSliderMin, kIconSizeSliderMax);
    m_sizeSlider->setSingleStep(kIconSizeSliderSingleStep);
    m_sizeSlider->setPageStep(kIconSizeSliderPageStep);
    m_sizeSlider->setValue(s.iconSize);
    m_sizeSlider->setTickPosition(QSlider::TicksBelow);
    m_sizeSlider->setTickInterval(kIconSizeSliderTickInterval);
    m_sizeLabel = new QLabel(QString::number(s.iconSize) + " px", iconGroup);
    m_sizeLabel->setFixedWidth(38);
    m_sizeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    connect(m_sizeSlider, &QSlider::valueChanged, this, [this](int v) {
        m_sizeLabel->setText(QString::number(v) + " px");
        emit iconSizeChanged(v);
    });
    sizeRow->addWidget(m_sizeSlider);
    sizeRow->addWidget(m_sizeLabel);
    iconLayout->addLayout(sizeRow, 0, 1);

    iconLayout->addWidget(new QLabel("Font:"), 1, 0);
    auto *fontRow = new QHBoxLayout;
    fontRow->setSpacing(6);
    m_fontFamilyCombo = new QComboBox(iconGroup);
    m_fontSizeSpin = new QSpinBox(iconGroup);
    m_fontSizeSpin->setRange(kFontSizeSpinMin, kFontSizeSpinMax);
    m_fontSizeSpin->setValue(s.fontSize);
    m_fontSizeSpin->setSuffix(" pt");
    m_fontSizeSpin->setFixedWidth(65);
    populateFontList();
    int fontIdx = m_fontFamilyCombo->findText(s.fontFamily, Qt::MatchFixedString);
    if (fontIdx >= 0)
        m_fontFamilyCombo->setCurrentIndex(fontIdx);
    connect(m_fontFamilyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        emit fontChanged(m_fontFamilyCombo->currentText());
    });
    connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        emit fontSizeChanged(v);
    });
    fontRow->addWidget(m_fontFamilyCombo, 1);
    fontRow->addWidget(m_fontSizeSpin);
    iconLayout->addLayout(fontRow, 1, 1);
    m_previewCheck = new QCheckBox("Show image previews", iconGroup);
    m_previewCheck->setChecked(s.showPreview);
    iconLayout->addWidget(m_previewCheck, 2, 0, 1, 2);
    appearanceLayout->addWidget(iconGroup);
    appearanceLayout->addStretch();

    auto *soundsTab = new QWidget(tabs);
    auto *soundsLayout = new QVBoxLayout(soundsTab);
    soundsLayout->setContentsMargins(8, 8, 8, 8);
    soundsLayout->setSpacing(8);
    auto *soundGroup = new QGroupBox("Sound Preferences", soundsTab);
    auto *soundLayout = new QVBoxLayout(soundGroup);
    soundLayout->setSpacing(6);
    m_soundsEnabledCheck = new QCheckBox("Enable application sounds", soundGroup);
    m_soundsEnabledCheck->setChecked(s.soundsEnabled);
    soundLayout->addWidget(m_soundsEnabledCheck);
    m_soundCreateCheck = new QCheckBox("Creating items", soundGroup);
    m_soundCreateCheck->setChecked(s.soundCreateEnabled);
    soundLayout->addWidget(m_soundCreateCheck);
    m_soundOpenCheck = new QCheckBox("Open item", soundGroup); m_soundOpenCheck->setChecked(s.soundOpenEnabled); soundLayout->addWidget(m_soundOpenCheck);
    m_soundRestartCheck = new QCheckBox("Restart", soundGroup); m_soundRestartCheck->setChecked(s.soundRestartEnabled); soundLayout->addWidget(m_soundRestartCheck);
    m_soundQuitCheck = new QCheckBox("Quit", soundGroup); m_soundQuitCheck->setChecked(s.soundQuitEnabled); soundLayout->addWidget(m_soundQuitCheck);
    m_soundDeleteCheck = new QCheckBox("Deleting items", soundGroup);
    m_soundDeleteCheck->setChecked(s.soundDeleteEnabled);
    soundLayout->addWidget(m_soundDeleteCheck);
    m_soundSettingsCheck = new QCheckBox("Opening settings", soundGroup);
    m_soundSettingsCheck->setChecked(s.soundSettingsEnabled);
    soundLayout->addWidget(m_soundSettingsCheck);
    auto updateSoundEventState = [this](bool enabled) {
        m_soundCreateCheck->setEnabled(enabled);
        m_soundDeleteCheck->setEnabled(enabled);
        m_soundSettingsCheck->setEnabled(enabled);
    };
    connect(m_soundsEnabledCheck, &QCheckBox::toggled, this, updateSoundEventState);
    updateSoundEventState(m_soundsEnabledCheck->isChecked());
    auto addSoundFileRow = [&](const QString &label, const QString &value, QLineEdit **target,
                               const QString &eventId) {
        auto *row = new QWidget(soundGroup);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->addWidget(new QLabel(label, row));
        auto *edit = new QLineEdit(row);
        edit->setText(value);
        edit->setPlaceholderText("System sound theme");
        rowLayout->addWidget(edit, 1);
        auto *browse = new QPushButton("Browse", row);
        auto *preview = new QPushButton("Preview", row);
        auto *reset = new QPushButton("Default", row);
        rowLayout->addWidget(browse);
        rowLayout->addWidget(preview);
        rowLayout->addWidget(reset);
        connect(browse, &QPushButton::clicked, this, [this, edit] {
            const QString path = QFileDialog::getOpenFileName(
                this, "Select sound file", QString(),
                "Audio files (*.wav *.ogg *.oga *.flac *.mp3);;All files (*)");
            if (!path.isEmpty())
                edit->setText(path);
        });
        connect(preview, &QPushButton::clicked, this, [edit, eventId] {
            SoundManager::playEvent(eventId, edit->text().trimmed());
        });
        connect(reset, &QPushButton::clicked, this, [edit] { edit->clear(); });
        soundLayout->addWidget(row);
        *target = edit;
    };
    addSoundFileRow("Create:", s.soundCreateFile, &m_soundCreateFileEdit, "complete");
    addSoundFileRow("Open:", s.soundOpenFile, &m_soundOpenFileEdit, "complete");
    addSoundFileRow("Restart:", s.soundRestartFile, &m_soundRestartFileEdit, "desktop-login");
    addSoundFileRow("Quit:", s.soundQuitFile, &m_soundQuitFileEdit, "desktop-logout");
    addSoundFileRow("Delete:", s.soundDeleteFile, &m_soundDeleteFileEdit, "trash-empty");
    addSoundFileRow("Settings:", s.soundSettingsFile, &m_soundSettingsFileEdit, "dialog-information");
    auto *soundNote = new QLabel("These preferences are stored with the application settings. Linux sound-theme playback can use them when sound integration is enabled.", soundsTab);
    soundNote->setWordWrap(true);
    soundsLayout->addWidget(soundGroup);
    soundsLayout->addWidget(soundNote);
    soundsLayout->addStretch();

    auto *customActionsTab = new QWidget(tabs);
    auto *customActionsLayout = new QVBoxLayout(customActionsTab);
    customActionsLayout->setContentsMargins(8, 8, 8, 8);
    customActionsLayout->setSpacing(8);
    auto *customActionsGroup = new QGroupBox("Right-click Custom Actions", customActionsTab);
    auto *customActionsLayoutInner = new QVBoxLayout(customActionsGroup);
    customActionsLayoutInner->setSpacing(8);
    for (int i = 0; i < 4; ++i) {
        auto *actionGroup = new QGroupBox(QString("Action %1").arg(i + 1), customActionsGroup);
        auto *actionLayout = new QGridLayout(actionGroup);
        actionLayout->setColumnStretch(1, 1);
        actionLayout->setColumnStretch(3, 2);
        actionLayout->addWidget(new QLabel("Name:"), 0, 0);
        m_customActionNameEdit[i] = new QLineEdit(actionGroup);
        m_customActionNameEdit[i]->setPlaceholderText("Menu label");
        m_customActionNameEdit[i]->setText(s.customActionNames.value(i));
        actionLayout->addWidget(m_customActionNameEdit[i], 0, 1);
        actionLayout->addWidget(new QLabel("Command:"), 0, 2);
        m_customActionCommandEdit[i] = new QLineEdit(actionGroup);
        m_customActionCommandEdit[i]->setPlaceholderText("Command");
        m_customActionCommandEdit[i]->setText(s.customActionCommands.value(i));
        actionLayout->addWidget(m_customActionCommandEdit[i], 0, 3);
        m_customActionSubmenuCheck[i] = new QCheckBox("Use submenu", actionGroup);
        m_customActionSubmenuCheck[i]->setChecked(s.customActionSubmenus[i]);
        actionLayout->addWidget(m_customActionSubmenuCheck[i], 1, 0, 1, 4);
        auto *subGroup = new QGroupBox("Submenu items", actionGroup);
        auto *subLayout = new QGridLayout(subGroup);
        subLayout->setColumnStretch(1, 1);
        subLayout->setColumnStretch(2, 2);
        subLayout->addWidget(new QLabel("Name"), 0, 1);
        subLayout->addWidget(new QLabel("Command"), 0, 2);
        for (int j = 0; j < 4; ++j) {
            subLayout->addWidget(new QLabel(QString("Item %1:").arg(j + 1)), j + 1, 0);
            m_customSubmenuNameEdit[i][j] = new QLineEdit(subGroup);
            m_customSubmenuNameEdit[i][j]->setPlaceholderText("Menu label");
            m_customSubmenuNameEdit[i][j]->setText(s.customSubmenuNames[i].value(j));
            subLayout->addWidget(m_customSubmenuNameEdit[i][j], j + 1, 1);
            m_customSubmenuCommandEdit[i][j] = new QLineEdit(subGroup);
            m_customSubmenuCommandEdit[i][j]->setPlaceholderText("Command");
            m_customSubmenuCommandEdit[i][j]->setText(s.customSubmenuCommands[i].value(j));
            subLayout->addWidget(m_customSubmenuCommandEdit[i][j], j + 1, 2);
        }
        actionLayout->addWidget(subGroup, 2, 0, 1, 4);
        customActionsLayoutInner->addWidget(actionGroup);
    }
    auto *customActionsNote = new QLabel(
        "A normal action needs a name and command. A submenu action needs valid name/command pairs in its submenu items; empty rows are hidden.",
        customActionsTab);
    customActionsNote->setWordWrap(true);
    auto *customActionsScroll = new QScrollArea(customActionsTab);
    customActionsScroll->setWidgetResizable(true);
    customActionsScroll->setFrameShape(QFrame::NoFrame);
    customActionsScroll->setWidget(customActionsGroup);
    customActionsLayout->addWidget(customActionsScroll, 1);
    customActionsLayout->addWidget(customActionsNote);
    customActionsLayout->addStretch();
    auto *desktopEntryTab = new QWidget(tabs);
    auto *desktopEntryLayout = new QVBoxLayout(desktopEntryTab);
    desktopEntryLayout->setContentsMargins(8, 8, 8, 8);
    desktopEntryLayout->setSpacing(8);
    auto *appGroup = new QGroupBox("Installed Applications", desktopEntryTab);
    auto *appLayout = new QVBoxLayout(appGroup);
    appLayout->setSpacing(6);
    auto *description = new QLabel(
        "Select an installed application to create a shortcut on the desktop.", appGroup);
    description->setWordWrap(true);
    appLayout->addWidget(description);
    m_appSearchEdit = new QLineEdit(appGroup);
    m_appSearchEdit->setPlaceholderText("Search applications");
    appLayout->addWidget(m_appSearchEdit);
    m_appList = new QListWidget(appGroup);
    m_appList->setAlternatingRowColors(false);
    m_appList->setUniformItemSizes(true);
    appLayout->addWidget(m_appList, 1);
    m_createShortcutButton = new QPushButton("Create Shortcut", appGroup);
    m_createShortcutButton->setEnabled(false);
    appLayout->addWidget(m_createShortcutButton, 0, Qt::AlignRight);
    m_editEntryButton = new QPushButton("Edit Selected Entry", appGroup);
    m_editEntryButton->setEnabled(false);
    appLayout->addWidget(m_editEntryButton, 0, Qt::AlignRight);
    connect(m_appList, &QListWidget::itemSelectionChanged, this, [this]() {
        m_createShortcutButton->setEnabled(m_appList->currentItem() != nullptr);
        m_editEntryButton->setEnabled(m_appList->currentItem() != nullptr);
    });
    connect(m_appSearchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        const QString needle = text.trimmed();
        for (int i = 0; i < m_appList->count(); ++i) {
            auto *item = m_appList->item(i);
            const bool matches = needle.isEmpty()
                || item->text().contains(needle, Qt::CaseInsensitive);
            item->setHidden(!matches);
        }
        if (m_appList->currentItem() && m_appList->currentItem()->isHidden())
            m_appList->setCurrentItem(nullptr);
    });
    connect(m_appList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (!item) return;
        emit applicationShortcutRequested(item->data(Qt::UserRole).toString());
    });
    connect(m_createShortcutButton, &QPushButton::clicked, this, [this]() {
        if (!m_appList->currentItem()) return;
        emit applicationShortcutRequested(m_appList->currentItem()->data(Qt::UserRole).toString());
    });
    connect(m_editEntryButton, &QPushButton::clicked, this, [this]() {
        if (!m_appList->currentItem()) return;
        emit desktopEntryEditRequested(m_appList->currentItem()->data(Qt::UserRole).toString());
    });
    desktopEntryLayout->addWidget(appGroup);
    desktopEntryLayout->addStretch();

    tabs->addTab(generalTab, "General");
    tabs->addTab(appearanceTab, "Appearance");
    tabs->addTab(soundsTab, "Sounds");
    tabs->addTab(customActionsTab, "Custom Actions");
    tabs->addTab(desktopEntryTab, "Desktop Entry");
    if (openDesktopEntryTab)
        tabs->setCurrentWidget(desktopEntryTab);
    contentLayout->addWidget(tabs);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, content);
    buttons->button(QDialogButtonBox::Ok)->setText("Submit");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    contentLayout->addWidget(buttons);

    root->addWidget(content);

    populateApplicationsList();
}

void PropertiesDialog::populateApplicationsList()
{
    m_appList->clear();

    QStringList appDirs;
    const QString dataHome = qEnvironmentVariableIsEmpty("XDG_DATA_HOME")
        ? QDir::homePath() + "/.local/share"
        : qEnvironmentVariable("XDG_DATA_HOME");
    appDirs << dataHome + "/applications";

    const QStringList dataDirs = qEnvironmentVariableIsEmpty("XDG_DATA_DIRS")
        ? QStringList{"/usr/local/share", "/usr/share"}
        : qEnvironmentVariable("XDG_DATA_DIRS").split(':', Qt::SkipEmptyParts);
    for (const QString &dir : dataDirs)
        appDirs << dir + "/applications";

    QSet<QString> seenDesktopFiles;
    QList<DesktopApplicationEntry> entries;

    for (const QString &dirPath : appDirs) {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;

        const QFileInfoList files = dir.entryInfoList({"*.desktop"}, QDir::Files, QDir::Name);
        for (const QFileInfo &fileInfo : files) {
            if (seenDesktopFiles.contains(fileInfo.fileName()))
                continue;

            const DesktopApplicationEntry entry = readDesktopApplication(fileInfo.absoluteFilePath());
            if (entry.name.isEmpty())
                continue;

            seenDesktopFiles.insert(fileInfo.fileName());
            entries.append(entry);
        }
    }

    std::sort(entries.begin(), entries.end(), [](const DesktopApplicationEntry &a,
                                                 const DesktopApplicationEntry &b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });

    for (const DesktopApplicationEntry &entry : entries) {
        auto *item = new QListWidgetItem(entry.name, m_appList);
        item->setData(Qt::UserRole, entry.desktopFilePath);

        if (!entry.icon.isEmpty()) {
            QIcon icon = QIcon::fromTheme(entry.icon);
            if (icon.isNull()) {
                QFileInfo iconFile(entry.icon);
                if (iconFile.exists())
                    icon = QIcon(iconFile.absoluteFilePath());
            }
            if (!icon.isNull())
                item->setIcon(icon);
        }
    }
}

void PropertiesDialog::populateFontList()
{
    m_fontFamilyCombo->clear();
    QFontDatabase db;
    QStringList fonts = db.families();
    fonts.sort(Qt::CaseInsensitive);
    for (const QString &family : fonts)
        m_fontFamilyCombo->addItem(family);
}

PropertiesDialog::Settings PropertiesDialog::currentSettings() const
{
    Settings s;
    s.iconSize     = m_sizeSlider->value();
    s.showPreview  = m_previewCheck->isChecked();
    s.autoArrange  = m_autoArrangeCheck->isChecked();
    s.showHidden   = m_showHiddenCheck->isChecked();
    s.sortOrder    = m_sortCombo->currentIndex();
    s.showTrash    = m_showTrashCheck->isChecked();
    s.showComputer = m_showComputerCheck->isChecked();
    s.showHome     = m_showHomeCheck->isChecked();
    s.trayEnabled  = m_trayEnabledCheck->isChecked();
    s.fontFamily   = m_fontFamilyCombo->currentText();
    s.fontSize     = m_fontSizeSpin->value();
    s.themeName = m_themeCombo->currentText();
    s.customThemePath = m_customThemePathEdit->text().trimmed();
    s.useCustomTheme = m_useCustomThemeCheck->isChecked();
    s.settingsWidth = m_settingsWidthSpin->value();
    s.settingsHeight = m_settingsHeightSpin->value();
    s.contextMenuOpacity = m_contextMenuOpacitySpin->value();
    s.settingsOpacity = m_settingsOpacitySpin->value();
    s.labelTextColor = m_labelTextColorEdit->text().trimmed();
    s.labelOutlineColor = m_labelOutlineColorEdit->text().trimmed();
    s.labelOutlineWidth = m_labelOutlineWidthSpin->value();
    s.animationsEnabled = m_animationsEnabledCheck->isChecked();
    s.animateAppearance = m_animateAppearanceCheck->isChecked();
    s.animateSelection = m_animateSelectionCheck->isChecked();
    s.animationSpeed = m_animationSpeedSpin->value();
    s.animationIntensity = m_animationIntensitySpin->value();
    s.soundsEnabled       = m_soundsEnabledCheck->isChecked();
    s.soundCreateEnabled  = m_soundCreateCheck->isChecked();
    s.soundOpenEnabled = m_soundOpenCheck->isChecked();
    s.soundRestartEnabled = m_soundRestartCheck->isChecked();
    s.soundQuitEnabled = m_soundQuitCheck->isChecked();
    s.soundDeleteEnabled  = m_soundDeleteCheck->isChecked();
    s.soundSettingsEnabled = m_soundSettingsCheck->isChecked();
    s.soundCreateFile = m_soundCreateFileEdit->text().trimmed();
    s.soundOpenFile = m_soundOpenFileEdit->text().trimmed();
    s.soundRestartFile = m_soundRestartFileEdit->text().trimmed();
    s.soundQuitFile = m_soundQuitFileEdit->text().trimmed();
    s.soundDeleteFile = m_soundDeleteFileEdit->text().trimmed();
    s.soundSettingsFile = m_soundSettingsFileEdit->text().trimmed();
    for (int i = 0; i < 4; ++i) {
        s.customActionNames << m_customActionNameEdit[i]->text().trimmed();
        s.customActionCommands << m_customActionCommandEdit[i]->text().trimmed();
        s.customActionSubmenus[i] = m_customActionSubmenuCheck[i]->isChecked();
        for (int j = 0; j < 4; ++j) {
            s.customSubmenuNames[i] << m_customSubmenuNameEdit[i][j]->text().trimmed();
            s.customSubmenuCommands[i] << m_customSubmenuCommandEdit[i][j]->text().trimmed();
        }
    }
    return s;
}
