# UI theme — QML singletons + C++ Quick Controls bootstrap (replaces qtquickcontrols2.conf).

set_source_files_properties(QuickControlsConfig.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(AppColors.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(OperationalStatus.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(AttachDiType.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(AppTheme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(AppTypography.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(central_logger_theme
    URI CentralLogger.Theme
    VERSION 1.0
    STATIC
    QML_FILES
        QuickControlsConfig.qml
        AppColors.qml
        OperationalStatus.qml
        AttachDiType.qml
        AppTheme.qml
        AppTypography.qml
    DEPENDENCIES
        central_logger_core
)

add_library(central_logger_theme_setup STATIC
    ThemeSetup.cpp
)
target_include_directories(central_logger_theme_setup PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(central_logger_theme_setup PUBLIC Qt6::QuickControls2)

add_library(central_logger::theme ALIAS central_logger_theme)
add_library(central_logger::theme_setup ALIAS central_logger_theme_setup)
