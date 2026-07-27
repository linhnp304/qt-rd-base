cmake_minimum_required(VERSION 3.16)

project(cross_p1 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(cross_p1
    src/main.cpp
    src/appsettings.cpp
    src/appsettings.h
    src/geo.h
    src/mainwindow.cpp
    src/mainwindow.h
    src/radarview.cpp
    src/radarview.h
    src/settingstab.cpp
    src/settingstab.h
    src/theme.h
)

target_link_libraries(cross_p1 PRIVATE Qt6::Widgets)

set_target_properties(cross_p1 PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

include(GNUInstallDirs)
install(TARGETS cross_p1
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
