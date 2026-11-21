cmake_minimum_required(VERSION 3.16)

project(central_logger VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Single source of truth for build-time versions ---------------------------
# Bump these in one place; everything else (find_package, qt_standard_project_setup,
# packaging scripts, QML module VERSION) reads from them.
set(QT_VERSION_REQUIRED "6.11" CACHE STRING "Minimum Qt version")
set(QML_MODULE_VERSION  "1.0"  CACHE STRING "QML module URI version for qt_add_qml_module")

# Maintenance Tool IDs (Qt 6.11.2, linux_gcc_64) — see README.md § Qt components
#   Essentials: qt.qt6.6112.linux_gcc_64  (Quick, Qml, Sql, Network, Test, …)
#   Modbus:     qtserialbus + qtserialport (SerialBus dependency)
#   Charts UI:  qtgraphs + qtquick3d + qtshadertools (Graphs/Quick3D; not qtcharts — deprecated in 6.11)
#   Qml base:   qttasktree (Qt 6.11 Qml asset downloader dependency)
#
# The patch version and install paths must agree with CI workflows
# (.github/workflows/{ci,dev-build,build-release}.yml), packaging/linux/ci_configure.sh,
# and packaging/windows/{install_qt_aqt.ps1,build_installer.ps1} — update
# both sides together.
find_package(Qt6 ${QT_VERSION_REQUIRED} REQUIRED COMPONENTS
    Quick
    QuickControls2
    Qml
    Sql
    Network
    SerialBus
    Graphs
    Svg
    Test
)

qt_standard_project_setup(REQUIRES ${QT_VERSION_REQUIRED})

# Silence QTP0004 — generate a qmldir per QML source subdirectory so components
# resolve each other via the implicit import.
qt_policy(SET QTP0004 NEW)

# Shared CMake helpers (Qt Test boilerplate, etc.).
include(${CMAKE_SOURCE_DIR}/cmake/CentralLoggerTest.cmake)

# Qt 6.11 qmllint / Qt Creator expect QML module output under the URI target path
# (CentralLogger.Core → …/CentralLogger/Core). Static qt_add_qml_module targets
# otherwise default to CMAKE_CURRENT_BINARY_DIR only (e.g. build/.../src/core).
# Required: source tree (src/...) deviates from URI layout (CentralLogger/...),
# so point the build-tree QML root explicitly; without it qmllint/qmlcachegen
# cannot resolve cross-module imports in this layout.
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" CACHE PATH
    "QML build-tree root; qt_add_qml_module appends URI path segments")

include(GNUInstallDirs)

# Shared UI kit (git submodule) — provides LoggerKit.Theme + LoggerKit.Components.
# Built as STATIC QML modules; version tracks this app's QML module version.
set(LOGGERKIT_QML_MODULE_VERSION ${QML_MODULE_VERSION})
add_subdirectory(shared/logger-ui-kit)

# Qt recommended layout — see cmake-get-started.html
# Layers: kit → utils → data → network (Modbus / REST) → core (C++ singletons) → theme → components → app (+ adapters)
add_subdirectory(src/utils)
add_subdirectory(src/data)
add_subdirectory(src/network)
add_subdirectory(src/core)
add_subdirectory(src/theme)
add_subdirectory(src/components)
add_subdirectory(src/app)

# Aggregate a `qmllint` target across every QML module so CI can lint every
# `.qml` file in one shot via `cmake --build build --target qmllint`.
# Must be declared after all `qt_add_qml_module(...)` subdirectories so each
# `<target>_qmllint` sibling target already exists.
add_custom_target(qmllint)
foreach(_module IN ITEMS
        logger_ui_kit_theme
        logger_ui_kit_components
        central_logger_core
        central_logger_theme
        central_logger_components
        app)
    if(TARGET "${_module}_qmllint")
        add_dependencies(qmllint "${_module}_qmllint")
    endif()
endforeach()

enable_testing()
option(CENTRAL_LOGGER_BUILD_ADAPTERS "Build independent edge adapters" ON)
if(CENTRAL_LOGGER_BUILD_ADAPTERS)
    add_subdirectory(adapters)
endif()
add_subdirectory(tests)

if(UNIX AND NOT APPLE)
    include(${CMAKE_SOURCE_DIR}/cmake/CPackOptions.cmake)
endif()
