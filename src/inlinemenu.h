#pragma once

#include <QWidget>
#include <QPoint>
#include <QRect>
#include <QVector>
#include <functional>
#include <QPointer>
#include <QGraphicsEffect>
#include <QColor>

class QPropertyAnimation;

// Lightweight replacement for QMenu - implemented as a child widget
// to avoid Wayland popup grab issues with layer-shell surfaces.
class InlineMenu : public QWidget {
    Q_OBJECT
public:
    struct Item {
        QString  label;
        bool     isSep     = false;
        bool     checkable = false;
        bool     checked   = false;
        bool     isSub     = false;
        InlineMenu *submenu = nullptr;
        std::function<void()> fn;
    };

    static constexpr int kItemH = 22;
    static constexpr int kSepH  = 8;
    static constexpr int kPadL  = 18;
    static constexpr int kPadR  = 12;
    static constexpr int kMinW  = 130;

    explicit InlineMenu(QWidget *parent);

    void setRoot(InlineMenu *root);
    void setTargetOpacity(qreal opacity);
    void setThemeColors(const QColor &background, const QColor &text,
                       const QColor &accent, const QColor &border,
                       const QColor &separator);

    void clear();
    void addAction(const QString &text, std::function<void()> fn);
    void addCheckable(const QString &text, bool checked, std::function<void()> fn);
    void addSub(const QString &text, InlineMenu *sub);
    void addSep();

    void popup(const QPoint &pos, const QRect &available = QRect());
    void closeAll();
    void closeAllImmediate();

    // Internal: close entire tree from this menu (no root delegation)
    void _closeTree();
    void _hideTreeImmediate();

    // Internal: hide only this menu (no recursion, no delegation)
    void _hideSelf();

    bool hitMenu(const QPoint &scenePos, int depth = 0) const;

    // Exposed for desktopscene's hit-testing
    int hitTest(const QPoint &p) const;
    QRect itemRect(int idx) const;

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void leaveEvent(QEvent *ev) override;
    void mousePressEvent(QMouseEvent *ev) override;

private:
    void startFadeIn();
    void startFadeOut(std::function<void()> onFinished = nullptr);

    QVector<Item> m_items;
    int           m_hover     = -1;
    InlineMenu   *m_activeSubmenu = nullptr;
    InlineMenu   *m_root     = nullptr;
    QRect         m_available;
    QPointer<QPropertyAnimation> m_opacityAnim;
    int m_fadeSeq = 0;
    bool m_closing = false;
    qreal m_targetOpacity = 1.0;
    QColor m_background = QColor("#211921");
    QColor m_text = QColor("#F7EAF2");
    QColor m_accent = QColor("#F2A2CD");
    QColor m_border = QColor("#B96F9D");
    QColor m_separator = QColor("#493343");

    QPointer<QGraphicsOpacityEffect> m_opacityEffect;
};
