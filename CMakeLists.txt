cmake_minimum_required(VERSION 3.16)

project(central_logger VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Maintenance Tool IDs (Qt 6.11.1, linux_gcc_64) — see README.md § Qt components
#   Essentials: qt.qt6.6111.linux_gcc_64  (Quick, Qml, Sql, Network, Test, …)
#   Modbus:     qtserialbus + qtserialport (SerialBus dependency)
#   Charts UI:  qtgraphs + qtquick3d + qtshadertools (Graphs/Quick3D; not qtcharts — deprecated in 6.11)
#   Qml base:   qttasktree (Qt 6.11 Qml asset downloader dependency)
find_package(Qt6 6.11 REQUIRED COMPONENTS
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

qt_standard_project_setup(REQUIRES 6.11)

# Aggregate a `qmllint` target across every QML module so CI can lint every
# `.qml` file in one shot via `cmake --build build --target qmllint`.
# `qt_add_qml_module(...)` below creates per-module `<target>_qmllint`
# siblings; we fold them into one alias.
add_custom_target(qmllint)
foreach(_module IN ITEMS central_logger_components central_logger_theme central_logger_app)
    if(TARGET "${_module}_qmllint")
        add_dependencies(qmllint "${_module}_qmllint")
    endif()
endforeach()

# Qt 6.11 qmllint / Qt Creator expect QML module output under the URI target path
# (CentralLogger.Core → …/CentralLogger/Core). Static qt_add_qml_module targets
# otherwise default to CMAKE_CURRENT_BINARY_DIR only (e.g. build/.../src/core).
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" CACHE PATH
    "QML build-tree root; qt_add_qml_module appends URI path segments")

include(GNUInstallDirs)

# Qt recommended layout — see cmake-get-started.html
# Layers: utils → data → network (Modbus / REST) → core (C++ singletons) → components (reusable QML) → app (shell)
add_subdirectory(src/utils)
add_subdirectory(src/data)
add_subdirectory(src/network)
add_subdirectory(src/core)
add_subdirectory(src/theme)
add_subdirectory(src/components)
add_subdirectory(src/app)

enable_testing()
add_subdirectory(tests)

include(${CMAKE_SOURCE_DIR}/cmake/CPackOptions.cmake)
