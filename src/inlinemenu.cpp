#include "inlinemenu.h"
#include <QPainter>
#include <QCursor>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QLinearGradient>

// ---------------------------------------------------------------------------
// InlineMenu - child-widget context menu (no QMenu, avoids Wayland grab issues)
// ---------------------------------------------------------------------------
InlineMenu::InlineMenu(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_NoSystemBackground);

    // Opacity effect for fade in/out
    auto *effect = new QGraphicsOpacityEffect(this);
    effect->setOpacity(1.0);
    setGraphicsEffect(effect);
    m_opacityEffect = effect;

    hide();
}

void InlineMenu::setRoot(InlineMenu *root) { m_root = root; }
void InlineMenu::setTargetOpacity(qreal opacity)
{
    m_targetOpacity = qBound<qreal>(0.2, opacity, 1.0);
    if (m_opacityEffect && !isVisible())
        m_opacityEffect->setOpacity(m_targetOpacity);
}
void InlineMenu::setThemeColors(const QColor &background, const QColor &text,
                                const QColor &accent, const QColor &border,
                                const QColor &separator)
{
    m_background = background; m_text = text; m_accent = accent;
    m_border = border; m_separator = separator; update();
}

void InlineMenu::clear() { m_items.clear(); m_hover = -1; m_activeSubmenu = nullptr; }

void InlineMenu::addAction(const QString &text, std::function<void()> fn) {
    m_items.push_back({text, false, false, false, false, nullptr, std::move(fn)});
}

void InlineMenu::addCheckable(const QString &text, bool checked, std::function<void()> fn) {
    m_items.push_back({text, false, true, checked, false, nullptr, std::move(fn)});
}

void InlineMenu::addSub(const QString &text, InlineMenu *sub) {
    sub->setRoot(m_root ? m_root : this);
    m_items.push_back({text, false, false, false, true, sub, nullptr});
}

void InlineMenu::addSep() { m_items.push_back({"", true}); }

void InlineMenu::popup(const QPoint &pos, const QRect &available) {
    QFontMetrics fm(font());
    int w = kMinW;
    for (auto &it : m_items)
        if (!it.isSep)
            w = qMax(w, fm.horizontalAdvance(it.label) + kPadL + kPadR + 4);

    int h = 8;
    for (auto &it : m_items)
        h += it.isSep ? kSepH : kItemH;

    m_available = available.isValid() ? available
                : QRect(0, 0, parentWidget()->width(), parentWidget()->height());

    QPoint p = pos;

    // Horizontal: shift left if it would overflow the right edge
    if (p.x() + w > m_available.right())
        p.rx() = qMax(m_available.left(), m_available.right() - w);

    // Vertical: open upward when there is more space above than below,
    // or when the menu would overflow downward. This prevents any edge-case
    // narrow band where the menu is clipped by the panel.
    const int spaceBelow = m_available.bottom() - p.y();
    const int spaceAbove = p.y() - m_available.top();
    if (h > spaceBelow && spaceAbove > spaceBelow)
        p.ry() = qMax(m_available.top(), p.y() - h);

    // Final clamp: keep fully inside the available area
    p.ry() = qBound(
        m_available.top(),
        p.y(),
        qMax(m_available.top(), m_available.bottom() - h)
    );

    resize(w, h);
    move(p);
    raise();
    m_closing = false;

    // Reset opacity and show
    if (m_opacityEffect) {
        m_opacityEffect->setOpacity(0.0);
    }

    show();
    startFadeIn();
}

void InlineMenu::closeAll() {
    // Delegate to root - avoids double-close
    InlineMenu *root = m_root ? m_root : this;
    root->_closeTree();
}

void InlineMenu::closeAllImmediate() {
    InlineMenu *root = m_root ? m_root : this;
    root->_hideTreeImmediate();
}

void InlineMenu::_closeTree() {
    for (auto &it : m_items)
        if (it.submenu)
            it.submenu->_hideSelf();

    startFadeOut([this]() { /* lambda keeps capture simple */ });
}

void InlineMenu::_hideTreeImmediate() {
    for (auto &it : m_items)
        if (it.submenu)
            it.submenu->_hideTreeImmediate();

    _hideSelf();
}

void InlineMenu::_hideSelf() {
    ++m_fadeSeq;

    if (m_opacityAnim) {
        m_opacityAnim->stop();
        m_opacityAnim->deleteLater();
        m_opacityAnim = nullptr;
    }

    m_closing = false;
    m_activeSubmenu = nullptr;
    m_hover = -1;

    if (m_opacityEffect) {
        m_opacityEffect->setOpacity(1.0);
    }

    hide();
}

bool InlineMenu::hitMenu(const QPoint &scenePos, int depth) const {
    if (depth > 16)
        return false;  // guard against circular submenu refs

    if (isVisible() && geometry().contains(scenePos))
        return true;

    for (auto &it : m_items)
        if (it.submenu && it.submenu->hitMenu(scenePos, depth + 1))
            return true;

    return false;
}

int InlineMenu::hitTest(const QPoint &p) const {
    int y = 4;

    for (int i = 0; i < (int)m_items.size(); ++i) {
        int h = m_items[i].isSep ? kSepH : kItemH;

        if (!m_items[i].isSep && p.y() >= y && p.y() < y + h)
            return i;

        y += h;
    }

    return -1;
}

QRect InlineMenu::itemRect(int idx) const {
    int y = 4;

    for (int i = 0; i < (int)m_items.size(); ++i) {
        int h = m_items[i].isSep ? kSepH : kItemH;

        if (i == idx)
            return {0, y, width(), h};

        y += h;
    }

    return {};
}

// ── Fade animations ─────────────────────────────────────────────────────────

static constexpr int kFadeMs = 170;

void InlineMenu::startFadeIn() {
    if (!m_opacityEffect)
        return;

    ++m_fadeSeq;

    if (m_opacityAnim) {
        m_opacityAnim->stop();
        m_opacityAnim->deleteLater();
        m_opacityAnim = nullptr;
    }

    auto *anim = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_opacityAnim = anim;
    m_closing = false;

    anim->setDuration(kFadeMs);
    anim->setStartValue(m_opacityEffect->opacity());
    anim->setEndValue(m_targetOpacity);
    anim->setEasingCurve(QEasingCurve::OutCubic);

    QObject::connect(anim, &QPropertyAnimation::finished, this, [this, anim]() {
        if (m_opacityAnim == anim)
            m_opacityAnim = nullptr;
    });

    anim->start(QPropertyAnimation::DeleteWhenStopped);
}

void InlineMenu::startFadeOut(std::function<void()> onFinished) {
    if (m_closing)
        return;

    if (!m_opacityEffect) {
        hide();
        if (onFinished)
            onFinished();
        return;
    }

    const int fadeSeq = ++m_fadeSeq;

    if (m_opacityAnim) {
        m_opacityAnim->stop();
        m_opacityAnim->deleteLater();
        m_opacityAnim = nullptr;
    }

    auto *anim = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_opacityAnim = anim;
    m_closing = true;

    anim->setDuration(kFadeMs);
    anim->setStartValue(m_opacityEffect->opacity());
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::InCubic);

    QObject::connect(anim, &QPropertyAnimation::finished, this,
                     [this, onFinished, fadeSeq, anim]() {
        if (m_opacityAnim == anim)
            m_opacityAnim = nullptr;

        if (fadeSeq != m_fadeSeq)
            return;

        m_closing = false;
        hide();

        if (onFinished)
            onFinished();
    });

    anim->start(QPropertyAnimation::DeleteWhenStopped);
}

void InlineMenu::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect().adjusted(1, 1, -2, -2);

    // -----------------------------------------------------------------------
    // Dark glassy blue / petrol menu
    // -----------------------------------------------------------------------

    // Soft outer shadow. It is drawn inside the widget so it does not require
    // another graphics effect and remains compatible with the fade animation.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 160));
    p.drawRoundedRect(r.adjusted(1, 2, 1, 1), 8, 8);

    // Main translucent glass background.
    QLinearGradient bgGradient(r.topLeft(), r.bottomLeft());
    QColor top = m_background.lighter(112); top.setAlpha(238);
    QColor bottom = m_background; bottom.setAlpha(245);
    bgGradient.setColorAt(0.0, top);
    bgGradient.setColorAt(1.0, bottom);

    p.setBrush(bgGradient);
    p.drawRoundedRect(r, 8, 8);

    // Thin outer cyan-teal edge.
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(m_border, 1));
    p.drawRoundedRect(r, 8, 8);

    // Very subtle inner highlight, giving the panel a glass edge.
    p.setPen(QPen(QColor(0x72, 0xC4, 0xD2, 0x55), 1));
    p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 7, 7);

    int y = 4;

    for (int i = 0; i < (int)m_items.size(); ++i) {
        const auto &it = m_items[i];

        // -------------------------------------------------------------------
        // Separator
        // -------------------------------------------------------------------
        if (it.isSep) {
            const int mid = y + kSepH / 2;

            p.setPen(m_separator);
            p.drawLine(kPadL - 2, mid, r.right() - 3, mid);

            // Small highlight directly under the separator.
            p.setPen(QColor(0x0A, 0x16, 0x1D, 0xA0));
            p.drawLine(kPadL - 2, mid + 1, r.right() - 3, mid + 1);

            y += kSepH;
            continue;
        }

        QRect ir(3, y, r.width() - 6, kItemH);

        // -------------------------------------------------------------------
        // Hovered item
        // -------------------------------------------------------------------
        if (i == m_hover) {
            QLinearGradient hoverGradient(ir.topLeft(), ir.bottomLeft());
            hoverGradient.setColorAt(0.0, m_accent.lighter(112));
            hoverGradient.setColorAt(1.0, m_accent.darker(110));

            p.setPen(Qt::NoPen);
            p.setBrush(hoverGradient);
            p.drawRoundedRect(ir, 5, 5);

            // Subtle cyan outline around the hovered item.
            p.setPen(QPen(m_accent, 1));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(ir.adjusted(0, 0, -1, -1), 5, 5);

            p.setPen(m_text.lighter(115));
        } else {
            p.setPen(m_text);
        }

        // Checkmark column
        if (it.checkable && it.checked) {
            p.drawText(
                QRect(4, y, kPadL - 6, kItemH),
                Qt::AlignVCenter | Qt::AlignCenter,
                "\u2713"
            );
        }

        // Label
        p.drawText(
            QRect(kPadL, y, r.width() - kPadL - kPadR, kItemH),
            Qt::AlignVCenter,
            it.label
        );

        // Submenu arrow
        if (it.isSub) {
            p.drawText(
                QRect(r.width() - kPadR, y, kPadR - 3, kItemH),
                Qt::AlignVCenter | Qt::AlignRight,
                "\u25BA"
            );
        }

        y += kItemH;
    }
}

void InlineMenu::mouseMoveEvent(QMouseEvent *ev) {
    int idx = hitTest(ev->pos());
    if (idx == m_hover)
        return;

    m_hover = idx;
    update();

    InlineMenu *root = m_root ? m_root : this;

    if (idx >= 0 && m_items[idx].isSub && m_items[idx].submenu) {
        if (root->m_activeSubmenu && root->m_activeSubmenu != m_items[idx].submenu)
            root->m_activeSubmenu->_closeTree();
        QRect r = itemRect(idx);
        m_items[idx].submenu->popup(
            mapToParent(QPoint(width() - 2, r.top())),
            m_available
        );
        root->m_activeSubmenu = m_items[idx].submenu;
    } else if (root->m_activeSubmenu && (idx < 0 || !m_items[idx].isSub)) {
        // Keep submenu open if the mouse is already hovering it
        // (child widget may not receive mouseMoveEvent before parent does)
        if (!root->m_activeSubmenu->underMouse()) {
            root->m_activeSubmenu->_closeTree();
            root->m_activeSubmenu = nullptr;
        }
    }
}

void InlineMenu::leaveEvent(QEvent *ev) {
    QWidget::leaveEvent(ev);

    InlineMenu *root = m_root ? m_root : this;
    if (!root->m_activeSubmenu)
        return;

    QWidget *container = parentWidget();
    if (!container)
        return;

    const QPoint scenePos = container->mapFromGlobal(QCursor::pos());
    if (!root->hitMenu(scenePos)) {
        root->m_activeSubmenu->_closeTree();
        root->m_activeSubmenu = nullptr;
        m_hover = -1;
        update();
    }
}

void InlineMenu::mousePressEvent(QMouseEvent *ev) {
    if (ev->button() == Qt::LeftButton) {
        int idx = hitTest(ev->pos());

        if (idx >= 0 && !m_items[idx].isSep && !m_items[idx].isSub) {
            auto fn = m_items[idx].fn;
            closeAll();
            if (fn)
                fn();
            return;
        }
    }

    ev->ignore();
}
