# Data layer static library — see docs/adr/0001-db.md

set(DATA_DB_FILES
    db/Database.cpp
    db/Database.h
)

set(DATA_MODEL_FILES
    models/AppSettings.h
    models/HistoryRow.h
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

set(DB_MIGRATION_FILES
    db/migrations/002_logger_sensor_attach_di.sql
    db/migrations/003_logger_sensor_all_parents.sql
    db/migrations/004_app_settings_history_flush.sql
    db/migrations/005_drop_maintenance_mode.sql
    db/migrations/006_logger_sensor_decimals.sql
)
foreach(mig ${DB_MIGRATION_FILES})
    get_filename_component(mig_alias ${mig} NAME)
    set_source_files_properties(${mig} PROPERTIES QT_RESOURCE_ALIAS "${mig_alias}")
endforeach()
qt_add_resources(data "db_migrations"
    PREFIX "/db/migrations"
    FILES
        ${DB_MIGRATION_FILES}
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
