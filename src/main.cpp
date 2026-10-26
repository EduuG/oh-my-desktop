#include "desktopwindow.h"
#include "desktopscene.h"
#include "thememanager.h"
#include "soundmanager.h"

#include <QApplication>
#include <QAction>
#include <QCoreApplication>
#include <QFile>
#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QScreen>
#include <QSettings>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QHash>
#include <QStandardPaths>
#include <QFileInfo>
#include <QLabel>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("SakuraDesktopIcons");
    app.setOrganizationName("SakuraDesktopIcons");
    app.setApplicationVersion(QStringLiteral("1.1"));
    app.setQuitOnLastWindowClosed(false);
    const QIcon applicationIcon(":/icons/sakura.svg");
    app.setWindowIcon(applicationIcon);

    // Load retro stylesheet
    QFile qssFile(":/styles/styles/retro.qss");
    if (qssFile.open(QFile::ReadOnly)) {
        app.setStyleSheet(QString::fromUtf8(qssFile.readAll()));
        qssFile.close();
    }
    const QString initialConfigDir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QSettings initialConfig(initialConfigDir + "/icon-positions.ini", QSettings::IniFormat);
    ThemeManager::apply(&app, initialConfig.value("appearance/useCustomTheme", false).toBool() ? "Custom" : initialConfig.value("appearance/theme", "Sakura").toString());

    const QIcon trayIcon = applicationIcon.isNull()
        ? app.style()->standardIcon(QStyle::SP_ComputerIcon) : applicationIcon;
    QSystemTrayIcon tray(trayIcon, &app);
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    bool restartRequested = false;
    bool quitSoundPlayed = false;
    auto soundSetting = [&](const char *key, const char *fileKey) {
        QSettings cfg(configDir + "/icon-positions.ini", QSettings::IniFormat);
        if (!cfg.value("sounds/enabled", true).toBool() || !cfg.value(key, true).toBool()) return false;
        return SoundManager::playEvent(key, cfg.value(fileKey).toString());
    };
    QMenu trayMenu;
    QAction *aboutAction = trayMenu.addAction("About");
    QAction *settingsAction = trayMenu.addAction("Settings");
    QAction *restartAction = trayMenu.addAction("Restart");
    QAction *quitAction = trayMenu.addAction("Quit SakuraDesktopIcons");
    trayMenu.addSeparator();
    QAction *trayEnabledAction = trayMenu.addAction("Enable System Tray");
    trayEnabledAction->setCheckable(true);
    tray.setContextMenu(&trayMenu);

    // Create one desktop window per screen
    QHash<QScreen *, DesktopWindow *> windows;
    auto updateTrayVisibility = [&](bool enabled) {
        trayEnabledAction->setChecked(enabled);
        tray.setVisible(enabled && QSystemTrayIcon::isSystemTrayAvailable());
    };

    QObject::connect(trayEnabledAction, &QAction::toggled, [&](bool enabled) {
        QSettings cfg(configDir + "/icon-positions.ini", QSettings::IniFormat);
        cfg.setValue("ui/trayEnabled", enabled);
        cfg.sync();
        updateTrayVisibility(enabled);
    });

    QObject::connect(aboutAction, &QAction::triggered, [&] {
        QWidget *parent = windows.isEmpty() ? nullptr : windows.constBegin().value();
        QMessageBox about(parent);
        about.setWindowTitle("About SakuraDesktopIcons");
        about.setWindowIcon(applicationIcon);
        about.setIconPixmap(applicationIcon.pixmap(64, 64));
        about.setTextFormat(Qt::RichText);
        about.setText(QStringLiteral(
            "<h3>SakuraDesktopIcons %1</h3>"
            "<p>A lightweight Wayland desktop icon manager for Qt6 and Hyprland.</p>"
            "<p><a href=\"https://github.com/cherryCTL/SakuraDesktopIcons\">"
            "GitHub: cherryCTL/SakuraDesktopIcons</a></p>").arg(QCoreApplication::applicationVersion()));
        about.setStandardButtons(QMessageBox::Ok);
        for (auto *label : about.findChildren<QLabel *>()) {
            if (label->text().contains("github.com", Qt::CaseInsensitive)) {
                label->setOpenExternalLinks(true);
                label->setTextInteractionFlags(Qt::TextBrowserInteraction);
            }
        }
        about.exec();
    });
    QObject::connect(settingsAction, &QAction::triggered, [&] {
        if (!windows.isEmpty())
            windows.constBegin().value()->showSettings();
    });
    QObject::connect(quitAction, &QAction::triggered, [&] {
        soundSetting("sounds/quit", "sounds/quitFile");
        quitSoundPlayed = true;
        app.quit();
    });
    QObject::connect(restartAction, &QAction::triggered, [&] {
        const QString program = QCoreApplication::applicationFilePath();
        const QStringList arguments = QCoreApplication::arguments().mid(1);
        if (!QProcess::startDetached(program, arguments)) {
            QWidget *parent = windows.isEmpty() ? nullptr : windows.constBegin().value();
            QMessageBox::warning(parent, "Restart", "SakuraDesktopIcons could not be restarted.");
            return;
        }
        restartRequested = true;
        soundSetting("sounds/restart", "sounds/restartFile");
        app.quit();
    });

    QObject::connect(&app, &QApplication::aboutToQuit, [&] {
        if (!restartRequested && !quitSoundPlayed)
            soundSetting("sounds/quit", "sounds/quitFile");
    });

    auto attachWindow = [&](DesktopWindow *window) {
        QObject::connect(window->scene(), &DesktopScene::trayEnabledChanged,
                         &app, updateTrayVisibility);
    };

    for (QScreen *screen : app.screens()) {
        auto *window = new DesktopWindow(screen);
        windows.insert(screen, window);
        attachWindow(window);
    }

    // DesktopScene performs legacy-config migration during construction, so
    // read the tray preference only after the first scene has initialized.
    QSettings config(configDir + "/icon-positions.ini", QSettings::IniFormat);
    updateTrayVisibility(config.value("ui/trayEnabled", true).toBool());
    // The scene constructor performs legacy-config migration. Apply the theme
    // again now so a migrated user's theme is honored on the first launch.
    ThemeManager::apply(&app, config.value("appearance/useCustomTheme", false).toBool() ? "Custom" : config.value("appearance/theme", "Sakura").toString());

    // Handle screens added at runtime
    QObject::connect(&app, &QApplication::screenAdded, [&](QScreen *screen) {
        auto *window = new DesktopWindow(screen);
        windows.insert(screen, window);
        attachWindow(window);
    });

    // Handle screen removal - hot-unplug cleanup
    QObject::connect(&app, &QApplication::screenRemoved, [&](QScreen *screen) {
        if (auto *w = windows.take(screen)) {
            w->close();
            w->deleteLater();
        }
    });

    return app.exec();
}
