#include "desktopscene.h"
#include "desktopicon.h"
#include "desktopconstants.h"

#include <QDir>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QMimeData>
#include <QUrl>

// External drop (file manager to desktop)

static QString resolveCollision(
    const QString &desktopPath,
    const QString &srcFileName,
    const QString &srcPath,
    QSet<QString> &alreadyUsed,
    QWidget *parent)
{
    const QString candidate = desktopPath + "/" + srcFileName;

    if (!QFileInfo::exists(candidate) && !alreadyUsed.contains(srcFileName)) {
        alreadyUsed.insert(srcFileName);
        return candidate;
    }

    const QFileInfo srcInfo(srcPath);
    if (srcInfo.exists() && srcInfo.absoluteFilePath() == candidate)
        return {};

    const auto btn = QMessageBox::question(
        parent,
        "File Already Exists",
        QString("An item named '%1' already exists on the Desktop.\n\n"
                "Do you want to replace it?").arg(srcFileName),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if (btn == QMessageBox::Cancel)
        return {};
    if (btn == QMessageBox::Yes) {
        alreadyUsed.insert(srcFileName);
        return candidate + "__REPLACE__";
    }

    const QFileInfo srcFileEntry(desktopPath + "/" + srcFileName);
    const QString baseName = srcFileEntry.completeBaseName();
    const QString suffix = srcFileEntry.suffix().isEmpty() ? "" : "." + srcFileEntry.suffix();
    int counter = 1;
    QString newName;
    do {
        newName = baseName + " (" + QString::number(counter) + ")" + suffix;
        ++counter;
    } while (QFileInfo::exists(desktopPath + "/" + newName)
             || alreadyUsed.contains(newName));

    alreadyUsed.insert(newName);
    return desktopPath + "/" + newName;
}

void DesktopScene::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        bool hasLocal = false;
        for (const QUrl &url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                hasLocal = true;
                break;
            }
        }
        if (hasLocal) {
            event->setDropAction(Qt::MoveAction);
            event->accept();
            return;
        }
    }
    event->ignore();
}

void DesktopScene::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
}

void DesktopScene::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty()) {
        event->ignore();
        return;
    }

    const bool preferMove = true;
    const QPoint dropPos = event->position().toPoint();
    QPair<int,int> startCell = posToCell(dropPos);

    int transferred = 0;
    QStringList errors;
    QSet<QString> usedNames;

    for (const QUrl &url : urls) {
        if (!url.isLocalFile())
            continue;

        const QString srcPath = url.toLocalFile();
        const QFileInfo srcInfo(srcPath);
        if (!srcInfo.exists())
            continue;

        const QString resolved = resolveCollision(m_desktopPath, srcInfo.fileName(), srcPath, usedNames, this);
        if (resolved.isEmpty()) {
            errors << QString("Skipped '%1'.").arg(srcInfo.fileName());
            continue;
        }

        QString finalDest = resolved;
        if (finalDest.endsWith("__REPLACE__")) {
            finalDest.chop(11);
            if (srcInfo.absoluteFilePath() == finalDest) {
                errors << QString("Skipped '%1' (same file).").arg(srcInfo.fileName());
                continue;
            }
            if (QFileInfo(finalDest).isDir())
                QDir(finalDest).removeRecursively();
            else
                QFile::remove(finalDest);
        }

        bool ok = srcInfo.isDir() ? QDir().rename(srcPath, finalDest)
                                  : QFile::rename(srcPath, finalDest);

        if (!ok) {
            errors << QString("Could not move '%1'.").arg(srcInfo.fileName());
            continue;
        }

        const QFileInfo destInfo(finalDest);

        for (int i = 0; i < m_icons.size(); ++i) {
            if (!m_icons[i]->isVirtual()
                && m_icons[i]->fileInfo().fileName() == destInfo.fileName()) {
                m_icons[i]->deleteLater();
                m_icons.removeAt(i);
                --i;
            }
        }

        const auto cell = nearestFreeCell(startCell, destInfo.fileName());
        m_occupiedCells[cell] = destInfo.fileName();
        const QPoint snapped = cellToPos(cell.first, cell.second);
        m_positions[destInfo.fileName()] = snapped;

        auto *icon = new DesktopIcon(destInfo, m_iconSize, m_showPreview, this);
        icon->move(snapped);
        connectIconSignals(icon, false);
        m_icons.append(icon);
        icon->show();
        icon->playAppearAnimation();

        if (++startCell.second >= maxRows()) { startCell.second = 0; ++startCell.first; }
        ++transferred;
    }

    if (transferred > 0)
        queueSavePositions(kSaveDebounceMs);

    if (!errors.isEmpty()) {
        event->setDropAction(Qt::IgnoreAction);
    } else {
        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
}
