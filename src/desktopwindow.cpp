#include "desktopwindow.h"
#include "desktopscene.h"

#include <LayerShellQt/Window>

#include <QVBoxLayout>
#include <QWindow>
#include <QResizeEvent>
#include <QGuiApplication>

DesktopWindow::DesktopWindow(QScreen *screen, QWidget *parent)
    : QWidget(parent)
    , m_screen(screen)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_X11NetWmWindowTypeDesktop, true);
    setObjectName("DesktopWindow");

    // Fill the screen geometry
    setGeometry(screen->geometry());
    connect(screen, &QScreen::geometryChanged, this, [this](const QRect &geometry) {
        setGeometry(geometry);
    });

    m_scene = new DesktopScene(screen, this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_scene);
    setLayout(layout);

    // Force native QWindow creation without mapping it to the compositor,
    // so LayerShellQt can set the layer-shell integration before Qt creates
    // the default xdg-toplevel surface on first show().
    winId();
    setupLayerShell();
    show();
}

void DesktopWindow::setupLayerShell()
{
    // LayerShellQt::Window::get() is only safe when a Wayland platform is active.
    // Guard this for offscreen/X11 sessions and headless smoke tests.
    if (!QGuiApplication::platformName().startsWith("wayland", Qt::CaseInsensitive))
        return;
    auto *lsWindow = LayerShellQt::Window::get(windowHandle());
    if (!lsWindow) return;

    using LSWindow = LayerShellQt::Window;
    lsWindow->setLayer(LSWindow::LayerBackground);
    lsWindow->setAnchors(LSWindow::Anchors(LSWindow::AnchorTop | LSWindow::AnchorBottom |
                         LSWindow::AnchorLeft | LSWindow::AnchorRight));
    lsWindow->setExclusiveZone(-1);
    lsWindow->setKeyboardInteractivity(LSWindow::KeyboardInteractivityOnDemand);
    lsWindow->setScope("hyprsillydesktopicons-desktop");
}

void DesktopWindow::setKeyboardInteractive(bool on)
{
    if (!QGuiApplication::platformName().startsWith("wayland", Qt::CaseInsensitive))
        return;
    auto *lsWindow = LayerShellQt::Window::get(windowHandle());
    if (!lsWindow) return;
    using LSWindow = LayerShellQt::Window;
    lsWindow->setKeyboardInteractivity(on ? LSWindow::KeyboardInteractivityOnDemand
                                          : LSWindow::KeyboardInteractivityNone);
}

void DesktopWindow::showSettings()
{
    if (m_scene)
        m_scene->showSettings();
}

void DesktopWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_scene)
        m_scene->setFixedSize(event->size());
}
