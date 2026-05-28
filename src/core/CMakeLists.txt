# Core layer — C++ singletons/controllers exposed to QML as CentralLogger.Core
# AppState + SettingsController only; no Modbus/REST yet (Task 4/5).
#
# qt_add_qml_module creates the backing target itself (STATIC) so we must
# NOT pre-declare it with qt_add_library.

qt_add_qml_module(central_logger_core
    URI CentralLogger.Core
    VERSION 1.0
    STATIC
    SOURCES
        AppState.h
        AppState.cpp
        SettingsController.h
        SettingsController.cpp
        LoggerListModel.h
        LoggerListModel.cpp
        DashboardController.h
        DashboardController.cpp
        HostValidator.h
        HostValidator.cpp
        LoggerDetailViewModel.h
        LoggerDetailViewModel.cpp
        LoggerSearchProxyModel.h
        LoggerSearchProxyModel.cpp
        sensors/SensorLiveRow.h
        sensors/SensorStatusHelper.h
        sensors/SensorStatusHelper.cpp
        sensors/SensorMerger.h
        sensors/SensorMerger.cpp
        sensors/SensorSnapshotCache.h
        sensors/SensorSnapshotCache.cpp
        sensors/SensorMonitoringTableModel.h
        sensors/SensorMonitoringTableModel.cpp
        events/RecentEventsModel.h
        events/RecentEventsModel.cpp
        charts/ChartQueryService.h
        charts/ChartQueryService.cpp
        charts/ChartPresentation.h
        charts/ChartPresentation.cpp
        charts/PollHistoryStore.h
        charts/PollHistoryStore.cpp
)

target_include_directories(central_logger_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(central_logger_core
    PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::Qml
        data
        network
)

add_library(central_logger::core ALIAS central_logger_core)
