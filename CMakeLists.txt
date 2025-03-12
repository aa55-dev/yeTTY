cmake_minimum_required(VERSION 3.5)

project(yeTTY VERSION 0.1 LANGUAGES CXX)
set(PROJECT_DOMAIN "dev.aa55.yeTTY")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6  REQUIRED COMPONENTS Widgets SerialPort Multimedia)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort Multimedia)
find_package(KF6TextEditor REQUIRED)


find_package(PkgConfig REQUIRED)
pkg_check_modules(libzstd REQUIRED IMPORTED_TARGET libzstd)


set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        portselectiondialog.cpp
        portselectiondialog.h
        portselectiondialog.ui
        yetty.version.h.in
        resources/resources.qrc
        triggersetupdialog.h
        triggersetupdialog.cpp
        triggersetupdialog.ui
        longtermrunmodedialog.h
        longtermrunmodedialog.cpp
        longtermrunmodedialog.ui
)

configure_file(yetty.version.h.in ${CMAKE_CURRENT_BINARY_DIR}/yetty.version.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=logical-not-parentheses -pedantic \
-Wall -Wextra -Wno-comment -Wcast-align -Wcast-qual -Wctor-dtor-privacy \
-Wdisabled-optimization -Wformat=2 -Winit-self -Wlogical-op -Wmissing-declarations \
-Wmissing-include-dirs -Wnoexcept -Wold-style-cast -Woverloaded-virtual \
-Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo -Wstrict-null-sentinel \
-Wstrict-overflow=2 -Wswitch-default -Wundef -Werror -Wunused \
-Wno-deprecated-register -Werror=return-type -Werror=uninitialized -Wstrict-aliasing=2")

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialPort
    Qt${QT_VERSION_MAJOR}::Multimedia
    KF6::TextEditor
    PkgConfig::libzstd)

# systemd inhibit to prevent system from going to sleep while program is active
option(USE_SYSTEMD_INHIBIT "Use systemd-inhibit" ON)
pkg_check_modules(libsystemd IMPORTED_TARGET libsystemd)
if(libsystemd_FOUND)
    message("Building with systemd inhibit")
    target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::libsystemd)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SYSTEMD_AVAILABLE)
endif()


install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES resources/dev.aa55.yeTTY.desktop DESTINATION share/applications)
install(FILES resources/dev.aa55.yeTTY.svgz DESTINATION share/icons/hicolor/scalable/apps/)
