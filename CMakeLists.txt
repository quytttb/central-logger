cmake_minimum_required(VERSION 3.16)

project(central_logger VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Maintenance Tool IDs (Qt 6.11.1, linux_gcc_64) — see README.md § Qt components
#   Essentials: qt.qt6.6111.linux_gcc_64  (Quick, Qml, Sql, Network, Test, …)
#   Modbus:     qt.qt6.6111.addons.qtserialbus.linux_gcc_64
#               qt.qt6.6111.addons.qtserialport.linux_gcc_64  (dependency of SerialBus)
#   Charts UI:  qt.qt6.6111.addons.qtgraphs.linux_gcc_64  (not qtcharts — deprecated in 6.11)
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
