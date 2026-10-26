#pragma once

#include <QDialog>
#include <QFont>

class QSlider;
class QLabel;
class QCheckBox;
class QComboBox;
class QListWidget;
class QPushButton;
class QLineEdit;
class QSpinBox;

class PropertiesDialog : public QDialog
{
    Q_OBJECT
public:
    struct Settings {
        int  iconSize    = 40;
        bool showPreview = true;
        bool autoArrange = false;
        bool showHidden  = false;
        int  sortOrder   = 0;   // 0=Name, 1=Type, 2=Date
        bool showTrash   = true;
        bool showComputer= false;
        bool showHome    = false;
        bool trayEnabled = true;
        QString fontFamily = "MS Sans Serif";
        int  fontSize    = 8;
        QString themeName = "Sakura";
        QString customThemePath;
        bool useCustomTheme = false;
        QStringList customActionNames;
        QStringList customActionCommands;
        bool customActionSubmenus[4] = {false, false, false, false};
        QStringList customSubmenuNames[4];
        QStringList customSubmenuCommands[4];
        int settingsWidth = 1280;
        int settingsHeight = 720;
        int contextMenuOpacity = 94;
        int settingsOpacity = 94;
        QString labelTextColor;
        QString labelOutlineColor;
        int labelOutlineWidth = 1;
        bool animationsEnabled = true;
        bool animateAppearance = true;
        bool animateSelection = true;
        int animationSpeed = 100;
        int animationIntensity = 100;
        bool soundsEnabled = true;
        bool soundCreateEnabled = true;
        bool soundOpenEnabled = true;
        bool soundRestartEnabled = true;
        bool soundQuitEnabled = true;
        bool soundDeleteEnabled = true;
        bool soundSettingsEnabled = true;
        QString soundCreateFile;
        QString soundOpenFile;
        QString soundRestartFile;
        QString soundQuitFile;
        QString soundDeleteFile;
        QString soundSettingsFile;
    };

    explicit PropertiesDialog(const Settings &s, QWidget *parent = nullptr,
                               bool openDesktopEntryTab = false);
    Settings currentSettings() const;

signals:
    void iconSizeChanged(int size);
    void fontChanged(const QString &family);
    void fontSizeChanged(int size);
    void applied();
    void applicationShortcutRequested(const QString &desktopFilePath);
    void desktopEntryEditRequested(const QString &desktopFilePath);

private:
    void populateApplicationsList();
    void populateFontList();

    QSlider   *m_sizeSlider;
    QLabel    *m_sizeLabel;
    QComboBox *m_fontFamilyCombo;
    QSpinBox  *m_fontSizeSpin;
    QCheckBox *m_previewCheck;
    QCheckBox *m_autoArrangeCheck;
    QCheckBox *m_showHiddenCheck;
    QCheckBox *m_showTrashCheck;
    QCheckBox *m_showComputerCheck;
    QCheckBox *m_showHomeCheck;
    QCheckBox *m_trayEnabledCheck;
    QComboBox *m_sortCombo;
    QComboBox *m_themeCombo;
    QLineEdit *m_customThemePathEdit;
    QCheckBox *m_useCustomThemeCheck;
    QLineEdit *m_customActionNameEdit[4];
    QLineEdit *m_customActionCommandEdit[4];
    QCheckBox *m_customActionSubmenuCheck[4];
    QLineEdit *m_customSubmenuNameEdit[4][4];
    QLineEdit *m_customSubmenuCommandEdit[4][4];
    QSpinBox  *m_settingsWidthSpin;
    QSpinBox  *m_settingsHeightSpin;
    QSpinBox  *m_contextMenuOpacitySpin;
    QSpinBox  *m_settingsOpacitySpin;
    QLineEdit *m_labelTextColorEdit;
    QLineEdit *m_labelOutlineColorEdit;
    QSpinBox  *m_labelOutlineWidthSpin;
    QCheckBox *m_animationsEnabledCheck;
    QCheckBox *m_animateAppearanceCheck;
    QCheckBox *m_animateSelectionCheck;
    QSpinBox  *m_animationSpeedSpin;
    QSpinBox  *m_animationIntensitySpin;
    QCheckBox *m_soundsEnabledCheck;
    QCheckBox *m_soundCreateCheck;
    QCheckBox *m_soundOpenCheck;
    QCheckBox *m_soundRestartCheck;
    QCheckBox *m_soundQuitCheck;
    QCheckBox *m_soundDeleteCheck;
    QCheckBox *m_soundSettingsCheck;
    QLineEdit *m_soundCreateFileEdit;
    QLineEdit *m_soundOpenFileEdit;
    QLineEdit *m_soundRestartFileEdit;
    QLineEdit *m_soundQuitFileEdit;
    QLineEdit *m_soundDeleteFileEdit;
    QLineEdit *m_soundSettingsFileEdit;
    QLineEdit *m_appSearchEdit;
    QListWidget *m_appList;
    QPushButton *m_createShortcutButton;
    QPushButton *m_editEntryButton;
};
