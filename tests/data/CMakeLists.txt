add_central_logger_test(
    NAME    test_database_repositories
    SOURCES test_database_repositories.cpp
    LIBS    data
)

add_central_logger_test(
    NAME    test_retention_purge
    SOURCES test_retention_purge.cpp
    LIBS    data
)

add_central_logger_test(
    NAME    test_database_migrations
    SOURCES test_database_migrations.cpp
    LIBS    data
)

add_central_logger_test(
    NAME    test_wal_contention
    SOURCES test_wal_contention.cpp
    LIBS    data
)
