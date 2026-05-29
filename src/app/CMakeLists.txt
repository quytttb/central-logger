# Desktop shell — QML module URI CentralLogger.App.
# Binary "central_logger", target "app" (so the OUTPUT_NAME does not collide
# with the QML module folder).

qt_add_executable(app
    main.cpp
)

qt_add_qml_module(app
    URI CentralLogger.App
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/views/DashboardView.qml
        qml/views/LoggersView.qml
        qml/views/LoggerDetailView.qml
        qml/views/SettingsView.qml
    DEPENDENCIES
        central_logger_core
        central_logger_theme
        central_logger_components
)

target_include_directories(app PRIVATE
    ${CMAKE_SOURCE_DIR}/src/theme
)

set_target_properties(app PROPERTIES
    OUTPUT_NAME central_logger
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(app
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Network
        Qt6::SerialBus
        Qt6::Graphs
        Qt6::Svg
        central_logger_coreplugin
        central_logger_componentsplugin
        central_logger_themeplugin
        central_logger_theme_setup
        data
        network
)

# Linux/Windows packaging: Qt CMake Deployment API + CPack DEB (see cmake/CPackOptions.cmake)
qt_generate_deploy_app_script(
    TARGET app
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)

install(TARGETS app
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(SCRIPT ${deploy_script})

install(FILES ${CMAKE_SOURCE_DIR}/packaging/linux/central-logger.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)

install(FILES ${CMAKE_SOURCE_DIR}/resources/icons/brand_4m_technologies_blue.svg
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
    RENAME central-logger.svg
)
