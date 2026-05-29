# Data layer static library — see docs/adr/0001-db.md

set(DATA_DB_FILES
    db/Database.cpp
    db/Database.h
)

set(DATA_MODEL_FILES
    models/AppSettings.h
    models/LoggerInfo.h
    models/LoggerSensor.h
    models/SensorReading.h
    models/SystemEvent.h
)

set(DATA_REPOSITORY_FILES
    repositories/EventRepository.cpp
    repositories/EventRepository.h
    repositories/LoggerRepository.cpp
    repositories/LoggerRepository.h
    repositories/SensorCatalogRepository.cpp
    repositories/SensorCatalogRepository.h
    repositories/SensorReadingRepository.cpp
    repositories/SensorReadingRepository.h
    repositories/SettingsRepository.cpp
    repositories/SettingsRepository.h
)

qt_add_library(data STATIC
    ${DATA_DB_FILES}
    ${DATA_MODEL_FILES}
    ${DATA_REPOSITORY_FILES}
)

add_library(central_logger::data ALIAS data)

# Keep db/, models/, repositories/ as sibling folders in Qt Creator / VS.
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
             PREFIX ""
             FILES
                 ${DATA_DB_FILES}
                 ${DATA_MODEL_FILES}
                 ${DATA_REPOSITORY_FILES})

set_source_files_properties(db/schema/001_initial.sql
    PROPERTIES QT_RESOURCE_ALIAS "001_initial.sql")
qt_add_resources(data "db_schema"
    PREFIX "/db/schema"
    FILES
        db/schema/001_initial.sql
)

target_include_directories(data
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(data
    PUBLIC
        Qt6::Core
        Qt6::Sql
        utils
)
