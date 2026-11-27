#include "launcherutil.h"
#include "desktopconstants.h"

#include <QProcess>
#include <QStandardPaths>
#include <QDebug>

namespace LauncherUtil {
namespace {

constexpr int kWindowedLaunchSize = 720;

QString shellQuote(const QString &value)
{
    QString escaped = value;
    escaped.replace('\'', "'\"'\"'");
    return '\'' + escaped + '\'';
}

QString joinCommand(const QString &program, const QStringList &arguments)
{
    QStringList parts;
    parts << shellQuote(program);
    for (const QString &argument : arguments)
        parts << shellQuote(argument);
    return parts.join(' ');
}

bool canUseHyprlandFloatDispatch()
{
    if (qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE"))
        return false;

    static const bool hasHyprctl = !QStandardPaths::findExecutable("hyprctl").isEmpty();
    return hasHyprctl;
}

// Runs `hyprctl dispatch ...` and waits for the reply. hyprctl answers "ok"
// only when the dispatcher actually ran, so a syntax or dispatcher error is
// reported as a failure instead of a successful process start.
bool runHyprctlDispatch(const QStringList &arguments)
{
    QProcess hyprctl;
    hyprctl.start("hyprctl", QStringList{"dispatch"} + arguments);
    if (!hyprctl.waitForFinished(kHyprctlDispatchTimeoutMs)) {
        hyprctl.kill();
        hyprctl.waitForFinished();
        return false;
    }
    return hyprctl.exitStatus() == QProcess::NormalExit
        && hyprctl.exitCode() == 0
        && hyprctl.readAllStandardOutput().trimmed() == "ok";
}

// Lua long bracket string, with enough '=' to never be closed early by the
// command itself.
QString luaLongString(const QString &value)
{
    QString level;
    while (value.contains(']' + level + ']') || value.endsWith(']' + level))
        level += '=';
    return '[' + level + '[' + value + ']' + level + ']';
}

bool dispatchLegacyExec(const QString &command)
{
    const QString rules = QString("[float; size %1 %1; center 1] ")
                              .arg(kWindowedLaunchSize);
    return runHyprctlDispatch({"exec", rules + command});
}

// With a Lua config (hyprland.lua), Hyprland evaluates the dispatch argument
// as `hl.dispatch(<argument>)`, so the hyprlang form above is a Lua syntax
// error there.
bool dispatchLuaExec(const QString &command)
{
    const QString expr = QString("hl.dsp.exec_cmd(%1, { float = true, size = { %2, %2 }, center = true })")
                             .arg(luaLongString(command))
                             .arg(kWindowedLaunchSize);
    return runHyprctlDispatch({expr});
}

bool startWithHyprlandFloatRule(const QString &command)
{
    // Try the syntax that worked last time first; the config can be switched
    // between hyprland.conf and hyprland.lua without restarting us.
    static bool preferLua = false;

    if (preferLua ? dispatchLuaExec(command) : dispatchLegacyExec(command))
        return true;
    if (preferLua ? dispatchLegacyExec(command) : dispatchLuaExec(command)) {
        preferLua = !preferLua;
        return true;
    }

    qWarning() << "[IPC] hyprctl dispatch exec failed, launching without float rules:"
               << command.left(80);
    return false;
}

} // namespace

bool startDetachedWindowedCommand(const QString &command)
{
    const QString trimmed = command.trimmed();
    if (trimmed.isEmpty())
        return false;

    if (canUseHyprlandFloatDispatch() && startWithHyprlandFloatRule(trimmed))
        return true;

    return QProcess::startDetached("sh", {"-lc", trimmed});
}

bool startDetachedWindowedProgram(const QString &program, const QStringList &arguments)
{
    if (program.trimmed().isEmpty())
        return false;

    if (canUseHyprlandFloatDispatch()) {
        const QString command = joinCommand(program, arguments);
        if (startWithHyprlandFloatRule(command))
            return true;
    }

    return QProcess::startDetached(program, arguments);
}

} // namespace LauncherUtil
