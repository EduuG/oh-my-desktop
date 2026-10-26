#pragma once

#include <QString>

class SoundManager
{
public:
    static bool available();
    static bool playEvent(const QString &eventId, const QString &customFile = QString());
};
