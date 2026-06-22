qt_add_executable(test_modbus_map_parser
    test_modbus_map_parser.cpp
)

target_link_libraries(test_modbus_map_parser
    PRIVATE
        network
        Qt6::Test
)

add_test(NAME test_modbus_map_parser COMMAND test_modbus_map_parser)

qt_add_executable(test_rest_config_parser
    test_rest_config_parser.cpp
)

target_link_libraries(test_rest_config_parser
    PRIVATE
        network
        Qt6::Test
)

add_test(NAME test_rest_config_parser COMMAND test_rest_config_parser)

qt_add_executable(test_rest_config_service_report
    test_rest_config_service_report.cpp
)

target_link_libraries(test_rest_config_service_report
    PRIVATE
        network
        Qt6::Test
)

add_test(NAME test_rest_config_service_report COMMAND test_rest_config_service_report)

qt_add_executable(test_modbus_bridge_batch
    test_modbus_bridge_batch.cpp
)

target_link_libraries(test_modbus_bridge_batch
    PRIVATE
        network
        data
        Qt6::Test
)

add_test(NAME test_modbus_bridge_batch COMMAND test_modbus_bridge_batch)

qt_add_executable(test_history_writer_worker
    test_history_writer_worker.cpp
)

target_link_libraries(test_history_writer_worker
    PRIVATE
        network
        Qt6::Test
)

add_test(NAME test_history_writer_worker COMMAND test_history_writer_worker)
