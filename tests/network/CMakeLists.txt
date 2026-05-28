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
