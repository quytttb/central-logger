add_central_logger_test(
    NAME    test_adapter_process_manager
    SOURCES test_adapter_process_manager.cpp
    LIBS    central_logger_core
)

add_central_logger_test(
    NAME    test_status_chip_qml
    SOURCES test_status_chip_qml.cpp
    LIBS    central_logger_componentsplugin central_logger_themeplugin
            central_logger_coreplugin logger_ui_kit_componentsplugin logger_ui_kit_themeplugin
            Qt6::Quick Qt6::QuickControls2
)

add_central_logger_test(
    NAME    test_loggers_refactor_qml
    SOURCES test_loggers_refactor_qml.cpp
    LIBS    central_logger_componentsplugin central_logger_themeplugin
            central_logger_coreplugin logger_ui_kit_componentsplugin logger_ui_kit_themeplugin
            LoggerKit::theme_setup central_logger_core data network
            Qt6::Quick Qt6::QuickControls2
)
target_include_directories(test_loggers_refactor_qml PRIVATE
    ${CMAKE_SOURCE_DIR}/shared/logger-ui-kit/src/theme
)
target_compile_definitions(test_loggers_refactor_qml PRIVATE
    CENTRAL_LOGGER_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_central_logger_test(
    NAME    test_dashboard_controller
    SOURCES test_dashboard_controller.cpp
    LIBS    central_logger_core data
)

add_central_logger_test(
    NAME    test_host_validator
    SOURCES test_host_validator.cpp
    LIBS    central_logger_core
)

add_central_logger_test(
    NAME    test_sensor_merger
    SOURCES test_sensor_merger.cpp
    LIBS    central_logger_core data network
)

add_central_logger_test(
    NAME    test_recent_events_model
    SOURCES test_recent_events_model.cpp
    LIBS    central_logger_core data network
)

add_central_logger_test(
    NAME    test_chart_query_service
    SOURCES test_chart_query_service.cpp
    LIBS    central_logger_core data
)

add_central_logger_test(
    NAME    test_poll_history_store
    SOURCES test_poll_history_store.cpp
    LIBS    central_logger_core data network
)

add_central_logger_test(
    NAME    test_downsampler
    SOURCES test_downsampler.cpp
    LIBS    central_logger_core
)

add_central_logger_test(
    NAME    test_graph_history
    SOURCES test_graph_history.cpp
    LIBS    central_logger_core data
)

add_central_logger_test(
    NAME    test_logger_overview_table_model
    SOURCES test_logger_overview_table_model.cpp
    LIBS    central_logger_core data network
)

add_central_logger_test(
    NAME    test_average_export_service
    SOURCES test_average_export_service.cpp
    LIBS    central_logger_core data
)

add_central_logger_test(
    NAME    test_history_table_model
    SOURCES test_history_table_model.cpp
    LIBS    central_logger_core
)

add_central_logger_test(
    NAME    test_chart_presentation
    SOURCES test_chart_presentation.cpp
    LIBS    central_logger_core
)

# Loopback Add/Edit/Reload regression tests (REST, SQLite and Modbus).
add_central_logger_test(
    NAME    test_logger_form_save
    SOURCES test_logger_form_save.cpp
    LIBS    central_logger_core data network Qt6::SerialBus
)

# Read-only threshold mirror.
add_central_logger_test(
    NAME    test_threshold_controller
    SOURCES test_threshold_controller.cpp
    LIBS    central_logger_core data
)
