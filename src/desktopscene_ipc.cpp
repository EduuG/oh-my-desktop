#include "desktopscene.h"
#include "desktopicon.h"
#include "desktopconstants.h"

#include <QGuiApplication>
#include <QProcess>
#include <qlogging.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

// ── Hyprland IPC (socket2 event listener) ─────────────────────────────────────

void DesktopScene::setupHyprlandIPC()
{
    const QByteArray runtimeDir = qgetenv("XDG_RUNTIME_DIR");
    const QByteArray his        = qgetenv("HYPRLAND_INSTANCE_SIGNATURE");

    if (runtimeDir.isEmpty() || his.isEmpty()) {
        qWarning() << "[IPC] HYPRLAND_INSTANCE_SIGNATURE or XDG_RUNTIME_DIR not set - socket2 disabled";
        return;
    }

    const QString sockPath = QString("%1/hypr/%2/.socket2.sock")
                                 .arg(QString::fromLocal8Bit(runtimeDir))
                                 .arg(QString::fromLocal8Bit(his));

    m_hypSockFd = ::socket(AF_UNIX, SOCK_STREAM, 0);

    if (m_hypSockFd < 0) {
        qWarning() << "[IPC] socket() failed - workspace events won't be tracked";
        return;
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;

    ::strncpy(
        addr.sun_path,
        sockPath.toLocal8Bit().constData(),
        sizeof(addr.sun_path) - 1
    );

    if (::connect(
            m_hypSockFd,
            reinterpret_cast<struct sockaddr*>(&addr),
            sizeof(addr)
        ) != 0) {

        qWarning() << "[IPC] connect() to socket2 failed - workspace events won't be tracked";
        ::close(m_hypSockFd);
        m_hypSockFd = -1;
        return;
    }

    // Connection succeeded - set up the notifier
    m_hypNotifier = new QSocketNotifier(
        m_hypSockFd,
        QSocketNotifier::Read,
        this
    );

    connect(m_hypNotifier, &QSocketNotifier::activated, this, [this]() {
        char buf[4096];

        const ssize_t n = ::read(
            m_hypSockFd,
            buf,
            sizeof(buf) - 1
        );

        if (n < 0) {
            qWarning()
                << "[IPC] socket2 read failed (errno"
                << errno
                << ") - disabling notifier";

            m_hypNotifier->setEnabled(false);
            return;
        }

        if (n == 0) {
            // Hyprland probably closed the socket (restart/crash)
            qWarning()
                << "[IPC] socket2 connection closed - disabling notifier";

            m_hypNotifier->setEnabled(false);
            return;
        }

        buf[n] = '\0';

        m_hypBuf.append(
            buf,
            static_cast<int>(n)
        );

        // Guard against unbounded buffer growth if events arrive
        // faster than the event loop can process them. Hyprland
        // events are single-line; the newest events are always at
        // the end, so dropping the oldest is safe.
        static constexpr int kMaxHypBufLocal = kMaxIPCBufferSize;

        if (m_hypBuf.size() > kMaxHypBufLocal) {
            const int drop = m_hypBuf.size() - kMaxHypBufLocal;
            const int nl   = m_hypBuf.indexOf('\n', drop);

            if (nl >= 0)
                m_hypBuf.remove(0, nl + 1);
            else
                m_hypBuf.remove(0, drop);
        }

        while (true) {
            const int nl = m_hypBuf.indexOf('\n');

            if (nl < 0)
                break;

            const QByteArray line = m_hypBuf.left(nl);
            m_hypBuf.remove(0, nl + 1);

            // "workspace>>" and "focusedmon>>" both signal a workspace switch
            if (line.startsWith("workspace>>")) {
                m_currentWorkspace =
                    QString::fromLocal8Bit(line.mid(11));

                closeAllMenus();

                for (auto *icon : m_icons)
                    icon->setSelected(false);

            } else if (line.startsWith("focusedmon>>")) {
                closeAllMenus();

                for (auto *icon : m_icons)
                    icon->setSelected(false);

            } else if (line.startsWith("openwindow>>")) {
                // openwindow>>windowaddress,workspacename,class,title
                //
                // Only redirect focus if SakuraDesktopIcons currently has
                // focus (QGuiApplication::focusWindow is our
                // DesktopWindow or one of its children).  This
                // prevents us from hijacking focus while the user
                // is actively typing in another application.
                if (!QGuiApplication::focusWindow())
                    continue;

                const QByteArray data = line.mid(12);
                const QList<QByteArray> parts = data.split(',');

                if (parts.size() >= 3) {
                    const QString addr =
                        QString::fromLocal8Bit(parts[0]);

                    const QString ws =
                        QString::fromLocal8Bit(parts[1]);

                    const QString cls =
                        QString::fromLocal8Bit(parts[2]);

                    if (!m_currentWorkspace.isEmpty()
                        && ws == m_currentWorkspace
                        && cls != QLatin1String("oh-my-desktop")) {

                        // Modern Hyprland Lua-compatible dispatcher.
                        //
                        // Old:
                        //   hyprctl dispatch focuswindow address:0x...
                        //
                        // New:
                        //   hyprctl dispatch
                        //     'hl.dsp.focus({ window = "address:0x..." })'
                        const QString dispatcher =
                            QStringLiteral(
                                "hl.dsp.focus({ window = \"address:0x%1\" })"
                            ).arg(addr);

                        if (!QProcess::startDetached(
                                "hyprctl",
                                {"dispatch", dispatcher}
                            )) {

                            qWarning()
                                << "[IPC] failed to dispatch Lua focus for"
                                << addr;
                        }
                    }
                }
            }
        }
    });
}