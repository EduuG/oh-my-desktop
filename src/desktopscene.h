#pragma once

#include <QWidget>
#include <QFileInfo>
#include <QHash>
#include <QPoint>
#include <QFileSystemWatcher>
#include <QProcess>
#include <QPointer>
#include <QScreen>
#include <QRect>
#include <QByteArray>
#include <QContextMenuEvent>
#include <QSocketNotifier>
#include <QLabel>
#include <QTimer>
#include <QKeyEvent>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include "desktopconstants.h"

class DesktopIcon;
class InlineMenu;

class DesktopScene : public QWidget
{
    Q_OBJECT
public:
    enum class SortOrder { ByName, ByType, ByDate };

    explicit DesktopScene(QScreen *screen, QWidget *parent = nullptr);
    ~DesktopScene() override;

    void showSettings(bool openDesktopEntryTab = false);

signals:
    void trayEnabledChanged(bool enabled);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void refresh();
    void onIconPressed(DesktopIcon *icon, bool ctrlHeld);
    void onIconReleased(DesktopIcon *icon, bool wasDrag, bool ctrlHeld);
    void onIconDragStarted(DesktopIcon *icon, QPoint offset);
    void onIconDragging(DesktopIcon *icon, QPoint scenePos);
    void onIconMoved(DesktopIcon *icon, QPoint scenePos);
    void onOpenRequested(const QFileInfo &fileInfo);
    void onRenameRequested(DesktopIcon *icon);
    void onDeleteRequested(DesktopIcon *icon);
    void onDeleteKeys(bool permanent);
    void onFileDroppedOnTrash(const QString &filePath);
    void onNewFolder();
    void onNewDocument();
    void onNewDesktopEntry();
    void onExtractArchive(const QFileInfo &fileInfo);
    void onExtractFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onOpenInTerminal();
    void onArrangeBy(SortOrder order);
    void onAutoArrangeToggled();
    void onShowHiddenToggled();
    void onShowProperties(bool openDesktopEntryTab = false);
    void onIconContextMenuRequested(DesktopIcon *icon, QPoint scenePos);
    void closeAllMenus();

    // Clipboard
    void copySelectedIcons();
    void cutSelectedIcons();
    void pasteFromClipboard();
    void updateCutIconAppearance();

private:
    QString iconKey(DesktopIcon *icon) const;
    DesktopIcon *folderDropTargetAt(const QPoint &leaderCenter, DesktopIcon *leader) const;
    bool moveIconIntoFolder(DesktopIcon *icon, const QFileInfo &targetFolder, QString *errorMessage);
    void queueRefresh(int delayMs = 0);
    void queueSavePositions(int delayMs = 0);
    void updateDragFeedbackCursor(bool canDropOnTarget);
    void clearDragFeedbackCursor();
    DesktopIcon *selectedIcon() const;
    DesktopIcon *keyboardNavigationTarget(Qt::Key directionKey, DesktopIcon *fromIcon) const;
    void openIconFromKeyboard(DesktopIcon *icon);
    void loadPositions();
    void savePositions();
    QString configPath() const;
    void arrangeAll();
    void applyIconSize(int newSize); // resize + remap positions to new grid
    void applyFont(const QString &family, int size); // update label font on all icons
    void applyLabelAppearance();
    QString activeThemeName() const;
    void setupHyprlandIPC(); // socket2 connection + event parsing (extracted to desktopscene_ipc.cpp)
    void updateReservedAreas();
    void handleScreenGeometryChanged();
    QRect workArea() const;    // screen work area in scene-local coordinates
    QRect menuBoundary() const; // popup boundary respecting panel reserved areas
    void showDesktopContextMenuAt(const QPoint &scenePos);
    bool addCustomActions(InlineMenu *menu, bool separator = true);

    // Grid helpers
    QPoint cellToPos(int col, int row) const;
    QPair<int,int> posToCell(QPoint pos) const;
    QPair<int,int> nextFreeCell() const;
    QPair<int,int> nearestFreeCell(QPair<int,int> target, const QString &skipName) const;
    int maxCols() const;
    int maxRows() const;

    // Refresh pipeline helpers
    QFileInfoList scanDesktopFiles() const;
    void removeDeletedIcons(const QSet<QString>& expectedNames);
    void rebuildGridPositions();
    void addNewIcons(const QFileInfoList& entries);
    void connectIconSignals(DesktopIcon* icon, bool isVirtual);
    void syncVirtualIcons();
    void applyAutoArrange();
    void updateIconPreview();

    // Extract dialog
    void showExtractDialog(const QStringList &files, const QString &archiveName,
                           bool alreadyExisted, bool wasEmpty);

    // Extraction state (async)
    QPointer<QProcess>    m_extractProc;
    QString               m_extractArchivePath;
    QString               m_extractDir;
    QPair<int,int>        m_extractArchiveCell;
    QSet<QString>         m_extractExistingFiles;
    QString               m_extractBaseName;
    bool                  m_extracting     = false;

    QList<DesktopIcon *> m_icons;
    QHash<QString, QPoint>          m_positions;          // filename -> snapped pixel pos
    QHash<QPair<int,int>, QString>  m_occupiedCells;      // grid cell -> filename
    QHash<DesktopIcon*, QPoint>     m_dragStartPositions; // for multi-icon drag
    bool                            m_clipboardCutMode = false; // true = cut, false = copy
    QStringList                     m_clipboardPaths;   // absolute paths from last copy/cut
    QFileSystemWatcher *m_watcher;
    QScreen            *m_screen    = nullptr;
    QString             m_monitorKey;
    QWidget            *m_dragOverlay = nullptr; // ghost overlay raised above icons
    QString             m_desktopPath;
    SortOrder           m_sortOrder   = SortOrder::ByName;
    QPoint              m_lastContextMenuPos;
    QTimer             *m_refreshTimer = nullptr;
    QTimer             *m_saveTimer = nullptr;
    QTimer             *m_panelRefreshTimer = nullptr;
    bool                m_autoArrange = false;
    bool                m_showHidden  = false;
    bool                m_showTrash   = true;
    bool                m_showComputer = false;
    bool                m_showHome    = false;
    bool                m_trayEnabled = true;
    int                 m_iconSize    = 40;
    bool                m_showPreview = true;
    QString             m_fontFamily  = "MS Sans Serif";
    int                 m_fontSize    = 8;
    QString             m_themeName = "Sakura";
    int                 m_settingsWidth = 1280;
    int                 m_settingsHeight = 720;
    int                 m_contextMenuOpacity = 94;
    int                 m_settingsOpacity = 94;
    QString             m_labelTextColor;
    QString             m_labelOutlineColor;
    int                 m_labelOutlineWidth = 1;
    bool                m_animationsEnabled = true;
    bool                m_animateAppearance = true;
    bool                m_animateSelection = true;
    int                 m_animationSpeed = 100;
    int                 m_animationIntensity = 100;
    bool                m_soundsEnabled = true;
    bool                m_soundCreateEnabled = true;
    bool                m_soundOpenEnabled = true;
    bool                m_soundRestartEnabled = true;
    bool                m_soundQuitEnabled = true;
    bool                m_soundDeleteEnabled = true;
    bool                m_soundSettingsEnabled = true;
    QString             m_soundCreateFile;
    QString             m_customThemePath;
    bool                m_useCustomTheme = false;
    QStringList         m_customActionNames;
    QStringList         m_customActionCommands;
    bool                m_customActionSubmenus[4] = {false, false, false, false};
    QStringList         m_customSubmenuNames[4];
    QStringList         m_customSubmenuCommands[4];
    QString             m_soundOpenFile;
    QString             m_soundRestartFile;
    QString             m_soundQuitFile;
    QString             m_soundDeleteFile;
    QString             m_soundSettingsFile;

    QWidget            *m_ctxMenu        = nullptr;
    QWidget            *m_ctxArrangeMenu = nullptr;
    QWidget            *m_iconCtxMenu    = nullptr;
    QWidget            *m_newMenu        = nullptr;
    QWidget            *m_systemMenu     = nullptr;
    QWidget            *m_customSubMenus[4] = {nullptr, nullptr, nullptr, nullptr};

    // Hyprland IPC listener
    QSocketNotifier    *m_hypNotifier  = nullptr;
    int                 m_hypSockFd    = -1;
    QString             m_currentWorkspace;
    // Panel reserved areas (queried from Hyprland, [left,top,right,bottom])
    int m_reservedLeft   = 0;
    int m_reservedTop    = 0;
    int m_reservedRight  = 0;
    int m_reservedBottom = 0;
    QByteArray          m_hypBuf;

    // Rubber-band selection
    bool   m_rubberBanding    = false;
    bool   m_rubberCtrl       = false;
    bool   m_cursorOnSurface  = true;  // false while cursor is outside our wl_surface
    bool   m_dragCursorActive = false;
    QChar  m_lastTypeSelectChar;
    int    m_lastTypeSelectMatch = -1;
    QPoint m_rubberOrigin;
    QRect  m_rubberRect;

    // In-scene tooltip - top-level Qt::ToolTip window so it appears above layer-shell surfaces
    QWidget *m_tooltipWindow = nullptr;
    QLabel  *m_tooltipLabel  = nullptr;
    QTimer  *m_tooltipTimer  = nullptr;

    int iconW() const { return m_iconSize + kIconExtraWidth; }
    int iconH() const { return m_iconSize + kIconExtraHeight; }
    int cellW() const { return iconW() + kGridMarginX; }
    int cellH() const { return iconH() + kGridMarginY; }
};
