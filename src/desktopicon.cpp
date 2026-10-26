#include "desktopicon.h"
#include "launcherutil.h"
#include "desktopconstants.h"
#include "exeiconutil.h"

#include <QPainter>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QMimeData>
#include <QMimeDatabase>
#include <QUrl>
#include <QIcon>
#include <QApplication>
#include <QFontMetrics>
#include <QImageReader>
#include <QStandardPaths>
#include <QDir>
#include <QRegularExpression>
#include <QSettings>
#include <QPixmap>
#include <QFile>
#include <QTextStream>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTimer>

// ── Elided-label helpers ────────────────────────────────────────────────────
// Generates candidate strings for elision, ordered from longest to shortest.
// Each candidate truncates at a natural break point (underscore, hyphen, dot,
// space) and appends the ellipsis character.
static void elisionCandidates(const QString &name, QVector<QString> &out)
{
    out.clear();
    out.append(name);  // full name - check first, no ellipsis needed

    // Collect break positions: AFTER every underscore, hyphen, dot, or space.
    QVector<int> breaks;
    for (int i = 0; i < name.size(); ++i) {
        const QChar c = name[i];
        if (c == '_' || c == '-' || c == '.' || c == ' ')
            breaks.append(i + 1);
    }
    // Add evenly-spaced mid-word fallbacks so we always have enough candidates.
    const int step = qMax(1, name.size() / 8);
    for (int i = step; i < name.size(); i += step)
        breaks.append(i);

    std::sort(breaks.begin(), breaks.end());
    breaks.erase(std::unique(breaks.begin(), breaks.end()), breaks.end());

    // Iterate from the END (longest prefixes) to the beginning (shortest).
    // This way the first candidate that fits has the maximum visible text.
    for (int i = breaks.size() - 1; i >= 0; --i) {
        const int b = breaks[i];
        if (b > 0 && b < name.size()) {
            QString cand = name.left(b);
            cand.append(QChar(0x2026));  // …
            out.append(cand);
        }
    }
    // Fallback: three ellipsis chars (always fits).
    out.append(QString(3, QChar(0x2026)));
}

// Returns a 2-line elided version of *name* that fits inside *availWidth*.
// Tries natural break points first (underscores, dots) so the ellipsis never
// appears in the middle of a word.
static QString buildElided(const QFontMetrics &fm, const QString &name,
                            int availWidth, int maxLines)
{
    const QRect testRect(0, 0, availWidth, 9999);
    const int   maxH = fm.lineSpacing() * maxLines;
    const int   flags = Qt::AlignLeft | Qt::TextWrapAnywhere;

    if (fm.boundingRect(testRect, flags, name).height() <= maxH)
        return name;

    QVector<QString> candidates;
    elisionCandidates(name, candidates);

    // The first candidate is the full name (already checked above).  Walk
    // through the progressively shorter ones until one fits.
    for (int i = 1; i < candidates.size(); ++i) {
        if (fm.boundingRect(testRect, flags, candidates[i]).height() <= maxH)
            return candidates[i];
    }
    // Last resort: should always fit (three ellipsis chars).
    return candidates.last();
}


static DesktopEntryData parseDesktopEntry(const QString &filePath) {
    DesktopEntryData data;
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return data;
    QTextStream in(&f);
    bool inDesktop = false;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.startsWith('[')) {
            if (line == "[Desktop Entry]") {
                inDesktop = true;
                continue;
            }
            if (inDesktop)
                break;
            continue;
        }
        if (!inDesktop || line.startsWith('#') || !line.contains('=')) continue;
        int eq = line.indexOf('=');
        QString key = line.left(eq).trimmed();
        QString val = line.mid(eq+1).trimmed();
        if (key == "Name") data.name = val;
        else if (key == "Exec") data.exec = val;
        else if (key == "Icon") data.icon = val;
    }
    return data;
}

DesktopIcon::DesktopIcon(const QFileInfo &fileInfo, int iconSize, bool showPreview, QWidget *parent)
    : QWidget(parent)
    , m_fileInfo(fileInfo)
    , m_showPreview(showPreview)
    , m_iconDisplaySize(iconSize)
{
    setFixedSize(iconW(), iconH());
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::ArrowCursor);
    setMouseTracking(true);

    // .desktop file support
    if (fileInfo.suffix().toLower() == "desktop") {
        m_isDesktopFile = true;
        m_desktopEntry = parseDesktopEntry(fileInfo.absoluteFilePath());
        m_tipText = m_desktopEntry.name.isEmpty() ? fileInfo.fileName() : m_desktopEntry.name;
    } else {
        m_tipText = m_fileInfo.fileName();
    }
    resolveIcon();
    m_cachedLabelFont = QFont(); // invalidate: will rebuild on first paint with real font
}

DesktopIcon::DesktopIcon(VirtualType type, int iconSize, QWidget *parent)
    : QWidget(parent)
    , m_virtualType(type)
    , m_iconDisplaySize(iconSize)
{
    setFixedSize(iconW(), iconH());
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::ArrowCursor);
    setMouseTracking(true);
    if (type == VirtualType::Trash) {
        setAcceptDrops(true);
    }
    resolveIcon();
    m_tipText = virtualLabel();
    m_cachedLabelFont = QFont(); // invalidate
}

QString DesktopIcon::virtualLabel() const
{
    switch (m_virtualType) {
        case VirtualType::Trash:    return "Trash";
        case VirtualType::Computer: return "Root";
        case VirtualType::Home:     return "Home";
        default:                    return {};
    }
}

void DesktopIcon::rebuildLabel(const QFont &font)
{
    const QFontMetrics fm(font);
    QString fullName;
    if (isVirtual()) {
        fullName = virtualLabel();
    } else if (m_isDesktopFile && !m_desktopEntry.name.isEmpty()) {
        fullName = m_desktopEntry.name;
    } else {
        fullName = m_fileInfo.fileName();
    }
    const int avail = iconW() - kIconLabelPadding;
    m_labelElided         = buildElided(fm, fullName, avail, 2);
    // Selected state allows one extra line for more information.  The
    // paintEvent clamps the highlight to widget bounds as a safety net.
    m_labelElidedSelected = buildElided(fm, fullName, avail, 2 + kIconLabelExtraLines);
    m_tipText = fullName;
}

void DesktopIcon::updateSettings(int iconSize, bool showPreview)
{
    m_iconDisplaySize = iconSize;
    m_showPreview     = showPreview;
    setFixedSize(iconW(), iconH());
    m_hasThumbnail = false;
    m_thumbnail    = QPixmap();
    m_cachedIconPixmap = QPixmap();
    m_cachedIconSize = -1;
    resolveIcon();
    m_cachedLabelFont = QFont(); // invalidate
    update();
}

void DesktopIcon::updateSize(int iconSize)
{
    m_iconDisplaySize = iconSize;
    setFixedSize(iconW(), iconH());
    m_cachedIconPixmap = QPixmap();
    m_cachedIconSize = -1;
    m_cachedLabelFont = QFont(); // invalidate
    update();
}

void DesktopIcon::setLabelFont(const QString &family, int pointSize)
{
    m_fontFamily = family;
    m_fontSize = pointSize;
    m_cachedLabelFont = QFont(); // invalidate cache
    update();
}

void DesktopIcon::setGhost(bool ghost)
{
    if (m_ghost == ghost) return;
    m_ghost = ghost;
    update();
}

void DesktopIcon::setDropTarget(bool on)
{
    if (m_dropTarget == on) return;
    m_dropTarget = on;
    update();
}

void DesktopIcon::setCutState(bool cut)
{
    if (m_cut == cut) return;
    m_cut = cut;
    update();
}

void DesktopIcon::resolveIcon()
{
    // Invalidate cached rendered icon - it's about to change
    m_cachedIconPixmap = QPixmap();
    m_cachedIconSize = -1;

    if (m_virtualType != VirtualType::None) {
        switch (m_virtualType) {
            case VirtualType::Trash:
                m_icon = QIcon::fromTheme("user-trash",
                         QIcon::fromTheme("trashcan_empty",
                         QIcon::fromTheme("trash")));
                break;
            case VirtualType::Computer:
                m_icon = QIcon::fromTheme("computer",
                         QIcon::fromTheme("system"));
                break;
            case VirtualType::Home:
                m_icon = QIcon::fromTheme("user-home",
                         QIcon::fromTheme("folder-home"));
                break;
            default: break;
        }
        if (m_icon.isNull())
            m_icon = QIcon::fromTheme("folder");
        return;
    }

    // Custom icon for .desktop files
    if (m_isDesktopFile && !m_desktopEntry.icon.isEmpty()) {
        m_icon = QIcon::fromTheme(m_desktopEntry.icon);
        if (m_icon.isNull()) {
            // Try icon from absolute path
            QFileInfo iconFile(m_desktopEntry.icon);
            if (iconFile.exists())
                m_icon = QIcon(iconFile.absoluteFilePath());
        }
        if (!m_icon.isNull()) return;
    }

    // Windows .exe: extract embedded icon via ExeIconUtil.
    QPixmap exePixmap = ExeIconUtil::loadEmbeddedIcon(m_fileInfo);
    if (!exePixmap.isNull()) {
        m_thumbnail    = exePixmap;
        m_hasThumbnail = true;
        return;
    }

    static const QStringList kImageExts = {
        "jpg", "jpeg", "png", "gif", "bmp", "webp",
        "tiff", "tif", "xpm", "pbm", "pgm", "ppm"
    };

    const QString ext = m_fileInfo.suffix().toLower();
    if (m_showPreview && !m_fileInfo.isDir() && kImageExts.contains(ext)) {
        QImageReader reader(m_fileInfo.absoluteFilePath());
        reader.setAutoTransform(true);
        const QSize srcSize = reader.size();
        if (srcSize.isValid() && !srcSize.isEmpty()) {
            reader.setScaledSize(srcSize.scaled(kIconPreviewScale, kIconPreviewScale, Qt::KeepAspectRatio));
        }
        const QImage img = reader.read();
        if (!img.isNull()) {
            m_thumbnail    = QPixmap::fromImage(img);
            m_hasThumbnail = true;
            return;
        }
    }

    QMimeDatabase mimeDb;
    const QMimeType mimeType = mimeDb.mimeTypeForFile(m_fileInfo);
    QStringList iconNames;

    if (m_fileInfo.isDir()) {
        iconNames << "folder";
    } else {
        // Check by extension first for known executable types.
        // QMimeDatabase uses content sniffing by default, so .exe files
        // without recognizable magic bytes get resolved as text/plain
        // or inode/x-empty. Dolphin uses extension-based lookup, so
        // we do the same for visual parity.
        if (ext == "exe" || ext == "bin" || ext == "run" || ext == "app") {
            iconNames << "application-x-ms-dos-executable"
                      << "application-x-executable";
        }

        if (!mimeType.iconName().isEmpty())
            iconNames << mimeType.iconName();
        if (!mimeType.genericIconName().isEmpty())
            iconNames << mimeType.genericIconName();

        const QString extIcon = "application-x-extension-" + ext;
        if (!ext.isEmpty())
            iconNames << extIcon;

        iconNames << "text-x-generic";
    }

    for (const QString &iconName : iconNames) {
        if (iconName.isEmpty())
            continue;
        m_icon = QIcon::fromTheme(iconName);
        if (!m_icon.isNull())
            return;
    }

    m_icon = QIcon::fromTheme(m_fileInfo.isDir() ? "folder" : "text-x-generic");
}

void DesktopIcon::setSelected(bool selected)
{
    if (m_selected == selected) return;
    m_selected = selected;
    if (selected)
        playAttentionAnimation();
    update();
}

void DesktopIcon::setLabelAppearance(const QColor &textColor, const QColor &outlineColor,
                                     int outlineWidth)
{
    if (textColor.isValid())
        m_labelTextColor = textColor;
    if (outlineColor.isValid())
        m_labelOutlineColor = outlineColor;
    m_labelOutlineWidth = qBound(0, outlineWidth, 6);
    update();
}

void DesktopIcon::setAnimationPreferences(bool enabled, bool appearanceEnabled,
                                           bool selectionEnabled, int speedPercent,
                                           int intensityPercent)
{
    m_animationsEnabled = enabled;
    m_appearanceAnimationEnabled = appearanceEnabled;
    m_selectionAnimationEnabled = selectionEnabled;
    m_animationSpeed = qBound(25, speedPercent, 300);
    m_animationIntensity = qBound(25, intensityPercent, 200);
}

void DesktopIcon::setVisualScale(qreal value)
{
    m_visualScale = qBound<qreal>(0.65, value, 1.25);
    update();
}

void DesktopIcon::setVisualAngle(qreal value)
{
    m_visualAngle = qBound<qreal>(-12.0, value, 12.0);
    update();
}

void DesktopIcon::playDragWobble()
{
    if (!m_animationsEnabled)
        return;
    if (m_visualAnimation)
        m_visualAnimation->stop();
    auto *animation = new QPropertyAnimation(this, "visualAngle", this);
    m_visualAnimation = animation;
    const qreal amplitude = 2.0 * m_animationIntensity / 100.0;
    animation->setDuration(qMax(80, 22000 / m_animationSpeed));
    animation->setStartValue(-amplitude);
    animation->setKeyValueAt(0.25, amplitude);
    animation->setKeyValueAt(0.50, -amplitude);
    animation->setKeyValueAt(0.75, amplitude);
    animation->setEndValue(-amplitude);
    animation->setLoopCount(-1);
    animation->start(QPropertyAnimation::DeleteWhenStopped);
}

void DesktopIcon::playDeleteShake()
{
    if (!m_animationsEnabled)
        return;
    if (m_visualAnimation)
        m_visualAnimation->stop();
    auto *animation = new QPropertyAnimation(this, "visualAngle", this);
    m_visualAnimation = animation;
    const qreal amplitude = 5.0 * m_animationIntensity / 100.0;
    animation->setDuration(qMax(100, 26000 / m_animationSpeed));
    animation->setStartValue(0.0);
    animation->setKeyValueAt(0.16, -amplitude);
    animation->setKeyValueAt(0.32, amplitude);
    animation->setKeyValueAt(0.48, -amplitude);
    animation->setKeyValueAt(0.64, amplitude);
    animation->setKeyValueAt(0.80, -amplitude / 2.0);
    animation->setEndValue(0.0);
    connect(animation, &QPropertyAnimation::finished, this, [this, animation] {
        if (m_visualAnimation == animation)
            m_visualAnimation = nullptr;
        setVisualAngle(0.0);
    });
    animation->start(QPropertyAnimation::DeleteWhenStopped);
}

void DesktopIcon::playAppearAnimation(int delayMs)
{
    if (!m_animationsEnabled || !m_appearanceAnimationEnabled)
        return;
    if (!m_animationEffect) {
        auto *effect = new QGraphicsOpacityEffect(this);
        effect->setOpacity(1.0);
        setGraphicsEffect(effect);
        m_animationEffect = effect;
    }

    QTimer::singleShot(qMax(0, delayMs), this, [this] {
        if (!m_animationEffect)
            return;
        if (m_animation)
            m_animation->stop();
        m_animationEffect->setOpacity(0.0);
        auto *animation = new QPropertyAnimation(m_animationEffect, "opacity", this);
        m_animation = animation;
        animation->setDuration(qMax(40, 22000 / m_animationSpeed));
        animation->setStartValue(0.0);
        animation->setEndValue(1.0);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        connect(animation, &QPropertyAnimation::finished, this, [this, animation] {
            if (m_animation == animation)
                m_animation = nullptr;
        });
        animation->start(QPropertyAnimation::DeleteWhenStopped);

        if (m_visualAnimation)
            m_visualAnimation->stop();
        auto *bounce = new QPropertyAnimation(this, "visualScale", this);
        m_visualAnimation = bounce;
        const qreal amplitude = 0.12 * m_animationIntensity / 100.0;
        bounce->setDuration(qMax(80, 26000 / m_animationSpeed));
        bounce->setStartValue(0.82);
        bounce->setKeyValueAt(0.48, 1.0 + amplitude);
        bounce->setKeyValueAt(0.72, 1.0 - amplitude / 2.0);
        bounce->setEndValue(1.0);
        connect(bounce, &QPropertyAnimation::finished, this, [this, bounce] {
            if (m_visualAnimation == bounce)
                m_visualAnimation = nullptr;
            setVisualScale(1.0);
        });
        bounce->start(QPropertyAnimation::DeleteWhenStopped);
    });
}

void DesktopIcon::playAttentionAnimation()
{
    if (!m_animationsEnabled || !m_selectionAnimationEnabled)
        return;
    if (!m_animationEffect) {
        auto *effect = new QGraphicsOpacityEffect(this);
        effect->setOpacity(1.0);
        setGraphicsEffect(effect);
        m_animationEffect = effect;
    }
    if (m_animation)
        m_animation->stop();
    auto *animation = new QPropertyAnimation(m_animationEffect, "opacity", this);
    m_animation = animation;
    animation->setDuration(qMax(30, 14000 / m_animationSpeed));
    animation->setStartValue(0.72);
    animation->setKeyValueAt(0.5, 1.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::InOutSine);
    connect(animation, &QPropertyAnimation::finished, this, [this, animation] {
        if (m_animation == animation)
            m_animation = nullptr;
    });
    animation->start(QPropertyAnimation::DeleteWhenStopped);

    if (m_visualAnimation)
        m_visualAnimation->stop();
    auto *scale = new QPropertyAnimation(this, "visualScale", this);
    m_visualAnimation = scale;
    const qreal amplitude = 0.08 * m_animationIntensity / 100.0;
    scale->setDuration(qMax(50, 14000 / m_animationSpeed));
    scale->setStartValue(1.0);
    scale->setKeyValueAt(0.5, 1.0 + amplitude);
    scale->setEndValue(1.0);
    connect(scale, &QPropertyAnimation::finished, this, [this, scale] {
        if (m_visualAnimation == scale)
            m_visualAnimation = nullptr;
        setVisualScale(1.0);
    });
    scale->start(QPropertyAnimation::DeleteWhenStopped);
}

void DesktopIcon::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    if (m_ghost) p.setOpacity(0.4);
    p.translate(width() / 2.0, height() / 2.0);
    p.rotate(m_visualAngle);
    p.scale(m_visualScale, m_visualScale);
    p.translate(-width() / 2.0, -height() / 2.0);

    const QRect iconRect((iconW() - m_iconDisplaySize) / 2, kIconLabelMarginTop, m_iconDisplaySize, m_iconDisplaySize);

    // Drop-target highlight (trash)
    if (m_dropTarget) {
        p.fillRect(iconRect.adjusted(-2,-2,2,2), QColor(0, 0, 200, 60));
    }

    // Cut state: dim the icon slightly
    const qreal origOpacity = p.opacity();
    if (m_cut) p.setOpacity(0.5);

    if (m_hasThumbnail) {
        const QSize scaled = m_thumbnail.size().scaled(iconRect.size(), Qt::KeepAspectRatio);
        QRect dst(QPoint(0, 0), scaled);
        dst.moveCenter(iconRect.center());
        p.drawPixmap(dst, m_thumbnail);
    } else {
        // Request the icon pixmap at the exact target size.
        // QIcon::pixmap() picks the best available resolution from the
        // icon theme and scales it cleanly.  HiDPI: multiply by the
        // screen devicePixelRatio so we get enough source pixels.
        const qreal dpr = devicePixelRatioF();
        const int pxSide = qRound(m_iconDisplaySize * dpr);
        QPixmap px;
        if (m_cachedIconSize != m_iconDisplaySize) {
            px = m_icon.pixmap(pxSide, pxSide, QIcon::Normal, QIcon::Off);
            if (!px.isNull()) {
                px.setDevicePixelRatio(dpr);
                m_cachedIconPixmap = px;
                m_cachedIconSize = m_iconDisplaySize;
            }
        }
        if (!m_cachedIconPixmap.isNull()) {
            const QSize scaled = m_cachedIconPixmap.size()
                .scaled(iconRect.size(), Qt::KeepAspectRatio);
            QRect dst(QPoint(0, 0), scaled);
            dst.moveCenter(iconRect.center());
            p.drawPixmap(dst, m_cachedIconPixmap);
        } else {
            // Last-resort fallback: let QIcon paint whatever it can.
            m_icon.paint(&p, iconRect);
        }
    }

    // Hover brightening: white overlay on the icon image
    if (m_hovered && !m_selected) {
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        p.fillRect(iconRect, QColor(50, 50, 50, 120));
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

    // Restore opacity for label drawing
    if (m_cut) p.setOpacity(origOpacity);

    // ── Label ───────────────────────────────────────────────────────────────
    QFont font(m_fontFamily, m_fontSize);
    p.setFont(font);
    QFontMetrics fm(font);

    // Rebuild cache if font changed (e.g. first paint after QSS applied)
    if (font != m_cachedLabelFont) {
        m_cachedLabelFont = font;
        rebuildLabel(font);
    }

    const QString &label = m_selected ? m_labelElidedSelected : m_labelElided;

    // Measure the actual text height to keep the highlight tight.
    // Clamp to widget bottom so we never paint outside our bounds.
    const int textTop  = iconRect.bottom() + kIconLabelMarginTop;
    const int maxTextH = height() - textTop;
    const QRect measureRect(kIconLabelMarginX, textTop, iconW() - kIconLabelPadding, maxTextH);
    const QRect boundRect = fm.boundingRect(measureRect,
        Qt::AlignTop | Qt::AlignHCenter | Qt::TextWrapAnywhere, label);
    const int highlightH = qMin(maxTextH, qMax(fm.height() + kIconLabelMinHeightPadding, boundRect.height() + kIconLabelMinHeightPadding));
    const QRect highlightRect(0, textTop, iconW(), highlightH);
    const QRect textRect = highlightRect.adjusted(kIconLabelMarginX, kIconLabelMarginX, -kIconLabelMarginX, -kIconLabelMarginX);

    if (m_selected) {
        p.fillRect(highlightRect, QColor(0, 0, 128));

        // Simple selection border around the whole icon (image + label)
        const QRect selRect(0, 0, iconW(), highlightRect.bottom() + 1);
        p.setPen(QPen(QColor(255, 255, 255, 180), 1, Qt::DotLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(selRect.adjusted(0, 0, -1, -1));
    } else if (m_hovered) {
        p.fillRect(highlightRect, QColor(0, 0, 128, 80));
    }

    // Theme/user-configurable outline + label. Multiple small offset draws are
    // inexpensive for icon labels and preserve wrapped-text alignment.
    p.setPen(m_labelOutlineColor);
    const int outline = m_labelOutlineWidth;
    for (int y = -outline; y <= outline; ++y) {
        for (int x = -outline; x <= outline; ++x) {
            if (x == 0 && y == 0)
                continue;
            if (x * x + y * y <= outline * outline + 1)
                p.drawText(textRect.translated(x, y), Qt::AlignTop | Qt::AlignHCenter | Qt::TextWrapAnywhere, label);
        }
    }
    p.setPen(m_labelTextColor);
    p.drawText(textRect, Qt::AlignTop | Qt::AlignHCenter | Qt::TextWrapAnywhere, label);
}

void DesktopIcon::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_ctrlAtPress      = event->modifiers() & Qt::ControlModifier;
        m_dragStartPos     = event->globalPosition().toPoint();
        m_dragStartIconPos = pos();
        m_dragOffset       = event->position().toPoint();
        m_dragging         = false;
        m_lastDragPos      = pos();
        emit pressed(this, m_ctrlAtPress);
        event->accept();
    } else if (event->button() == Qt::RightButton) {
        m_rightPressed = true;
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void DesktopIcon::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        const QPoint delta = event->globalPosition().toPoint() - m_dragStartPos;
        if (!m_dragging && delta.manhattanLength() > QApplication::startDragDistance()) {
            m_dragging = true;
            playDragWobble();
            emit dragStarted(this, m_dragOffset);
        }
        if (m_dragging) {
            // Compute new position from delta - avoids mapToGlobal on every event
            const QPoint newPos = m_dragStartIconPos + delta;
            auto *pw = parentWidget();
            if (pw) {
                const int pwW = pw->width();
                const int pwH = pw->height();
                const QPoint clamped(qBound(0, newPos.x(), pwW - width()),
                                     qBound(0, newPos.y(), pwH - height()));
                m_lastDragPos = clamped;
                emit dragging(this, clamped);
            }
        }
    }
    QWidget::mouseMoveEvent(event);
}

void DesktopIcon::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_dragging)
            emit moved(this, m_lastDragPos); // virtual position, not actual widget pos
        emit released(this, m_dragging, m_ctrlAtPress);
        if (m_visualAnimation)
            m_visualAnimation->stop();
        setVisualAngle(0.0);
        setVisualScale(1.0);
        m_dragging = false;
        event->accept();
    } else if (event->button() == Qt::RightButton) {
        if (m_rightPressed) {
            setSelected(true);
            playAttentionAnimation();
            emit iconContextMenuRequested(this, mapToParent(event->position().toPoint()));
        }
        m_rightPressed = false;
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

void DesktopIcon::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_virtualType != VirtualType::None) {
            QString uri;
            switch (m_virtualType) {
                case VirtualType::Trash:    uri = "trash:///"; break;
                case VirtualType::Computer: uri = "computer:///"; break;
                case VirtualType::Home:     uri = QDir::homePath(); break;
                default: break;
            }
            if (!uri.isEmpty() && LauncherUtil::startDetachedWindowedProgram("gio", {"open", uri}))
                emit activationSucceeded();
        } else if (m_isDesktopFile && !m_desktopEntry.exec.isEmpty()) {
            // Run .desktop command via shell for maximum compatibility
            QString exec = m_desktopEntry.exec;
            // Remove field codes like %f, %u, etc.
            exec.replace(QRegularExpression("%[fFuUdDnNickvm]"), "");
            exec = exec.trimmed();
            if (!exec.isEmpty()) {
                emit openCommandRequested(exec);
            }
        } else {
            emit openRequested(m_fileInfo);
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

// .desktop icon fields (m_isDesktopFile, m_desktopEntry)



void DesktopIcon::contextMenuEvent(QContextMenuEvent *event)
{
    if (event->reason() == QContextMenuEvent::Mouse) {
        event->accept();
        return;
    }
    setSelected(true);
    playAttentionAnimation();
    emit iconContextMenuRequested(this, mapToParent(rect().center()));
    event->accept();
}

void DesktopIcon::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void DesktopIcon::dragEnterEvent(QDragEnterEvent *event)
{
    if (m_virtualType == VirtualType::Trash && event->mimeData()->hasUrls()) {
        m_dropTarget = true;
        update();
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DesktopIcon::dragLeaveEvent(QDragLeaveEvent *)
{
    m_dropTarget = false;
    update();
}

void DesktopIcon::dropEvent(QDropEvent *event)
{
    m_dropTarget = false;
    update();
    if (m_virtualType != VirtualType::Trash) return;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            emit fileDropped(url.toLocalFile());
    }
    event->acceptProposedAction();
}

void DesktopIcon::wheelEvent(QWheelEvent *event)
{
    // Forward to parent (DesktopScene) so Ctrl+Scroll zoom works
    // even when the cursor is over an icon.
    event->ignore();
    if (parentWidget())
        QCoreApplication::sendEvent(parentWidget(), event);
}

void DesktopIcon::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}
