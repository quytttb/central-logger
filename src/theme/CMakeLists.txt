# UI theme — app-specific QML singletons (chip presentation domain logic).
# Base tokens (AppColors / AppTheme / AppTypography / QuickControlsConfig) +
# ThemeSetup now live in the shared kit (LoggerKit.Theme).

set_source_files_properties(OperationalStatus.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(AttachDiType.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(central_logger_theme
    URI CentralLogger.Theme
    VERSION ${QML_MODULE_VERSION}
    STATIC
    QML_FILES
        OperationalStatus.qml
        AttachDiType.qml
    DEPENDENCIES
        LoggerKit.Theme
)

add_library(central_logger::theme ALIAS central_logger_theme)
