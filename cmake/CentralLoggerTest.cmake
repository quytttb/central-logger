# Helper macro for declaring Qt Test executables in tests/{data,core,network}/CMakeLists.txt.
# Reduces boilerplate: target_link_libraries + Qt6::Test + add_test in one call.
#
# Usage:
#   add_central_logger_test(
#       NAME    test_database_repositories
#       SOURCES test_database_repositories.cpp
#       LIBS    data            # target name; multiple allowed
#   )
#
# After the macro, the test is built as `cmake --build build --target test_<name>`
# and registered with `add_test(NAME <name> COMMAND <name>)`.

function(add_central_logger_test)
    set(options)
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES LIBS)
    cmake_parse_arguments(_ACT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT _ACT_NAME)
        message(FATAL_ERROR "add_central_logger_test: NAME is required")
    endif()
    if(NOT _ACT_SOURCES)
        message(FATAL_ERROR "add_central_logger_test: SOURCES is required for ${_ACT_NAME}")
    endif()

    qt_add_executable(${_ACT_NAME} ${_ACT_SOURCES})

    if(_ACT_LIBS)
        list(APPEND _LINK_LIBS ${_ACT_LIBS})
    endif()
    list(APPEND _LINK_LIBS Qt6::Test)

    target_link_libraries(${_ACT_NAME} PRIVATE ${_LINK_LIBS})
    add_test(NAME ${_ACT_NAME} COMMAND ${_ACT_NAME})
endfunction()
