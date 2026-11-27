#include "desktopwindow.h"
#include "desktopscene.h"
#include "desktopconstants.h"

#include <LayerShellQt/Window>

#include <QVBoxLayout>
#include <QWindow>
#include <QResizeEvent>

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
    auto *lsWindow = LayerShellQt::Window::get(windowHandle());
    if (!lsWindow) return;

    using LSWindow = LayerShellQt::Window;
    lsWindow->setLayer(LSWindow::LayerBackground);
    lsWindow->setAnchors(LSWindow::Anchors(LSWindow::AnchorTop | LSWindow::AnchorBottom |
                         LSWindow::AnchorLeft | LSWindow::AnchorRight));
    lsWindow->setExclusiveZone(-1);
    lsWindow->setKeyboardInteractivity(LSWindow::KeyboardInteractivityOnDemand);
    lsWindow->setActivateOnShow(false);
    lsWindow->setScope(kLayerShellNamespace);
    lsWindow->setScreen(m_screen);
}

void DesktopWindow::setKeyboardInteractive(bool on)
{
    auto *lsWindow = LayerShellQt::Window::get(windowHandle());
    if (!lsWindow) return;
    using LSWindow = LayerShellQt::Window;
    lsWindow->setKeyboardInteractivity(on ? LSWindow::KeyboardInteractivityOnDemand
                                          : LSWindow::KeyboardInteractivityNone);
}

void DesktopWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_scene)
        m_scene->setFixedSize(event->size());
}
