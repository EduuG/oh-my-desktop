#!/usr/bin/env bash
set -Eeuo pipefail

# SakuraDesktopIcons local installer
#
# Install:
#   ./install.sh install
#
# Uninstall:
#   ./install.sh uninstall
#
# Running without an argument opens the same small interactive menu.

APP_NAME="SakuraDesktopIcons"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# Install into the standard per-user launcher locations.
BIN_DIR="${HOME}/.local/bin"
APP_DIR="${HOME}/.local/share/applications"
ICON_DIR="${HOME}/.local/share/icons/hicolor/scalable/apps"
BUILD_DIR="${SCRIPT_DIR}/build"
BINARY_PATH="${BIN_DIR}/${APP_NAME}"
LAUNCHER_PATH="${BIN_DIR}/${APP_NAME}-launcher"
DESKTOP_PATH="${APP_DIR}/${APP_NAME}.desktop"
ICON_PATH="${ICON_DIR}/sakura.svg"

if [[ -t 1 && -z "${NO_COLOR:-}" ]]; then
    C_RESET=$'\033[0m'; C_BOLD=$'\033[1m'; C_RED=$'\033[31m'
    C_GREEN=$'\033[32m'; C_YELLOW=$'\033[33m'; C_CYAN=$'\033[36m'; C_MAGENTA=$'\033[35m'
else
    C_RESET=''; C_BOLD=''; C_RED=''; C_GREEN=''; C_YELLOW=''; C_CYAN=''; C_MAGENTA=''
fi

info() { printf '%s•%s %s\n' "${C_CYAN}" "${C_RESET}" "$1"; }
success() { printf '%s✓%s %s\n' "${C_GREEN}" "${C_RESET}" "$1"; }
warning() { printf '%s!%s %s\n' "${C_YELLOW}" "${C_RESET}" "$1" >&2; }
error() { printf '%sError:%s %s\n' "${C_RED}" "${C_RESET}" "$1" >&2; exit 1; }

usage() {
    cat <<EOF
Usage: $0 [install|uninstall]

Install builds ${APP_NAME} and places it at:
  ${BINARY_PATH}

The desktop entry is created at:
  ${DESKTOP_PATH}
EOF
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || error "Required command not found: $1"
}

build_project() {
    require_command cmake
    require_command make || require_command ninja

    [[ -f "${SCRIPT_DIR}/CMakeLists.txt" ]] || error "CMakeLists.txt was not found next to install.sh."

    info "Configuring ${APP_NAME} in ${BUILD_DIR}"
    cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release

    info "Compiling ${APP_NAME}"
    cmake --build "${BUILD_DIR}" --parallel "$(get_cpu_count)"

    [[ -x "${BUILD_DIR}/${APP_NAME}" ]] || error "Build completed, but ${BUILD_DIR}/${APP_NAME} was not found."
}

get_cpu_count() {
    if command -v nproc >/dev/null 2>&1; then
        nproc
    else
        printf '2\n'
    fi
}

install_desktop_entry() {
    mkdir -p "${BIN_DIR}" "${APP_DIR}" "${ICON_DIR}"

    install -m 0755 "${BUILD_DIR}/${APP_NAME}" "${BINARY_PATH}.new"
    mv -f "${BINARY_PATH}.new" "${BINARY_PATH}"

    # The desktop entry uses this single-instance launcher instead of
    # starting the binary directly. flock closes the small race between two
    # rapid desktop clicks; the executable path check handles long process names.
    cat > "${LAUNCHER_PATH}.new" <<EOF
#!/usr/bin/env bash
set -u
APP_BINARY="${BINARY_PATH}"
LOCK_DIR="\${XDG_RUNTIME_DIR:-\${TMPDIR:-/tmp}}"
LOCK_FILE="\${LOCK_DIR}/${APP_NAME}.launch.lock"

is_running() {
    # Match the executable path, not only the process name. This avoids
    # pgrep's 15-character name limit on some procps implementations.
    ps -eo args= | awk -v exe="\${APP_BINARY}" '\$1 == exe { found=1 } END { exit(found ? 0 : 1) }'
}

if ! command -v flock >/dev/null 2>&1; then
    if is_running; then
        exit 0
    fi
    exec "\${APP_BINARY}" "\$@"
fi

exec 9>"\${LOCK_FILE}"
if ! flock -n 9; then
    exit 0
fi
if is_running; then
    exit 0
fi
exec "\${APP_BINARY}" "\$@"
EOF
    chmod 0755 "${LAUNCHER_PATH}.new"
    mv -f "${LAUNCHER_PATH}.new" "${LAUNCHER_PATH}"

    # Keep the application icon beside the local desktop entry. The binary
    # itself embeds the Qt icon, but desktop environments need a file path.
    if [[ -f "${SCRIPT_DIR}/assets/icons/sakura.svg" ]]; then
        install -m 0644 "${SCRIPT_DIR}/assets/icons/sakura.svg" "${ICON_PATH}"
    else
        warning "assets/icons/sakura.svg was not found; the desktop entry will use the application name as its icon fallback."
        rm -f "${ICON_PATH}"
    fi

    cat > "${DESKTOP_PATH}.new" <<EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=SakuraDesktopIcons
Comment=Wayland desktop icon manager for Qt6 and Hyprland
Exec=${LAUNCHER_PATH}
TryExec=${LAUNCHER_PATH}
Icon=${ICON_PATH}
Terminal=false
Categories=Utility;DesktopSettings;
StartupNotify=false
EOF
    # Keep the launcher executable as requested, even though desktop
    # environments generally only require read permission for .desktop files.
    chmod 0755 "${DESKTOP_PATH}.new"
    mv -f "${DESKTOP_PATH}.new" "${DESKTOP_PATH}"

    # Update the local desktop-entry cache when the utility is available.
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "${APP_DIR}" >/dev/null 2>&1 || true
    fi
}

install_app() {
    build_project
    install_desktop_entry
    success "${APP_NAME} installed."
    printf '  Binary:  %s\n' "${BINARY_PATH}"
    printf '  Desktop: %s\n' "${DESKTOP_PATH}"
}

uninstall_app() {
    local removed=0
    if [[ -e "${BINARY_PATH}" ]]; then
        rm -f "${BINARY_PATH}"
        removed=1
    fi
    if [[ -e "${LAUNCHER_PATH}" ]]; then
        rm -f "${LAUNCHER_PATH}"
        removed=1
    fi
    if [[ -e "${DESKTOP_PATH}" ]]; then
        rm -f "${DESKTOP_PATH}"
        removed=1
    fi
    if [[ -e "${ICON_PATH}" ]]; then
        rm -f "${ICON_PATH}"
        removed=1
    fi

    if command -v update-desktop-database >/dev/null 2>&1 && [[ -d "${APP_DIR}" ]]; then
        update-desktop-database "${APP_DIR}" >/dev/null 2>&1 || true
    fi

    # Remove empty directories only; never remove the user's source tree.
    rmdir --ignore-fail-on-non-empty "${ICON_DIR}" \
        "${HOME}/.local/share/icons/hicolor" "${HOME}/.local/share/icons" \
        "${APP_DIR}" "${HOME}/.local/share" "${BIN_DIR}" 2>/dev/null || true

    if (( removed )); then
        success "${APP_NAME} and its desktop entry were removed."
    else
        info "No local installation was found."
    fi
}

sakura_banner() {
    printf '\n%s%s              .@@@@@@.                         %s\n' "${C_MAGENTA}" "${C_BOLD}" "${C_RESET}"
    printf '%s%s          .@@@o@@@@@o@@.      .              %s\n' "${C_MAGENTA}" "${C_BOLD}" "${C_RESET}"
    printf '%s%s        @@o@@@@@@@@@@o@@@                       %s\n' "${C_MAGENTA}" "${C_BOLD}" "${C_RESET}"
    printf '%s%s       ~@@@o@@@@@@@o@@~       S A K U R A       %s\n' "${C_MAGENTA}" "${C_BOLD}" "${C_RESET}"
    printf '%s%s          ~~@@@@|/@@@~~       Desktop Icons      %s\n' "${C_MAGENTA}" "${C_BOLD}" "${C_RESET}"
    printf '%s%s               |                                  %s\n' "${C_MAGENTA}" "${C_BOLD}" "${C_RESET}"
    printf '%s%s              /|\                 INSTALLER     %s\n' "${C_MAGENTA}" "${C_BOLD}" "${C_RESET}"
    printf '%s%s             *__|__*                              %s\n\n' "${C_MAGENTA}" "${C_BOLD}" "${C_RESET}"
}

interactive_menu() {
    sakura_banner
    printf '%s%s  INSTALLED%s                                      %s%s SETTINGS%s\n' "${C_GREEN}" "${C_BOLD}" "${C_RESET}" "${C_CYAN}" "${C_BOLD}" "${C_RESET}"
    printf '  %-24s %s✓ local build + install%s          binary  %s~/.local/bin%s\n' "${APP_NAME}" "${C_GREEN}" "${C_RESET}" "${C_DIM:-}" "${C_RESET}"
    printf '  %-24s %s• desktop entry%s                  desktop %s~/.local/share/applications%s\n' "${APP_NAME}.desktop" "${C_CYAN}" "${C_RESET}" "${C_DIM:-}" "${C_RESET}"
    printf '  %-24s %sexecutable files enabled%s          icon    %s~/.local/share/icons%s\n\n' "installer.sh" "${C_GREEN}" "${C_RESET}" "${C_DIM:-}" "${C_RESET}"
    printf '  %s1%s  Install / build\n' "${C_GREEN}" "${C_RESET}"
    printf '  %s2%s  Uninstall\n' "${C_RED}" "${C_RESET}"
    printf '  %s0%s  Exit\n\n' "${C_CYAN}" "${C_RESET}"
    read -r -p 'Choose [0-2]: ' choice || choice=0
    case "${choice}" in
        1) install_app ;;
        2) uninstall_app ;;
        0) exit 0 ;;
        *) error "Please choose 0, 1 or 2." ;;
    esac
}

case "${1:-}" in
    install) install_app ;;
    uninstall|remove) uninstall_app ;;
    -h|--help|help) usage ;;
    '') interactive_menu ;;
    *) usage; exit 2 ;;
esac
