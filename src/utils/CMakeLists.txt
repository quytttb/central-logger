# Utils layer — stateless helpers shared by data / core / network layers.
# Rule: no QObject, no QML, no business logic; linkable from any upper layer.

qt_add_library(utils STATIC
    AppConstants.h
    AdapterConstants.h
    Version.h
    DbConstants.h
    UiConstants.h
    FormatConstants.h
    SensorConstants.h
    CsvUtils.h
    time/DateTimeUtils.h
    time/DateTimeUtils.cpp
    network/HostValidator.h
    network/HostValidator.cpp
    os/WindowsFramelessHelper.h
    os/WindowsFramelessHelper.cpp
    events/EventLevels.h
    sensors/AttachDiTypeHelper.h
    sensors/AttachDiTypeHelper.cpp
)

target_include_directories(utils
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(utils
    PUBLIC
        Qt6::Core
    PRIVATE
        Qt6::Network
)

add_library(central_logger::utils ALIAS utils)
