#include "core/AppState.h"
#include "core/DashboardController.h"
#include "core/LoggerFormController.h"
#include "core/LoggerDetailViewModel.h"
#include "core/SettingsController.h"
#include "core/history/HistoryViewModel.h"
#include "data/db/Database.h"
#include "network/modbus/ModbusBridge.h"
#include "network/modbus/ModbusDataDispatcher.h"
#include "network/modbus/ModbusService.h"
#include "network/modbus/ModbusTypes.h"
#include "network/workers/HistoryWriterWorker.h"
#include "network/rest/RestConfigService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QMetaType>
#include <QMutex>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include "ThemeSetup.h"
#include "utils/os/WindowsFramelessHelper.h"
#include <QString>
#include <QThread>
#include <QtDebug>

using CentralLogger::Core::AppState;
using CentralLogger::Core::DashboardController;
using CentralLogger::Core::LoggerFormController;
using CentralLogger::Core::LoggerDetailViewModel;
using CentralLogger::Core::SettingsController;
using CentralLogger::Data::Database;
using CentralLogger::Network::LoggerRuntimeConfig;
using CentralLogger::Network::HistoryWriterWorker;
using CentralLogger::Network::ModbusBridge;
using CentralLogger::Network::ModbusDataDispatcher;
using CentralLogger::Network::ModbusService;
using CentralLogger::Network::PollSnapshot;
using CentralLogger::Network::RestConfigService;

// ---------------------------------------------------------------------------
// File-based message handler
// ---------------------------------------------------------------------------

namespace {

QFile  *g_logFile  = nullptr;
QMutex  g_logMutex;

void fileMessageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    const char *level = "DEBUG";
    switch (type) {
    case QtDebugMsg:    level = "DEBUG";    break;
    case QtInfoMsg:     level = "INFO";     break;
    case QtWarningMsg:  level = "WARNING";  break;
    case QtCriticalMsg: level = "CRITICAL"; break;
    case QtFatalMsg:    level = "FATAL";    break;
    }
    const QByteArray line =
        (QDateTime::currentDateTime().toString(Qt::ISODate)
         + QStringLiteral(" [") + QLatin1String(level) + QStringLiteral("] ")
         + msg + QLatin1Char('\n')).toUtf8();

    QMutexLocker lock(&g_logMutex);
    if (g_logFile && g_logFile->isOpen()) {
        g_logFile->write(line);
        g_logFile->flush();
    }
    fprintf(stderr, "%s", line.constData());
    if (type == QtFatalMsg)
        abort();
}

// Returns the path where the log file will be created, for display in the UI.
QString initFileLogging()
{
    const QString dataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    const QString logPath = dataDir + QStringLiteral("/central-logger.log");

    // Rotate: if the current log exceeds 5 MB, rename it to .log.old.
    {
        QFileInfo fi(logPath);
        if (fi.exists() && fi.size() > 5 * 1024 * 1024) {
            QFile::remove(logPath + QStringLiteral(".old"));
            QFile::rename(logPath, logPath + QStringLiteral(".old"));
        }
    }

    g_logFile = new QFile(logPath);
    if (!g_logFile->open(QIODevice::Append | QIODevice::Text)) {
        delete g_logFile;
        g_logFile = nullptr;
        return {};
    }
    qInstallMessageHandler(fileMessageHandler);
    return logPath;
}

} // namespace

// ---------------------------------------------------------------------------

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // Force Direct3D 11 as the default RHI backend on Windows if none is specified.
    // This avoids the "Qt was built without Direct3D 12 support" warning on systems
    // where Qt was built without modern Direct3D 12 SDK headers (e.g. MinGW builds).
    if (qgetenv("QSG_RHI_BACKEND").isEmpty()) {
        qputenv("QSG_RHI_BACKEND", "d3d11");
    }
#endif

    CentralLogger::Theme::applyQuickControlsStyle();

    QGuiApplication app(argc, argv);
    
    static WindowsFramelessHelper framelessHelper;
    app.installNativeEventFilter(&framelessHelper);

    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/CentralLogger/Components/resources/icons/brand_4m_technologies_blue.svg")));
    QCoreApplication::setOrganizationName(QStringLiteral("4M Technologies"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("4mtech.vn"));
    QCoreApplication::setApplicationName(QStringLiteral("Central Logger"));

    const QString logFilePath = initFileLogging();
    qInfo() << "Central Logger starting — log:" << logFilePath;

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
    const QString dbPath = Database::defaultPath();
    if (!database.open(Database::defaultConnectionName(), dbPath, &dbError)) {
        qCritical() << "Failed to open database:" << dbError;

        SettingsController fatalSettings(nullptr);
        fatalSettings.setTheme(QStringLiteral("dark"));
        SettingsController::setInstance(&fatalSettings);

        const QString errorKind = dbError.contains(QStringLiteral("Incompatible"),
                                                   Qt::CaseInsensitive)
                                      ? QStringLiteral("newer_than_app")
                                      : QStringLiteral("migrate_fail");

        QQmlApplicationEngine engine;
        engine.setInitialProperties({
            {QStringLiteral("errorMessage"), dbError},
            {QStringLiteral("dbPath"),       dbPath},
            {QStringLiteral("backupPath"),   dbPath + QStringLiteral(".bak")},
            {QStringLiteral("errorKind"),    errorKind},
        });
        QObject::connect(
            &engine,
            &QQmlApplicationEngine::objectCreationFailed,
            &app,
            []() { QCoreApplication::exit(1); },
            Qt::QueuedConnection);
        engine.loadFromModule(QStringLiteral("CentralLogger.App"), QStringLiteral("FatalStartup"));
        return app.exec();
    }

    SettingsController settings(nullptr);
    settings.setDatabase(&database);
    settings.load();
    settings.setLogFilePath(logFilePath);
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

    ModbusDataDispatcher dispatcher;

    QThread historyThread;
    historyThread.setObjectName(QStringLiteral("HistoryWriter"));
    HistoryWriterWorker historyWorker;
    historyWorker.setDatabasePath(database.connection().databaseName());
    historyWorker.setFlushIntervalSeconds(settings.historyFlushIntervalS());
    historyWorker.moveToThread(&historyThread);
    QObject::connect(&historyThread, &QThread::started,
                     &historyWorker, &HistoryWriterWorker::start);
    historyThread.start();

    dispatcher.setHistoryWriter(&historyWorker);

    QObject::connect(&settings, &SettingsController::saved, [&]() {
        historyWorker.setFlushIntervalSeconds(settings.historyFlushIntervalS());
    });

    QObject::connect(&modbusService, &ModbusService::pollFinished,
                     &dispatcher,     &ModbusDataDispatcher::onPollFinished,
                     Qt::QueuedConnection);
    QObject::connect(&dispatcher, &ModbusDataDispatcher::liveSnapshotReady,
                     &bridge,     &ModbusBridge::applyLiveSnapshot,
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

    // Logger Add/Edit/Remove + REST config probing (split from dashboard).
    LoggerFormController loggerForm(nullptr);
    loggerForm.setDatabase(&database);
    loggerForm.setRestConfigService(&restConfig);
    loggerForm.setDashboardController(&dashboard);
    LoggerFormController::setInstance(&loggerForm);

    LoggerDetailViewModel::registerServices(&database, &restConfig, &appState, &dashboard);
    CentralLogger::Core::HistoryViewModel::registerDatabase(&database);
    CentralLogger::Core::HistoryViewModel::registerHistoryWriter(&historyWorker);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        QMetaObject::invokeMethod(&modbusService, "shutdown",
                                  Qt::BlockingQueuedConnection);
        modbusThread.quit();
        modbusThread.wait();

        historyWorker.shutdown();
        historyThread.quit();
        historyThread.wait();
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
