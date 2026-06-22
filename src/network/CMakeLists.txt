# Network layer — Modbus TCP + REST per docs/contracts/.

qt_add_library(network STATIC
    modbus/ModbusMapParser.h
    modbus/ModbusPollPlan.h
    modbus/ModbusTypes.h
    modbus/ModbusBridge.h
    modbus/ModbusBridge.cpp
    modbus/ModbusDataDispatcher.h
    modbus/ModbusDataDispatcher.cpp
    modbus/ModbusService.h
    modbus/ModbusService.cpp
    workers/HistoryWriterWorker.h
    workers/HistoryWriterWorker.cpp
    rest/RestConfigParser.h
    rest/RestConfigService.h
    rest/RestConfigService.cpp
)

set_target_properties(network PROPERTIES
    AUTOMOC ON
)

target_include_directories(network
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(network
    PUBLIC
        Qt6::Core
        Qt6::SerialBus
        Qt6::Network
        data
)

add_library(central_logger::network ALIAS network)
