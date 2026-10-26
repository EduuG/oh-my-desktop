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

QString luaStringLiteral(const QString &value)
{
    QString escaped = value;
    escaped.replace('\\', "\\\\");
    escaped.replace('\'', "\\'");
    escaped.replace('\n', "\\n");
    escaped.replace('\r', "\\r");
    escaped.replace('\t', "\\t");
    return '\'' + escaped + '\'';
}

bool startWithHyprlandFloatRule(const QString &command, int width, int height)
{
    const QString lua = QStringLiteral(
        "hl.dsp.exec_cmd(%1, { float = true, size = { %2, %3 }, center = true })")
        .arg(luaStringLiteral(command))
        .arg(width)
        .arg(height);

    const bool ok = QProcess::startDetached(
        "hyprctl",
        {"dispatch", lua}
    );

    if (!ok)
        qWarning() << "[IPC] hyprctl dispatch hl.dsp.exec_cmd failed for:"
                   << command.left(80);

    return ok;
}

} // namespace

bool startDetachedWindowedCommand(const QString &command)
{
    const QString trimmed = command.trimmed();
    if (trimmed.isEmpty())
        return false;

    if (canUseHyprlandFloatDispatch() &&
    startWithHyprlandFloatRule(trimmed, kWindowedLaunchSize, kWindowedLaunchSize))
        return true;

    return QProcess::startDetached("sh", {"-lc", trimmed});
}

bool startDetachedWindowedProgram(const QString &program,
                                  const QStringList &arguments,
                                  bool floating,
                                  int width,
                                  int height)
{
    if (program.trimmed().isEmpty())
        return false;

    if (floating && canUseHyprlandFloatDispatch()) {
        const QString command = joinCommand(program, arguments);
        if (startWithHyprlandFloatRule(command, width, height))
            return true;
    }

    return QProcess::startDetached(program, arguments);
}

} // namespace LauncherUtil
