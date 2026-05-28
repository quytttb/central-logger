#include "core/AppState.h"
#include "core/DashboardController.h"
#include "core/LoggerDetailViewModel.h"
#include "core/SettingsController.h"
#include "data/db/Database.h"
#include "network/modbus/ModbusBridge.h"
#include "network/modbus/ModbusService.h"
#include "network/modbus/ModbusTypes.h"
#include "network/rest/RestConfigService.h"

#include <QCoreApplication>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QLoggingCategory>
#include <QMetaType>
#include <QQmlApplicationEngine>
#include "ThemeSetup.h"
#include <QString>
#include <QThread>
#include <QtDebug>

using CentralLogger::Core::AppState;
using CentralLogger::Core::DashboardController;
using CentralLogger::Core::LoggerDetailViewModel;
using CentralLogger::Core::SettingsController;
using CentralLogger::Data::Database;
using CentralLogger::Network::LoggerRuntimeConfig;
using CentralLogger::Network::ModbusBridge;
using CentralLogger::Network::ModbusService;
using CentralLogger::Network::PollSnapshot;
using CentralLogger::Network::RestConfigService;

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // Disable the loud warning: "Qt was built without Direct3D 12 support"
    // which MinGW builds of Qt emit because they lack modern Windows SDK headers.
    // This is clean and keeps the console output uncluttered.
    QLoggingCategory::setFilterRules(QStringLiteral("qt.rhi.general.warning=false"));

    // Default to Direct3D 11 for stable DirectX performance on Windows
    if (qgetenv("QSG_RHI_BACKEND").isEmpty()) {
        qputenv("QSG_RHI_BACKEND", "d3d11");
    }
#endif

    CentralLogger::Theme::applyQuickControlsStyle();

    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/CentralLogger/Components/resources/icons/brand_4m_technologies_blue.svg")));
    QCoreApplication::setOrganizationName(QStringLiteral("4M Technologies"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("4mtech.vn"));
    QCoreApplication::setApplicationName(QStringLiteral("Central Logger"));

    // Font path matches qt_add_qml_module(RESOURCES) alias in the generated qrc.
    const QString iconFontPath =
        QStringLiteral(":/qt/qml/CentralLogger/Components/resources/fonts/MaterialSymbols/"
                       "MaterialSymbolsOutlined.ttf");
    const int fontId = QFontDatabase::addApplicationFont(iconFontPath);
    if (fontId < 0) {
        qWarning() << "[main] Failed to load icon font from" << iconFontPath
                   << "— icons will show as boxes";
    } else {
        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty()) {
            qDebug() << "[main] Icon font loaded:" << families.constFirst();
        }
    }

    // Snapshot and runtime-config travel across thread boundaries via
    // queued signals; the meta-system needs them registered.
    qRegisterMetaType<PollSnapshot>("CentralLogger::Network::PollSnapshot");
    qRegisterMetaType<QVector<LoggerRuntimeConfig>>(
        "QVector<CentralLogger::Network::LoggerRuntimeConfig>");

    Database database;
    QString dbError;
    if (!database.open(Database::defaultConnectionName(), Database::defaultPath(), &dbError)) {
        qCritical() << "Failed to open database:" << dbError;
        return -1;
    }

    SettingsController settings;
    settings.setDatabase(&database);
    settings.load();
    SettingsController::setInstance(&settings);

    AppState appState(nullptr);
    appState.setDatabase(&database);
    appState.refreshFromDatabase();
    AppState::setInstance(&appState);

    // Modbus stack: worker on its own thread, bridge on the main thread.
    QThread modbusThread;
    modbusThread.setObjectName(QStringLiteral("ModbusWorker"));
    ModbusService modbusService;
    modbusService.moveToThread(&modbusThread);
    modbusThread.start();

    ModbusBridge bridge;
    bridge.setDatabase(&database);

    QObject::connect(&modbusService, &ModbusService::pollFinished,
                     &bridge,         &ModbusBridge::applySnapshot,
                     Qt::QueuedConnection);

    DashboardController dashboard(nullptr);
    dashboard.setDatabase(&database);
    dashboard.setAppState(&appState);
    dashboard.setSettingsController(&settings);
    dashboard.setModbusBridge(&bridge);
    dashboard.setModbusService(&modbusService);

    QObject::connect(&bridge,    &ModbusBridge::snapshotApplied,
                     &dashboard, &DashboardController::onSnapshotApplied,
                     Qt::QueuedConnection);

    dashboard.reloadLoggers();
    dashboard.startModbusPolling();
    dashboard.purgeOldData();           // Task 16: retention purge on startup
    DashboardController::setInstance(&dashboard);

    // REST service for Logger Detail view-models (per-view instance).
    RestConfigService restConfig;
    restConfig.setDatabase(&database);
    dashboard.setRestConfigService(&restConfig);
    LoggerDetailViewModel::registerServices(&database, &restConfig, &appState, &dashboard);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        QMetaObject::invokeMethod(&modbusService, "shutdown",
                                  Qt::BlockingQueuedConnection);
        modbusThread.quit();
        modbusThread.wait();
    });

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("CentralLogger.App", "Main");

    return app.exec();
}
