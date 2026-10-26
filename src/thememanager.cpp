#include "thememanager.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSettings>
#include <QTextStream>

namespace {
QColor parseColor(QString value, const QColor &fallback)
{
    value = value.trimmed();
    if (value.startsWith("#")) {
        QColor c(value);
        if (c.isValid()) return c;
    }
    if (value.startsWith("0x", Qt::CaseInsensitive)) value = "#" + value.mid(2);
    QColor c(value);
    if (c.isValid()) return c;
    QRegularExpression hex(R"((?:rgba?\()?\s*(?:0x|#)?([0-9a-fA-F]{6,8}))");
    auto m = hex.match(value);
    if (m.hasMatch()) {
        c = QColor("#" + m.captured(1).right(6));
        if (c.isValid()) return c;
    }
    return fallback;
}

QColor firstColor(QString value, const QColor &fallback)
{
    value.remove("rgb:");
    auto matches = QRegularExpression(R"((?:#|0x)?[0-9a-fA-F]{6,8})").globalMatch(value);
    if (matches.hasNext()) return parseColor(matches.next().captured(0), fallback);
    return parseColor(value, fallback);
}

void parseHyprFile(const QString &path, QColor *active, QColor *inactive)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        auto match = QRegularExpression(R"(^\s*(col\.active_border|col\.inactive_border)\s*=\s*(.+?)(?:\s+#.*)?$)").match(line);
        if (!match.hasMatch()) continue;
        QColor parsed = firstColor(match.captured(2), QColor());
        if (!parsed.isValid()) continue;
        if (match.captured(1) == "col.active_border") *active = parsed;
        else *inactive = parsed;
    }
}

void parseNoctaliaFile(const QString &path, SakuraTheme *t)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    const QString source = QString::fromUtf8(file.readAll());
    // Narrow parser for Noctalia's generated local declarations. Never execute Lua.
    QRegularExpression re("\\b(primary|surface|on_surface|secondary|on_secondary|error|on_error)\\s*=\\s*\"rgb\\(([0-9a-fA-F]{6,8})\\)\"");
    auto it = re.globalMatch(source);
    QHash<QString, QColor> colors;
    while (it.hasNext()) {
        const auto m = it.next();
        QColor c("#" + m.captured(2).right(6));
        if (c.isValid()) colors.insert(m.captured(1), c);
    }
    const auto get = [&](const QString &key, const QColor &fallback) { return colors.value(key, fallback); };
    t->accent = get("primary", t->accent);
    t->windowBackground = get("surface", t->windowBackground);
    t->windowText = get("on_surface", t->windowText);
    t->menuBackground = t->windowBackground.lighter(125);
    t->menuText = t->windowText;
    t->border = get("secondary", t->border);
    t->highlight = get("secondary", t->highlight);
    t->highlightedText = get("on_secondary", t->highlightedText);
    t->separator = t->border.darker(125);
    t->disabledText = t->windowText.darker(145);
    t->labelText = t->windowText;
}

QString noctaliaPath()
{
    const QStringList candidates = {"/home/cherry/.config/hypr/noctlia.lua",
        QDir::homePath() + "/.config/hypr/noctlia.lua",
        QDir::homePath() + "/.config/hypr/noctalia.lua"};
    for (const QString &path : candidates) if (QFileInfo::exists(path)) return path;
    return {};
}

QColor jsonColor(const QJsonObject &o, const char *key, const QColor &fallback)
{ return parseColor(o.value(QLatin1String(key)).toString(), fallback); }
}

QStringList ThemeManager::customThemePaths()
{
    const QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/themes";
    QDir dir(dirPath);
    if (!dir.exists()) return {};
    QStringList result;
    for (const QString &file : dir.entryList({"*.json"}, QDir::Files, QDir::Name)) result << dir.filePath(file);
    return result;
}

QStringList ThemeManager::availableThemes()
{ return {"Sakura", "Noctalia", "Normal Dark", "Joyfull Sea", "Emerald Green"}; }

SakuraTheme ThemeManager::fromNoctalia()
{
    SakuraTheme t; t.name = "Noctalia";
    t.windowBackground = QColor("#1e1e2e"); t.menuBackground = QColor("#313244");
    t.windowText = QColor("#cdd6f4"); t.menuText = t.windowText;
    t.accent = QColor("#cba6f7"); t.highlight = QColor("#45475a"); t.border = QColor("#6c7086");
    const QString path = noctaliaPath();
    if (!path.isEmpty()) parseNoctaliaFile(path, &t);
    t.separator = t.border.darker(125); t.labelText = t.windowText;
    return t;
}

SakuraTheme ThemeManager::theme(const QString &rawName)
{
    QString name = rawName;
    if (name == "Default Dark") name = "Normal Dark";
    else if (name == "Default Light") name = "Normal Dark";
    else if (name == "Aero") name = "Joyfull Sea";
    else if (name == "Glass") name = "Emerald Green";
    if (name == "Sakura") {
        SakuraTheme t; t.name = name; t.windowBackground="#171317"; t.menuBackground="#2B202B";
        t.windowText="#F7EAF2"; t.menuText="#F7EAF2"; t.accent="#F2A2CD"; t.highlight="#493343";
        t.border="#B96F9D"; t.separator="#493343"; t.labelText="#F7EAF2"; t.labelOutline="#171317";
        t.disabledText="#8f7285"; return t;
    }
    if (name == "Noctalia") return fromNoctalia();
    if (name == "Joyfull Sea") {
        SakuraTheme t; t.name = name; t.windowBackground="#E2F5FF"; t.menuBackground="#F3FBFF";
        t.windowText="#173D55"; t.menuText="#173D55"; t.accent="#218CCB"; t.highlight="#B8E8FA";
        t.highlightedText="#FFFFFF"; t.border="#8CCFEF"; t.separator="#C5EAFE";
        t.labelText="#15394B"; t.labelOutline="#F1FCFF"; t.disabledText="#7194A3"; return t;
    }
    if (name == "Emerald Green") {
        SakuraTheme t; t.name = name; t.windowBackground="#101713"; t.menuBackground="#26392D";
        t.windowText="#E8F5EC"; t.menuText="#E8F5EC"; t.accent="#087F5B"; t.highlight="#2D4635";
        t.highlightedText="#172033"; t.border="#315640"; t.separator="#1D2B22";
        t.labelText="#F2F5FF"; t.labelOutline="#171923"; t.disabledText="#8290AA"; return t;
    }
    if (name == "Custom") {
        QSettings cfg(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/icon-positions.ini", QSettings::IniFormat);
        SakuraTheme custom;
        if (loadCustomTheme(cfg.value("appearance/customThemePath").toString(), &custom)) return custom;
    }
    SakuraTheme t; t.name = name;
    if (name == "Normal Dark") {
        t.windowBackground="#242424"; t.menuBackground="#404040"; t.windowText="#FFFFFF"; t.menuText="#FFFFFF";
        t.accent="#3584E4"; t.highlight="#454545"; t.border="#505050"; t.separator="#383838";
        t.labelText="#FFFFFF"; t.labelOutline="#242424"; t.disabledText="#858585";
    }
    return t;
}

bool ThemeManager::loadCustomTheme(const QString &path, SakuraTheme *result, QString *error)
{
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) { if (error) *error="Could not open theme file"; return false; }
    QJsonParseError pe; const QJsonDocument doc=QJsonDocument::fromJson(file.readAll(), &pe);
    if (!doc.isObject()) { if (error) *error=pe.errorString(); return false; }
    const QJsonObject o=doc.object(); SakuraTheme t; t.name=o.value("name").toString();
    if (t.name.isEmpty()) { if(error)*error="Theme name is missing"; return false; }
    t.windowBackground=jsonColor(o,"windowBackground",t.windowBackground); t.windowText=jsonColor(o,"windowText",t.windowText);
    t.accent=jsonColor(o,"accent",t.accent); t.highlight=jsonColor(o,"highlight",t.highlight); t.menuBackground=jsonColor(o,"menuBackground",t.menuBackground);
    t.menuText=jsonColor(o,"menuText",t.menuText); t.border=jsonColor(o,"border",t.border); *result=t; return true;
}

QString ThemeManager::styleSheet(const SakuraTheme &t)
{
    const QString field=t.windowBackground.lighter(108).name();
    return QString("QWidget{background:%2;color:%1;} QWidget#DesktopWindow,QWidget#DesktopScene{background:transparent;} QDialog,QMainWindow,QTabWidget,QTabWidget::pane,QStackedWidget,QScrollArea,QScrollArea>QWidget,QFrame#PropContent{background:%2;color:%1;} QGroupBox{background:%8;border:1px solid %7;border-radius:6px;margin-top:8px;padding-top:8px;} QGroupBox::title{subcontrol-origin:padding;subcontrol-position:top left;top:1px;left:8px;padding:0 4px;background:%8;} QTabWidget::tab-bar{alignment:center;} QTabBar{background:transparent;} QPushButton,QComboBox,QSpinBox,QLineEdit,QListWidget{background:%8;color:%1;border:1px solid %7;border-radius:4px;padding:4px 7px;} QPushButton:hover,QComboBox:hover,QSpinBox:hover,QLineEdit:focus{border:1px solid %3;} QPushButton:pressed,QPushButton:checked,QTabBar::tab:selected{background:%3;color:%4;} QPushButton:disabled,QComboBox:disabled,QSpinBox:disabled,QLineEdit:disabled{background:%2;color:%9;border-color:%7;} QTabBar::tab{background:%8;color:%1;border:1px solid %7;padding:6px 12px;} QTabBar::tab:hover{background:%3;color:%4;} QCheckBox{background:transparent;} QCheckBox::indicator{width:15px;height:15px;border:1px solid %7;border-radius:3px;background:%8;} QCheckBox::indicator:hover{border:2px solid %3;} QCheckBox::indicator:checked{background:%3;border:1px solid %7;} QCheckBox::indicator:disabled{background:%2;border-color:%9;} QComboBox::drop-down{background:%8;border-left:1px solid %7;} QComboBox QAbstractItemView{background:%5;color:%6;border:1px solid %7;selection-background-color:%3;selection-color:%4;} QAbstractItemView{background:%5;color:%6;selection-background-color:%3;selection-color:%4;} QMenu{background:%5;color:%6;border:1px solid %7;} QMenu::item:selected{background:%3;color:%4;} QToolTip{background:%5;color:%6;border:1px solid %7;} QScrollBar{background:%2;} QScrollBar::handle{background:%7;border-radius:4px;}").arg(t.windowText.name(),t.windowBackground.name(),t.accent.name(),t.highlightedText.name(),t.menuBackground.name(),t.menuText.name(),t.border.name(),field,t.disabledText.name());
}

void ThemeManager::apply(QApplication *app, const QString &name)
{ if (app) app->setStyleSheet(styleSheet(theme(availableThemes().contains(name) || name == "Custom" ? name : "Sakura"))); }
