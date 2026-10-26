#include "exeiconutil.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QProcess>

namespace ExeIconUtil {

static QString cacheDirPath()
{
    static QString path;
    if (path.isEmpty()) {
        path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
             + "/exe-icons";
        QDir().mkpath(path);
    }
    return path;
}

static QString sanitizeCacheKey(const QString &key)
{
    QString out;
    for (const QChar c : key) {
        if (c.isLetterOrNumber())
            out += c;
        else
            out += '_';
    }
    return out;
}

QPixmap loadEmbeddedIcon(const QFileInfo &fileInfo)
{
    if (fileInfo.suffix().toLower() != "exe")
        return QPixmap();

    const QString exePath = fileInfo.absoluteFilePath();
    if (!fileInfo.exists() || !fileInfo.isReadable())
        return QPixmap();

    // Check that wrestool is available.
    if (QStandardPaths::findExecutable("wrestool").isEmpty())
        return QPixmap();

    // Cache key based on path + mtime so icon auto-refreshes when file changes.
    const QString cacheKey = sanitizeCacheKey(exePath) + "_" +
                             QString::number(fileInfo.lastModified().toMSecsSinceEpoch());
    const QString cachePng = cacheDirPath() + "/" + cacheKey + ".png";

    // Serve from cache if valid.
    if (QFile::exists(cachePng)) {
        QImage img(cachePng);
        if (!img.isNull())
            return QPixmap::fromImage(img);
        QFile::remove(cachePng);  // corrupt entry
    }

    // Extract the main icon group (type 14) from the PE file.
    QProcess wrestool;
    wrestool.start("wrestool", {"-x", "--type=14", exePath});
    if (!wrestool.waitForFinished(3000))
        return QPixmap();

    const QByteArray icoData = wrestool.readAllStandardOutput();
    if (icoData.isEmpty())
        return QPixmap();

    // Write ICO to temp file, then extract best PNG with icotool.
    const QString tmpIco = QDir::tempPath() + "/_hyprsillydesktopicons_tmp.ico";
    {
        QFile tmp(tmpIco);
        if (!tmp.open(QIODevice::WriteOnly))
            return QPixmap();
        tmp.write(icoData);
    }

    // icotool -x --index=1 extracts the largest/best image as PNG.
    QProcess conv;
    conv.start("icotool", {"-x", "--index=1", "-o", cachePng, tmpIco});
    if (!conv.waitForFinished(3000)) {
        QFile::remove(tmpIco);
        return QPixmap();
    }
    QFile::remove(tmpIco);

    if (!QFile::exists(cachePng))
        return QPixmap();

    QImage img(cachePng);
    if (img.isNull()) {
        QFile::remove(cachePng);
        return QPixmap();
    }

    return QPixmap::fromImage(img);
}

} // namespace ExeIconUtil
