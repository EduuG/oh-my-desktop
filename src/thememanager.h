#pragma once

#include <QColor>
#include <QString>
#include <QStringList>

class QApplication;

struct SakuraTheme
{
    QString name;
    QColor windowBackground = QColor("#202124");
    QColor windowText = QColor("#f1f3f4");
    QColor accent = QColor("#8ab4f8");
    QColor highlight = QColor("#3c4043");
    QColor highlightedText = QColor("#ffffff");
    QColor labelText = QColor("#ffffff");
    QColor labelOutline = QColor("#161616");
    QColor menuBackground = QColor("#292a2d");
    QColor menuText = QColor("#f1f3f4");
    QColor border = QColor("#5f6368");
    QColor separator = QColor("#4a4d51");
    QColor disabledText = QColor("#80868b");
    int menuOpacity = 94;
};

class ThemeManager
{
public:
    static QStringList availableThemes();
    static SakuraTheme theme(const QString &name);
    static SakuraTheme fromNoctalia();
    static bool loadCustomTheme(const QString &path, SakuraTheme *result, QString *error = nullptr);
    static QString styleSheet(const SakuraTheme &theme);
    static void apply(QApplication *application, const QString &name);

private:
    static QStringList customThemePaths();
};
