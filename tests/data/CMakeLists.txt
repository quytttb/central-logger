qt_add_executable(test_database_repositories
    test_database_repositories.cpp
)

target_link_libraries(test_database_repositories
    PRIVATE
        data
        Qt6::Test
)

add_test(NAME test_database_repositories COMMAND test_database_repositories)

qt_add_executable(test_retention_purge
    test_retention_purge.cpp
)

target_link_libraries(test_retention_purge
    PRIVATE
        data
        Qt6::Test
)

add_test(NAME test_retention_purge COMMAND test_retention_purge)
