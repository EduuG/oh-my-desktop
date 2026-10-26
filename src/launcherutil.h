#pragma once

#include <QString>
#include <QStringList>

namespace LauncherUtil {

bool startDetachedWindowedCommand(const QString &command);
bool startDetachedWindowedProgram(const QString &program,
                                  const QStringList &arguments = {},
                                  bool floating = true,
                                  int width = 720,
                                  int height = 720);
}
