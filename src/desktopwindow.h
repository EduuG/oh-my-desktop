#pragma once

#include <QWidget>
#include <QScreen>

class DesktopScene;

class DesktopWindow : public QWidget
{
    Q_OBJECT
public:
    explicit DesktopWindow(QScreen *screen, QWidget *parent = nullptr);
    ~DesktopWindow() override = default;

    void setKeyboardInteractive(bool on);
    void showSettings();
    DesktopScene *scene() const { return m_scene; }

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupLayerShell();

    DesktopScene *m_scene;
    QScreen *m_screen;
};
