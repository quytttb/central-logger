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
    NAME    test_sensor_monitoring_table_model
    SOURCES test_sensor_monitoring_table_model.cpp
    LIBS    central_logger_core data network
)

add_central_logger_test(
    NAME    test_logger_detail_view_model
    SOURCES test_logger_detail_view_model.cpp
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
    NAME    test_chart_presentation
    SOURCES test_chart_presentation.cpp
    LIBS    central_logger_core
)

# Regression + contract tests for saveLoggerFromForm (REST + DB interaction).
add_central_logger_test(
    NAME    test_logger_form_save
    SOURCES test_logger_form_save.cpp
    LIBS    central_logger_core data network
)
