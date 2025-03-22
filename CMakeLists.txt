cmake_minimum_required(VERSION 3.5)

project(yeTTY VERSION 0.1 LANGUAGES CXX)
set(PROJECT_DOMAIN "dev.aa55.yeTTY")

option(ENABLE_ASAN "Enable address sanitizer" OFF)
option(ENABLE_MSAN "Enable memory sanitizer" OFF)
option(ENABLE_CLANG_TIDY "Enable clang tidy" OFF)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets SerialPort Multimedia)
find_package(KF6TextEditor REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(libzstd REQUIRED IMPORTED_TARGET libzstd)

if(ENABLE_ASAN AND ENABLE_MSAN)
  message(FATAL_ERROR "Both sanitizers cannot be enabled at same time")
endif()

if(ENABLE_ASAN)
  message("Building with ASAN")
  add_compile_options(-fsanitize=address)
  add_compile_options(-fsanitize=undefined)
  add_compile_options(-fno-sanitize-recover=all)
  add_compile_options(-fsanitize=float-divide-by-zero)
  add_compile_options(-fsanitize=float-cast-overflow)
  add_compile_options(-fno-sanitize=null)
  add_compile_options(-fno-sanitize=alignment)
  add_link_options(-fsanitize=address -fsanitize=undefined)
endif()

if(ENABLE_MSAN)
  message("Building with MSAN")
  add_compile_options(-fsanitize=memory -fno-omit-frame-pointer)
  add_link_options(-fsanitize=memory)
endif()

if (ENABLE_CLANG_TIDY)
    message("Enabling clang tidy")
    set(CMAKE_CXX_CLANG_TIDY
        "clang-tidy;-checks=*,-llvmlibc-restrict-system-libc-headers,\
        -llvmlibc-implementation-in-namespace,\
        -llvmlibc-callee-namespace,\
        -misc-use-anonymous-namespace,\
        -modernize-use-trailing-return-type,\
        -fuchsia-default-arguments-calls,\
        -fuchsia-default-arguments-declarations,\
        -cppcoreguidelines-avoid-magic-numbers,\
        -readability-magic-numbers,\
        -readability-implicit-bool-conversion,\
        -clang-diagnostic-unknown-warning-option,\
        -readability-identifier-length,\
        -llvm-header-guard,\
        -readability-avoid-const-params-in-decls,\
        -readability-redundant-access-specifiers,\
        -google-readability-todo,\
        -misc-include-cleaner,\
        -cppcoreguidelines-pro-type-vararg,\
        -hicpp-vararg,\
        -altera-unroll-loops;-warnings-as-errors=*;-header-filter=.;--exclude-header-filter=ui_\*")
endif()

add_compile_options($<$<CONFIG:Release>:-Wunused>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-pedantic>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Werror=logical-not-parentheses>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wall>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wextra>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wctor-dtor-privacy>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-comment>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wsign-promo>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wcast-align>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wcast-qual>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wdisabled-optimization>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wformat=2>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wmissing-declarations>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wmissing-include-dirs>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wredundant-decls>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wshadow>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wsign-conversion>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wstrict-overflow=2>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wswitch-default>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wundef>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Werror>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Werror=return-type>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Werror=uninitialized>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wstrict-aliasing=2>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-unknown-warning-option>)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wnoexcept>)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wlogical-op>)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wstrict-null-sentinel>)
endif()


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
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets
    Qt6::SerialPort
    Qt6::Multimedia
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
