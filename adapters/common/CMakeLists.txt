qt_add_library(adapter_v1 STATIC
    EdgeSource.h
    ModbusV1Server.h
    ModbusV1Server.cpp
    RestV1Server.h
    RestV1Server.cpp
)

set_target_properties(adapter_v1 PROPERTIES AUTOMOC ON)
target_include_directories(adapter_v1 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(adapter_v1 PUBLIC utils Qt6::Network Qt6::SerialBus)
