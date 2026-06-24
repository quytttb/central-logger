qt_add_executable(test_dashboard_controller
    test_dashboard_controller.cpp
)

target_link_libraries(test_dashboard_controller
    PRIVATE
        central_logger_core
        data
        Qt6::Test
)

add_test(NAME test_dashboard_controller COMMAND test_dashboard_controller)

qt_add_executable(test_host_validator
    test_host_validator.cpp
)

target_link_libraries(test_host_validator
    PRIVATE
        central_logger_core
        Qt6::Test
)

add_test(NAME test_host_validator COMMAND test_host_validator)

qt_add_executable(test_sensor_merger
    test_sensor_merger.cpp
)

target_link_libraries(test_sensor_merger
    PRIVATE
        central_logger_core
        data
        network
        Qt6::Test
)

add_test(NAME test_sensor_merger COMMAND test_sensor_merger)

qt_add_executable(test_sensor_monitoring_table_model
    test_sensor_monitoring_table_model.cpp
)

target_link_libraries(test_sensor_monitoring_table_model
    PRIVATE
        central_logger_core
        data
        network
        Qt6::Test
)

add_test(NAME test_sensor_monitoring_table_model
         COMMAND test_sensor_monitoring_table_model)

qt_add_executable(test_logger_detail_view_model
    test_logger_detail_view_model.cpp
)

target_link_libraries(test_logger_detail_view_model
    PRIVATE
        central_logger_core
        data
        network
        Qt6::Test
)

add_test(NAME test_logger_detail_view_model
         COMMAND test_logger_detail_view_model)

qt_add_executable(test_recent_events_model
    test_recent_events_model.cpp
)

target_link_libraries(test_recent_events_model
    PRIVATE
        central_logger_core
        data
        network
        Qt6::Test
)

add_test(NAME test_recent_events_model
         COMMAND test_recent_events_model)

qt_add_executable(test_chart_query_service
    test_chart_query_service.cpp
)

target_link_libraries(test_chart_query_service
    PRIVATE
        central_logger_core
        data
        Qt6::Test
)

add_test(NAME test_chart_query_service COMMAND test_chart_query_service)

qt_add_executable(test_poll_history_store
    test_poll_history_store.cpp
)

target_link_libraries(test_poll_history_store
    PRIVATE
        central_logger_core
        data
        network
        Qt6::Test
)

add_test(NAME test_poll_history_store COMMAND test_poll_history_store)

qt_add_executable(test_chart_presentation
    test_chart_presentation.cpp
)

target_link_libraries(test_chart_presentation
    PRIVATE
        central_logger_core
        Qt6::Test
)

add_test(NAME test_chart_presentation COMMAND test_chart_presentation)

# Regression + contract tests for saveLoggerFromForm (REST + DB interaction).
qt_add_executable(test_logger_form_save
    test_logger_form_save.cpp
)

target_link_libraries(test_logger_form_save
    PRIVATE
        central_logger_core
        data
        network
        Qt6::Test
)

add_test(NAME test_logger_form_save COMMAND test_logger_form_save)
