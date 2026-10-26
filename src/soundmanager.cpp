#include "soundmanager.h"

#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

bool SoundManager::available()
{
    return !QStandardPaths::findExecutable("canberra-gtk-play").isEmpty()
        || !QStandardPaths::findExecutable("paplay").isEmpty()
        || !QStandardPaths::findExecutable("pw-play").isEmpty();
}

bool SoundManager::playEvent(const QString &eventId, const QString &customFile)
{
    if (!customFile.isEmpty() && QFileInfo::exists(customFile)) {
        const QString pwPlay = QStandardPaths::findExecutable("pw-play");
        if (!pwPlay.isEmpty())
            return QProcess::startDetached(pwPlay, {customFile});
        const QString paPlay = QStandardPaths::findExecutable("paplay");
        if (!paPlay.isEmpty())
            return QProcess::startDetached(paPlay, {customFile});
    }

    const QString canberra = QStandardPaths::findExecutable("canberra-gtk-play");
    if (!canberra.isEmpty())
        return QProcess::startDetached(canberra, {"-i", eventId, "-d", "SakuraDesktopIcons"});

    // Without libcanberra, do not invent a sound-theme implementation. The
    // event is simply treated as optional and the application keeps running.
    return false;
}
