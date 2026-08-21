add_central_logger_test(
    NAME    test_modbus_map_parser
    SOURCES test_modbus_map_parser.cpp
    LIBS    network
)

add_central_logger_test(
    NAME    test_rest_config_parser
    SOURCES test_rest_config_parser.cpp
    LIBS    network
)

add_central_logger_test(
    NAME    test_rest_config_service_report
    SOURCES test_rest_config_service_report.cpp
    LIBS    network
)

add_central_logger_test(
    NAME    test_modbus_bridge_batch
    SOURCES test_modbus_bridge_batch.cpp
    LIBS    network data
)

add_central_logger_test(
    NAME    test_history_writer_worker
    SOURCES test_history_writer_worker.cpp
    LIBS    network
)
