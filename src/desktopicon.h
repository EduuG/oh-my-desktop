#include <QString>

// .desktop file support
struct DesktopEntryData {
    QString name;
    QString exec;
    QString icon;
};


#pragma once

#include <QWidget>
#include <QFileInfo>
#include <QPoint>
#include <QPixmap>
#include <QIcon>
#include <QWheelEvent>
#include <QPointer>
#include <QColor>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include "desktopconstants.h"

class DesktopIcon : public QWidget
{
    private:
        bool m_isDesktopFile = false;
        DesktopEntryData m_desktopEntry;
    Q_OBJECT
    Q_PROPERTY(qreal visualScale READ visualScale WRITE setVisualScale)
    Q_PROPERTY(qreal visualAngle READ visualAngle WRITE setVisualAngle)
public:
    enum class VirtualType { None, Trash, Computer, Home };

    explicit DesktopIcon(const QFileInfo &fileInfo, int iconSize, bool showPreview,
                         QWidget *parent = nullptr);
    // Virtual icon (Trash, My Computer, etc.)
    explicit DesktopIcon(VirtualType type, int iconSize, QWidget *parent = nullptr);

    const QFileInfo &fileInfo() const { return m_fileInfo; }
    VirtualType virtualType() const   { return m_virtualType; }
    bool isVirtual() const            { return m_virtualType != VirtualType::None; }
    const QString &tipText() const    { return m_tipText; }

    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }
    void updateSettings(int iconSize, bool showPreview);
    void updateSize(int iconSize);
    void setLabelFont(const QString &family, int pointSize);
    void setLabelAppearance(const QColor &textColor, const QColor &outlineColor, int outlineWidth);
    void setGhost(bool ghost);
    void setDropTarget(bool on);
    void setCutState(bool cut);
    void playAppearAnimation(int delayMs = 0);
    void playAttentionAnimation();
    void setAnimationPreferences(bool enabled, bool appearanceEnabled,
                                  bool selectionEnabled, int speedPercent,
                                  int intensityPercent = 100);
    qreal visualScale() const { return m_visualScale; }
    void setVisualScale(qreal value);
    qreal visualAngle() const { return m_visualAngle; }
    void setVisualAngle(qreal value);
    void playDragWobble();
    void playDeleteShake();
    bool isCut() const { return m_cut; }

signals:
    void pressed(DesktopIcon *icon, bool ctrlHeld);
    void released(DesktopIcon *icon, bool wasDrag, bool ctrlHeld);
    void dragStarted(DesktopIcon *icon, QPoint localOffset);
    void dragging(DesktopIcon *icon, QPoint scenePos);
    void moved(DesktopIcon *icon, QPoint scenePos);
    void openRequested(const QFileInfo &fileInfo);
    void openCommandRequested(const QString &command);
    void activationSucceeded();
    void renameRequested(DesktopIcon *icon);
    void deleteRequested(DesktopIcon *icon);
    void iconContextMenuRequested(DesktopIcon *icon, QPoint scenePos);
    // Emitted when a file icon is dropped onto this (trash) icon
    void fileDropped(const QString &filePath);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    // Drag-and-drop target (for trash)
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void resolveIcon();
    void rebuildLabel(const QFont &font);
    QString virtualLabel() const;

    QFileInfo   m_fileInfo;
    VirtualType m_virtualType     = VirtualType::None;
    bool    m_selected        = false;
    bool    m_hovered         = false;
    bool    m_dragging        = false;
    bool    m_ghost           = false;
    bool    m_dropTarget      = false;  // highlighted as drop target
    bool    m_cut             = false;  // dimmed: icon is in clipboard cut state
    bool    m_hasThumbnail    = false;
    bool    m_ctrlAtPress     = false;
    bool    m_rightPressed    = false;
    bool    m_showPreview     = true;
    int     m_iconDisplaySize = 40;
    QString m_fontFamily      = "MS Sans Serif";
    int     m_fontSize        = 8;
    QColor  m_labelTextColor = Qt::white;
    QColor  m_labelOutlineColor = QColor(0, 0, 0, 160);
    int     m_labelOutlineWidth = 1;
    bool    m_animationsEnabled = true;
    bool    m_appearanceAnimationEnabled = true;
    bool    m_selectionAnimationEnabled = true;
    int     m_animationSpeed = 100;
    int     m_animationIntensity = 100;
    QPoint  m_dragStartPos;      // global cursor pos at press
    QPoint  m_dragStartIconPos;  // icon pos() at press
    QPoint  m_dragOffset;
    QPoint  m_lastDragPos;
    QIcon   m_icon;
    QPixmap m_thumbnail;
    QPixmap m_cachedIconPixmap;      // cache for the rendered icon pixmap
    int     m_cachedIconSize = -1;   // size at which the cached pixmap was generated
    QString m_labelElided;           // cached 2-line elided (non-selected)
    QString m_labelElidedSelected;   // cached 4-line elided (selected)
    QString m_tipText;               // full filename for in-scene tooltip
    QFont   m_cachedLabelFont;       // font used to build the cache
    QPointer<QGraphicsOpacityEffect> m_animationEffect;
    QPointer<QPropertyAnimation> m_animation;
    QPointer<QPropertyAnimation> m_visualAnimation;
    qreal   m_visualScale = 1.0;
    qreal   m_visualAngle = 0.0;

    int iconW() const { return m_iconDisplaySize + kIconExtraWidth; }
    int iconH() const { return m_iconDisplaySize + kIconExtraHeight; }
};
