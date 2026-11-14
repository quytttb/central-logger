qt_add_library(osg_source STATIC OsgSource.h OsgSource.cpp)
set_target_properties(osg_source PROPERTIES AUTOMOC ON)
target_link_libraries(osg_source PUBLIC adapter_v1 Qt6::Network)

qt_add_executable(osg_adapter main.cpp)
target_link_libraries(osg_adapter PRIVATE osg_source adapter_v1)

add_central_logger_test(
    NAME test_osg_adapter
    SOURCES test_osg_adapter.cpp
    LIBS osg_source adapter_v1 network
)
