cmake_minimum_required(VERSION 3.20)
project(SakuraDesktopIcons VERSION 1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(LayerShellQt REQUIRED)

set(SOURCES
    src/main.cpp
    src/desktopwindow.cpp
    src/desktopscene.cpp
    src/desktopscene_grid.cpp
    src/desktopscene_dragdrop.cpp
    src/desktopscene_ipc.cpp
    src/desktopscene_externaldrag.cpp
    src/desktopscene_clipboard.cpp
    src/desktopicon.cpp
    src/exeiconutil.cpp
    src/launcherutil.cpp
    src/propertiesdialog.cpp
    src/filepropertiesdialog.cpp
    src/desktopentrydialog.cpp
    src/thememanager.cpp
    src/soundmanager.cpp
    src/inlinemenu.cpp
)

set(HEADERS
    src/desktopwindow.h
    src/desktopscene.h
    src/desktopicon.h
    src/exeiconutil.h
    src/launcherutil.h
    src/propertiesdialog.h
    src/filepropertiesdialog.h
    src/desktopentrydialog.h
    src/thememanager.h
    src/soundmanager.h
    src/inlinemenu.h
)

set(RESOURCES
    assets/resources.qrc
)

add_executable(SakuraDesktopIcons ${SOURCES} ${HEADERS} ${RESOURCES})

target_include_directories(SakuraDesktopIcons PRIVATE src)

target_link_libraries(SakuraDesktopIcons PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    LayerShellQt::Interface
)

install(TARGETS SakuraDesktopIcons)
install(FILES assets/themes/example.json examples/custom-theme.json
        DESTINATION share/SakuraDesktopIcons/examples)
