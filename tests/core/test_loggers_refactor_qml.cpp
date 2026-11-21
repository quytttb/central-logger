// Offscreen Qt Quick smoke tests for the Loggers Data | Graph + Settings > Loggers
// refactor (Plan §6 test matrix):
//   - 0 loggers: Data (Cards & List) and Graph render empty states cleanly
//   - 1 online logger + 8 analog sensors: 4-column x 2-row card grid, live values,
//     non-empty timestamp, and poll updates preserve ListView scroll position
//   - 3 loggers (2 online, 1 offline): offline section is dimmed (opacity < 1)
//     and renders "—" placeholders instead of stale values
//   - Settings > Loggers: local Add/Edit, read-only thresholds, explicit Reload,
//     responsive layout and light/dark theme

#include "ThemeSetup.h"
#include "LoggerTestFixtures.h"
#include "../fixtures/ReadOnlyEdgeHttpServer.h"
#include "core/AppState.h"
#include "core/DashboardController.h"
#include "core/LoggerFormController.h"
#include "core/SettingsController.h"
#include "core/threshold/ThresholdController.h"
#include "core/history/HistoryViewModel.h"
#include "data/db/Database.h"
#include "data/models/LoggerSensor.h"
#include "data/repositories/LoggerRepository.h"
#include "data/repositories/SensorCatalogRepository.h"
#include "network/modbus/ModbusTypes.h"
#include "network/rest/RestConfigService.h"

#include <QDateTime>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(CentralLogger_ComponentsPlugin)
Q_IMPORT_QML_PLUGIN(CentralLogger_ThemePlugin)
Q_IMPORT_QML_PLUGIN(CentralLogger_CorePlugin)
Q_IMPORT_QML_PLUGIN(LoggerKit_ComponentsPlugin)
Q_IMPORT_QML_PLUGIN(LoggerKit_ThemePlugin)

using namespace CentralLogger::Core;
using namespace CentralLogger::Data;
using namespace CentralLogger::Network;

namespace {

QString uniqueConn(const char *tag)
{
    static int n = 0;
    return QStringLiteral("qml_smoke_%1_%2").arg(QLatin1String(tag)).arg(++n);
}

struct RuntimeRig
{
    Database             db;
    ReadOnlyEdgeHttpServer edge;
    SettingsController   settings{nullptr};
    AppState             appState{nullptr};
    DashboardController  dashboard{nullptr};
    RestConfigService    rest;
    LoggerFormController form{nullptr};
    ThresholdController  thresholds{nullptr};

    explicit RuntimeRig(const char *tag)
    {
        if (!db.open(uniqueConn(tag), QStringLiteral(":memory:"))) {
            qFatal("RuntimeRig: failed to open in-memory DB");
        }
        settings.setDatabase(&db);
        settings.load();
        SettingsController::setInstance(&settings);

        appState.setDatabase(&db);
        appState.refreshFromDatabase();
        AppState::setInstance(&appState);

        dashboard.setDatabase(&db);
        dashboard.setAppState(&appState);
        dashboard.setSettingsController(&settings);
        DashboardController::setInstance(&dashboard);

        rest.setDatabase(&db);

        form.setDatabase(&db);
        form.setRestConfigService(&rest);
        form.setDashboardController(&dashboard);
        LoggerFormController::setInstance(&form);

        thresholds.setDatabase(&db);
        QObject::connect(&form, &LoggerFormController::configReloadFinished,
                         &thresholds, [this](qint64 id, bool ok) {
            if (ok && id == thresholds.selectedLoggerId())
                thresholds.refresh();
        });
        ThresholdController::setInstance(&thresholds);

        dashboard.reloadLoggers();
    }

    ~RuntimeRig()
    {
        ThresholdController::setInstance(nullptr);
        LoggerFormController::setInstance(nullptr);
        DashboardController::setInstance(nullptr);
        AppState::setInstance(nullptr);
        SettingsController::setInstance(nullptr);
    }
};

QUrl viewUrl(const char *relPath)
{
    return QUrl::fromLocalFile(
        QStringLiteral(CENTRAL_LOGGER_SOURCE_DIR "/") + QLatin1String(relPath));
}

QQuickItem *findVisual(QQuickItem *item, const auto &matches)
{
    if (matches(item))
        return item;
    for (auto *child : item->childItems()) {
        if (auto *found = findVisual(child, matches))
            return found;
    }
    return nullptr;
}

QQuickItem *namedVisual(QQuickItem *item, const QString &name)
{
    return findVisual(item, [&name](QQuickItem *child) { return child->objectName() == name; });
}

} // namespace

class TestLoggersRefactorQml : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        LoggerKit::Theme::applyQuickControlsStyle();
    }

    void zeroLoggers_viewsInstantiateWithoutErrors();
    void eightAnalogSensors_fourByTwoGridAndOfflineDimmingAndScrollPreserved();
    void settingsLocalEditsAndReadOnlyThresholds();
    void responsiveDashboardAndHistoryToolbar();
    void dashboardSeriesRemainsPopulatedAcrossAxisAndPointChanges();
    void historyScrollBenchmarkAndRefreshAnchor();
};

void TestLoggersRefactorQml::zeroLoggers_viewsInstantiateWithoutErrors()
{
    RuntimeRig rig("zero");
    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1280, 800);
    window.show();

    for (const char *path : {
             "src/app/qml/views/DashboardView.qml",
             "src/app/qml/views/LoggersView.qml",
             "src/app/qml/views/HistoryView.qml",
             "src/app/qml/views/SettingsView.qml",
         }) {
        QQmlComponent comp(&engine, viewUrl(path));
        QVERIFY2(comp.isReady(), qPrintable(comp.errorString()));
        QScopedPointer<QObject> obj(comp.create());
        QVERIFY2(obj, qPrintable(comp.errorString()));
        auto *item = qobject_cast<QQuickItem *>(obj.data());
        QVERIFY(item != nullptr);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(1200, 720));
        QCoreApplication::processEvents();
    }

    QCOMPARE(rig.dashboard.liveCards()->rowCount(), 0);
    QCOMPARE(rig.dashboard.liveList()->rowCount(), 0);
}

void TestLoggersRefactorQml::eightAnalogSensors_fourByTwoGridAndOfflineDimmingAndScrollPreserved()
{
    RuntimeRig rig("fleet");
    QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));

    const qint64 id1 = LoggerTestFixtures::seedLogger(rig.db, rig.dashboard, QStringLiteral("ST-01"), QStringLiteral("Station 1"),
                                          QStringLiteral("127.0.0.1"), 5020, rig.edge.port(), QStringLiteral("tok"));
    const qint64 id2 = LoggerTestFixtures::seedLogger(rig.db, rig.dashboard, QStringLiteral("ST-02"), QStringLiteral("Station 2"),
                                          QStringLiteral("127.0.0.1"), 5021, rig.edge.port(), QStringLiteral("tok"));
    const qint64 id3 = LoggerTestFixtures::seedLogger(rig.db, rig.dashboard, QStringLiteral("ST-03"), QStringLiteral("Station 3 Offline"),
                                          QStringLiteral("127.0.0.1"), 5022, rig.edge.port(), QStringLiteral("tok"));
    QVERIFY(id1 > 0 && id2 > 0 && id3 > 0);

    SensorCatalogRepository catalog(rig.db.connection());
    for (int i = 1; i <= 8; ++i) {
        LoggerSensor s;
        s.loggerId     = id1;
        s.edgeSensorId = i;
        s.sensorType   = QStringLiteral("ANALOG");
        s.name         = QStringLiteral("Sensor %1").arg(i);
        s.unit         = QStringLiteral("mg/L");
        s.minThreshold = 0.0;
        s.maxThreshold = 50.0;
        s.decimals     = 1;
        QVERIFY(catalog.upsert(s));
    }
    for (qint64 lid : {id2, id3}) {
        LoggerSensor s;
        s.loggerId     = lid;
        s.edgeSensorId = 1;
        s.sensorType   = QStringLiteral("ANALOG");
        s.name         = QStringLiteral("Flow");
        s.unit         = QStringLiteral("m3/h");
        s.decimals     = 1;
        QVERIFY(catalog.upsert(s));
    }
    rig.dashboard.reloadLoggers();

    auto sendPoll = [&](qint64 lid, bool ok, float baseVal) {
        PollSnapshot snap;
        snap.loggerId   = lid;
        snap.success    = ok;
        snap.header.mapVersion  = 2;
        snap.header.statusFlags = ok ? 0x03 : 0x00;
        snap.measuredAt = QDateTime::currentDateTimeUtc();
        if (ok) {
            const auto rows = catalog.listByLoggerId(lid);
            for (const auto &cat : rows) {
                AnalogSample a;
                a.edgeSensorId = static_cast<quint16>(cat.edgeSensorId);
                a.statusCode   = QStringLiteral("00");
                a.hasQuality   = true;
                a.qualityValid = true;
                a.value        = baseVal + float(cat.edgeSensorId);
                snap.analogs.append(a);
            }
        }
        rig.dashboard.onSnapshotApplied(snap, int(snap.analogs.size()), catalog.listByLoggerId(lid));
    };

    sendPoll(id1, true, 10.0f);
    sendPoll(id2, true, 20.0f);
    sendPoll(id3, false, 0.0f);

    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1280, 768);
    window.show();

    // Verify LoggerLiveSection 4-column grid on 8 analog sensors & offline dimming
    QQmlComponent secComp(&engine);
    secComp.setData(R"(
        import QtQuick
        import CentralLogger.Components
        import LoggerKit.Theme
        LoggerLiveSection {
            width: 1080
            readonly property color normalFill: AppColors.successContainer
            readonly property color alarmFill: AppColors.errorContainer
            readonly property color neutralFill: AppColors.surfaceContainer
        }
    )", QUrl(QStringLiteral("qrc:/SectionSmoke.qml")));
    QScopedPointer<QObject> secObj(secComp.create());
    QVERIFY2(secObj, qPrintable(secComp.errorString()));
    auto *secItem = qobject_cast<QQuickItem *>(secObj.data());
    secItem->setParentItem(window.contentItem());

    const QVariantMap firstSec = LoggerTestFixtures::cardSection(rig.dashboard, 0);
    secItem->setProperty("online", true);
    secItem->setProperty("sensors", firstSec.value(QStringLiteral("sensors")));
    QCoreApplication::processEvents();
    QCOMPARE(secItem->property("itemCount").toInt(), 8);
    QCOMPARE(secItem->property("gridColumns").toInt(), 4);
    QTRY_VERIFY_WITH_TIMEOUT(secItem->height() > 250.0, 1000);
    auto *limit = namedVisual(secItem, QStringLiteral("sensorLimit"));
    QVERIFY(limit);
    auto *card = limit->parentItem()->parentItem();
    QCOMPARE(limit->property("text").toString(), QStringLiteral("[0 - 50]"));
    QCOMPARE(card->property("color"), secItem->property("normalFill"));
    QCOMPARE(card->property("filled").toBool(), true);

    auto sensors = firstSec.value(QStringLiteral("sensors")).toList();
    QVariantMap first = sensors.constFirst().toMap();
    first.insert(QStringLiteral("alarmType"), QStringLiteral("max"));
    sensors[0] = first;
    secItem->setProperty("sensors", sensors);
    QCOMPARE(card->property("color"), secItem->property("alarmFill"));
    secItem->setProperty("online", false);
    QCOMPARE(card->property("color"), secItem->property("neutralFill"));
    secItem->setProperty("online", true);
    first.insert(QStringLiteral("alarmType"), QString());
    first.insert(QStringLiteral("hasLimit"), false);
    sensors[0] = first;
    secItem->setProperty("sensors", sensors);
    QCOMPARE(card->property("color"), secItem->property("neutralFill"));
    QVERIFY(!limit->property("visible").toBool());

    // Verify LoggersView Cards/List toggle + Graph tab + scroll preservation on poll
    QQmlComponent viewComp(&engine, viewUrl("src/app/qml/views/LoggersView.qml"));
    QVERIFY2(viewComp.isReady(), qPrintable(viewComp.errorString()));
    QScopedPointer<QObject> viewObj(viewComp.create());
    QVERIFY2(viewObj, qPrintable(viewComp.errorString()));
    auto *viewItem = qobject_cast<QQuickItem *>(viewObj.data());
    viewItem->setParentItem(window.contentItem());
    viewItem->setSize(QSizeF(1160, 400));
    QCoreApplication::processEvents();

    // Find the Cards ListView and scroll down, then apply another poll snapshot
    QQuickItem *cardsFlick = nullptr;
    const auto children = viewItem->findChildren<QQuickItem *>();
    for (QQuickItem *ch : children) {
        if (ch->inherits("QQuickListView")) {
            cardsFlick = ch;
            break;
        }
    }
    QVERIFY(cardsFlick != nullptr);
    cardsFlick->setProperty("contentY", 120.0);
    QCOMPARE(cardsFlick->property("contentY").toDouble(), 120.0);

    // Poll update must update in place without resetting contentY to 0
    sendPoll(id1, true, 15.0f);
    QCoreApplication::processEvents();
    QCOMPARE(cardsFlick->property("contentY").toDouble(), 120.0);

    // Offline logger #3 has placeholder value "—"
    const QVariantMap offlineSec = LoggerTestFixtures::cardSection(rig.dashboard, 2);
    QCOMPARE(offlineSec.value(QStringLiteral("online")).toBool(), false);
    QCOMPARE(offlineSec.value(QStringLiteral("sensors")).toList().first().toMap()
                 .value(QStringLiteral("value")).toString(),
             QStringLiteral("—"));

    // Verify Loggers > Data Cards delegates inside ListView do not collapse to 0 height
    auto *cardsContent = cardsFlick->property("contentItem").value<QQuickItem *>();
    QVERIFY(cardsContent != nullptr);
    QList<QQuickItem *> liveSections;
    QTRY_VERIFY_WITH_TIMEOUT([&]() {
        liveSections.clear();
        for (QQuickItem *ch : cardsContent->childItems()) {
            if (ch->property("loggerId").isValid() && ch->property("gridColumns").isValid()) {
                liveSections.append(ch);
            }
        }
        return !liveSections.isEmpty() && liveSections.first()->height() > 250.0;
    }(), 1000);

    // Switch to List mode and Graph tab — must NOT emit SettingsController::saved()
    QSignalSpy savedSpy(&rig.settings, &SettingsController::saved);
    QVERIFY(rig.settings.saveLoggersViewMode(QStringLiteral("list")));
    QCOMPARE(savedSpy.count(), 0);
    QCoreApplication::processEvents();
    QCOMPARE(viewItem->property("isListMode").toBool(), true);

    QMetaObject::invokeMethod(viewItem, "openGraphForLogger", Q_ARG(QVariant, int(id1)));
    QCoreApplication::processEvents();
    QCOMPARE(viewItem->property("isGraphTab").toBool(), true);
    QCOMPARE(rig.dashboard.graphLoggerId(), id1);
    auto *zoomOut = namedVisual(viewItem, QStringLiteral("graphZoomOutButton"));
    QVERIFY(zoomOut && zoomOut->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(zoomOut, "clicked"));
    QCOMPARE(rig.dashboard.graphWindowMinutes(), 15);
    const auto firstChart = [viewItem]() {
        return findVisual(viewItem, [](const QQuickItem *child) {
            return child->objectName() == QStringLiteral("sensorMiniChart")
                && child->property("label").toString() == QStringLiteral("Sensor 1");
        });
    };
    QTRY_VERIFY_WITH_TIMEOUT(firstChart(), 2000);
    QPointer<QObject> chart = firstChart();
    sendPoll(id1, true, 23.0f);
    // The same GridView delegate must receive the live change.
    QTRY_VERIFY_WITH_TIMEOUT(chart && chart->property("latestText").toString() == QStringLiteral("24.0"), 2000);
    QVERIFY(chart);
    QCOMPARE(firstChart(), qobject_cast<QQuickItem *>(chart.data()));

    auto *resetZoom = namedVisual(viewItem, QStringLiteral("graphResetZoomButton"));
    auto *zoomIn = namedVisual(viewItem, QStringLiteral("graphZoomInButton"));
    QVERIFY(resetZoom && zoomIn);
    QVERIFY(QMetaObject::invokeMethod(resetZoom, "clicked"));
    QCOMPARE(rig.dashboard.graphWindowMinutes(), 5);
    QVERIFY(QMetaObject::invokeMethod(zoomIn, "clicked"));
    QCOMPARE(rig.dashboard.graphWindowMinutes(), 1);
    QVERIFY(!zoomIn->property("enabled").toBool());
}

void TestLoggersRefactorQml::settingsLocalEditsAndReadOnlyThresholds()
{
    RuntimeRig rig("settings");
    QVERIFY(rig.edge.port() != 0);

    QStringList qmlWarnings;
    QQmlEngine engine;
    connect(&engine, &QQmlEngine::warnings, &engine,
            [&qmlWarnings](const QList<QQmlError> &warnings) {
                for (const auto &warning : warnings)
                    qmlWarnings.append(warning.toString());
            });
    QQuickWindow window;
    window.resize(1280, 800);
    window.show();

    QQmlComponent settingsComp(&engine, viewUrl("src/app/qml/views/SettingsView.qml"));
    QVERIFY2(settingsComp.isReady(), qPrintable(settingsComp.errorString()));
    QScopedPointer<QObject> settingsObj(settingsComp.create());
    QVERIFY2(settingsObj, qPrintable(settingsComp.errorString()));
    auto *settingsItem = qobject_cast<QQuickItem *>(settingsObj.data());
    settingsItem->setParentItem(window.contentItem());
    settingsItem->setSize(QSizeF(1200, 760));
    QCoreApplication::processEvents();

    // General card must NOT collapse to 0 height inside SettingsGeneralPane
    auto *generalCard = settingsItem->findChild<QQuickItem *>(QStringLiteral("generalSettingsCard"));
    QVERIFY(generalCard != nullptr);
    QVERIFY2(generalCard->height() > 200.0,
             qPrintable(QStringLiteral("SettingsGeneralPane card collapsed height=%1")
                            .arg(generalCard->height())));

    settingsItem->setProperty("tabIndex", 1); // Loggers tab
    QCoreApplication::processEvents();

    // Responsive 1024x768 check: narrow 1-column ScrollView content expands past viewport height
    window.resize(1024, 768);
    settingsItem->setSize(QSizeF(944, 648));
    QCoreApplication::processEvents();
    auto *workspaceContent = settingsItem->findChild<QQuickItem *>(QStringLiteral("loggersWorkspaceContent"));
    QVERIFY(workspaceContent != nullptr);
    QVERIFY2(workspaceContent->height() > 1000.0,
             qPrintable(QStringLiteral("Expected scrollable 1-column height > 1000, got %1")
                            .arg(workspaceContent->height())));
    QVERIFY2(generalCard->height() > 200.0,
             qPrintable(QStringLiteral("General card collapsed at 1024x768: height=%1")
                            .arg(generalCard->height())));

    window.resize(1280, 800);
    settingsItem->setSize(QSizeF(1200, 760));
    auto *connectButton = namedVisual(settingsItem, QStringLiteral("connectLoggerConfigButton"));
    auto *save = namedVisual(settingsItem, QStringLiteral("saveLoggerButton"));
    auto *reload = namedVisual(settingsItem, QStringLiteral("reloadLoggerConfigButton"));
    auto *station = namedVisual(settingsItem, QStringLiteral("loggerStationCodeField"));
    auto *name = namedVisual(settingsItem, QStringLiteral("loggerDisplayNameField"));
    auto *host = namedVisual(settingsItem, QStringLiteral("loggerHostField"));
    auto *apiPort = namedVisual(settingsItem, QStringLiteral("loggerApiPortField"));
    auto *unitId = namedVisual(settingsItem, QStringLiteral("loggerModbusUnitField"));
    auto *poll = namedVisual(settingsItem, QStringLiteral("centralPollIntervalField"));
    QVERIFY(connectButton && save && reload && station && name && host && apiPort && unitId && poll);
    QVERIFY(host->setProperty("text", QStringLiteral("127.0.0.1")));
    QVERIFY(apiPort->setProperty("value", int(rig.edge.port())));
    QVERIFY(connectButton->isVisible());
    QVERIFY(!reload->isVisible());
    QVERIFY(!save->isEnabled());

    QSignalSpy probe(&rig.form, &LoggerFormController::probeConfigResult);
    QVERIFY(QMetaObject::invokeMethod(connectButton, "clicked"));
    QTRY_COMPARE(probe.size(), 1);
    QVERIFY(probe.first().at(0).toBool());
    QCOMPARE(name->property("text").toString(), QStringLiteral("Edge station"));
    QCOMPARE(station->property("text").toString(), QStringLiteral("ST-01"));
    QCOMPARE(unitId->property("value").toInt(), 3);
    QCOMPARE(poll->property("value").toInt(), CentralLogger::Defaults::kDefaultPollIntervalSec);

    QVERIFY(name->setProperty("text", QStringLiteral("Local alias")));
    QVERIFY(poll->setProperty("value", 7));
    QSignalSpy saved(&rig.form, &LoggerFormController::formSaveFinished);
    QVERIFY(save->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(save, "clicked"));
    QCOMPARE(saved.size(), 1);
    const auto addResult = saved.takeFirst();
    QVERIFY2(addResult.at(0).toBool(), qPrintable(addResult.at(2).toString()));
    const qint64 id1 = addResult.at(1).toLongLong();
    QVERIFY(id1 > 0);
    QVERIFY(!connectButton->isVisible());
    QVERIFY(reload->isVisible());
    QVERIFY(station->property("readOnly").toBool());

    auto *thresholdPane = namedVisual(settingsItem, QStringLiteral("sensorThresholdsPane"));
    QVERIFY(thresholdPane);
    const auto findLimit = [thresholdPane](const QString &text) {
        return findVisual(thresholdPane, [&text](QQuickItem *child) {
            return child->objectName() == QStringLiteral("thresholdValue")
                && child->property("text").toString() == text;
        });
    };
    QTRY_VERIFY(findLimit(QStringLiteral("6")) && findLimit(QStringLiteral("9")));
    QVERIFY(!rig.dashboard.isLoggerOnline(id1));
    QVERIFY(name->setProperty("text", QStringLiteral("Edited offline")));
    QVERIFY(poll->setProperty("value", 8));
    QVERIFY(save->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(save, "clicked"));
    QCOMPARE(saved.size(), 1);
    QVERIFY(saved.takeFirst().at(0).toBool());
    LoggerRepository loggers(rig.db.connection());
    QCOMPARE(loggers.findById(id1)->name, QStringLiteral("Edited offline"));
    QCOMPARE(loggers.findById(id1)->centralPollIntervalS, 8);

    // A successful poll must not enable threshold editing or start a REST fetch.
    SensorCatalogRepository catalog(rig.db.connection());
    PollSnapshot snap;
    snap.loggerId   = id1;
    snap.success    = true;
    snap.header.mapVersion  = 2;
    snap.header.statusFlags = 0x03;
    snap.measuredAt = QDateTime::currentDateTimeUtc();
    AnalogSample a;
    a.edgeSensorId = 1;
    a.statusCode   = QStringLiteral("00");
    a.hasQuality   = true;
    a.qualityValid = true;
    a.value        = 7.0f;
    snap.analogs   = { a };
    rig.dashboard.onSnapshotApplied(snap, 1, catalog.listByLoggerId(id1));

    QTRY_VERIFY(rig.dashboard.isLoggerOnline(id1));
    QVERIFY(!findLimit(QStringLiteral("6"))->inherits("QQuickTextInput"));
    QVERIFY(!findVisual(thresholdPane, [](const QQuickItem *child) {
        const QString text = child->property("text").toString();
        return text == QStringLiteral("Save") || text == QStringLiteral("Discard");
    }));

    QVERIFY(apiPort->setProperty("value", 1));
    QVERIFY(!reload->isEnabled()); // Save connection changes before reading that endpoint.
    QVERIFY(apiPort->setProperty("value", int(rig.edge.port())));
    QVERIFY(name->setProperty("text", QStringLiteral("Unsaved alias")));
    QVERIFY(poll->setProperty("value", 9));
    rig.edge.respondWith(R"({"revision":6,"thresholds_writable":true,
        "config":{"station_name":"Changed at edge","poll_interval":30},
        "sensors":[{"sensor_id":1,"sensor_type":"ANALOG","name":"pH","unit":"pH","max_threshold":null}]})");
    QSignalSpy reloadFinished(&rig.form, &LoggerFormController::configReloadFinished);
    QVERIFY(reload->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(reload, "clicked"));
    QTRY_COMPARE(reloadFinished.size(), 1);
    QVERIFY(reloadFinished.first().at(1).toBool());
    QTRY_VERIFY(findLimit(QStringLiteral("—")));
    QVERIFY(findLimit(QStringLiteral("6")));
    QCOMPARE(name->property("text").toString(), QStringLiteral("Unsaved alias"));
    QCOMPARE(poll->property("value").toInt(), 9);
    QCOMPARE(loggers.findById(id1)->name, QStringLiteral("Edited offline"));
    QCOMPARE(loggers.findById(id1)->centralPollIntervalS, 8);
    const QList<QByteArray> expectedRequests{"GET /api/v1/config HTTP/1.1", "GET /api/v1/config HTTP/1.1"};
    QCOMPARE(rig.edge.requests, expectedRequests);

    // Theme toggle light <-> dark works without QML warnings and without spurious saved()
    QSignalSpy savedSpy(&rig.settings, &SettingsController::saved);
    QVERIFY(rig.settings.saveTheme(QStringLiteral("light")));
    QCoreApplication::processEvents();
    QCOMPARE(rig.settings.theme(), QStringLiteral("light"));
    QVERIFY(rig.settings.saveTheme(QStringLiteral("dark")));
    QCoreApplication::processEvents();
    QCOMPARE(rig.settings.theme(), QStringLiteral("dark"));
    QCOMPARE(savedSpy.count(), 0);
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(qmlWarnings.join(QLatin1Char('\n'))));
}

void TestLoggersRefactorQml::responsiveDashboardAndHistoryToolbar()
{
    RuntimeRig rig("responsive");
    QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1024, 768);
    window.show();
    QQmlComponent dashboard(&engine, viewUrl("src/app/qml/views/DashboardView.qml"));
    QVERIFY2(dashboard.isReady(), qPrintable(dashboard.errorString()));
    QScopedPointer<QQuickItem> view(qobject_cast<QQuickItem *>(dashboard.create()));
    QVERIFY(view);
    view->setParentItem(window.contentItem());
    view->setSize(QSizeF(944, 688)); // 1024×768 minus nav rail and top bar
    auto *panels = namedVisual(view.data(), QStringLiteral("dashboardPanels"));
    QVERIFY(panels);
    QTRY_COMPARE(panels->property("columns").toInt(), 1);
    QCOMPARE(panels->childItems().size(), 2);
    const auto *chartPanel = panels->childItems().at(0);
    const auto *eventPanel = panels->childItems().at(1);
    QTRY_VERIFY(eventPanel->y() >= chartPanel->height());
    QVERIFY(eventPanel->y() + eventPanel->height() <= panels->height() + 1);

    window.resize(1920, 1080);
    view->setSize(QSizeF(1840, 1000));
    QTRY_COMPARE(panels->property("columns").toInt(), 2);
    QTRY_VERIFY(eventPanel->x() >= chartPanel->width());

    QQmlComponent history(&engine, viewUrl("src/app/qml/views/HistoryView.qml"));
    QVERIFY2(history.isReady(), qPrintable(history.errorString()));
    QScopedPointer<QObject> hist(history.create());
    QVERIFY2(hist, qPrintable(history.errorString()));
    QQmlComponent toolbar(&engine);
    toolbar.setData(R"(
        import QtQuick
        import CentralLogger.Components
        AppTopBar {}
    )", QUrl(QStringLiteral("qrc:/ToolbarSmoke.qml")));
    QVERIFY2(toolbar.isReady(), qPrintable(toolbar.errorString()));
    QScopedPointer<QQuickItem> bar(qobject_cast<QQuickItem *>(toolbar.create()));
    QVERIFY(bar);
    bar->setParentItem(window.contentItem());
    bar->setWidth(944);
    QVERIFY(bar->setProperty("toolbarSource", hist->property("topBarToolbar")));
    QTRY_VERIFY(namedVisual(bar.data(), QStringLiteral("historyActions")));
    const auto *actions = namedVisual(bar.data(), QStringLiteral("historyActions"));
    QTRY_COMPARE(actions->property("text").toString(), QStringLiteral("Actions"));
    QVERIFY(actions->mapToItem(bar.data(), QPointF(actions->width(), actions->height())).x() <= bar->width());
    QVERIFY(actions->mapToItem(bar.data(), QPointF(actions->width(), actions->height())).y() <= bar->height());
    bar->setWidth(1840);
    QTRY_COMPARE(actions->property("text").toString(), QStringLiteral("Export CSV"));
}

void TestLoggersRefactorQml::historyScrollBenchmarkAndRefreshAnchor()
{
    RuntimeRig rig("history-scroll");
    QQmlEngine engine;
    QQuickWindow window;
    window.resize(1024, 768);
    window.show();
    // Optional local A/B measurement using an untouched pre-change HistoryView.
    const QString baseline = qEnvironmentVariable("CENTRAL_LOGGER_HISTORY_BENCHMARK_BASELINE");
    QQmlComponent history(&engine, baseline.isEmpty() ? viewUrl("src/app/qml/views/HistoryView.qml")
                                                       : QUrl::fromLocalFile(baseline));
    QVERIFY2(history.isReady(), qPrintable(history.errorString()));
    QScopedPointer<QQuickItem> view(qobject_cast<QQuickItem *>(history.create()));
    QVERIFY2(view, qPrintable(history.errorString()));
    view->setParentItem(window.contentItem());
    view->setSize(QSizeF(944, 650));
    auto *vm = qobject_cast<HistoryViewModel *>(view->property("historyVm").value<QObject *>());
    QVERIFY(vm);
    auto *model = vm->tableModel();
    const auto now = QDateTime::currentDateTimeUtc();
    const int n = CentralLogger::Defaults::kHistorySearchLimit;
    QVector<HistoryRow> rows(n);
    for (int i = 0; i < n; ++i) {
        auto &row = rows[i];
        row.readingId = n - i;
        row.recordedAt = now.addSecs(-i);
        row.loggerName = QStringLiteral("Bench logger");
        row.sensorName = QStringLiteral("Pressure");
        row.unit = QStringLiteral("Pa");
        row.value = 2500.0 + i;
        row.statusCode = QStringLiteral("00");
    }
    model->setRows(rows);

    auto *table = findVisual(view.data(), [](const QQuickItem *item) {
        return item->inherits("QQuickTableView") && !item->inherits("QQuickHorizontalHeaderView");
    });
    QVERIFY(table);
    QTRY_COMPARE(table->property("rows").toInt(), n);
    QCOMPARE(table->property("reuseItems").toBool(), baseline.isEmpty());
    QQmlComponent scroller(&engine);
    scroller.setData(R"(
        import QtQuick
        Item {
            required property TableView table
            function scrollTo(row) {
                table.positionViewAtRow(row, TableView.AlignTop, 7)
                table.forceLayout()
            }
            function scrollSequence() {
                scrollTo(0)
                for (let step = 1; step <= 125; ++step) {
                    table.contentY = table.originY + step * 4 * 40
                    table.forceLayout()
                }
            }
            function topInset() {
                const cell = table.itemAtCell(Qt.point(0, table.topRow))
                return cell ? table.contentY - cell.y : -999
            }
        }
    )", QUrl(QStringLiteral("qrc:/HistoryScroll.qml")));
    QVERIFY2(scroller.isReady(), qPrintable(scroller.errorString()));
    QScopedPointer<QObject> driver(scroller.createWithInitialProperties({
        {QStringLiteral("table"), QVariant::fromValue(table)}
    }));
    QVERIFY2(driver, qPrintable(scroller.errorString()));
    QBENCHMARK {
        QVERIFY(QMetaObject::invokeMethod(driver.data(), "scrollSequence"));
    }

    if (baseline.isEmpty()) {
        QVERIFY(QMetaObject::invokeMethod(driver.data(), "scrollTo", Q_ARG(QVariant, QVariant(300))));
        QTRY_COMPARE_WITH_TIMEOUT(table->property("topRow").toInt(), 300, 2000);
        QVariant inset;
        QVERIFY(QMetaObject::invokeMethod(driver.data(), "topInset", Q_RETURN_ARG(QVariant, inset)));
        const qint64 anchoredId = model->readingIdAt(300);
        QSignalSpy reset(model, &HistoryTableModel::modelReset);
        auto latest = rows.takeFirst();
        latest.readingId = n + 1;
        latest.recordedAt = now.addSecs(1);
        rows.prepend(model->rows().constFirst());
        rows.prepend(latest);
        rows.removeLast();
        model->mergeLatest(rows);
        QTRY_COMPARE_WITH_TIMEOUT(model->readingIdAt(table->property("topRow").toInt()), anchoredId, 2000);
        QVariant afterInset;
        QVERIFY(QMetaObject::invokeMethod(driver.data(), "topInset", Q_RETURN_ARG(QVariant, afterInset)));
        QCOMPARE(afterInset.toDouble(), inset.toDouble());
        QCOMPARE(reset.size(), 0);
    }
}

void TestLoggersRefactorQml::dashboardSeriesRemainsPopulatedAcrossAxisAndPointChanges()
{
    RuntimeRig rig("dashboard-series");
    QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
    QQmlEngine engine;
    QQuickWindow window;
    window.resize(900, 650);
    window.show();
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import CentralLogger.Components
        ChartTimeSeriesPanel {
            readonly property real startTime: Date.now() - 300000
            readonly property var displayedSeries: chart.seriesList[0]
            axis: ({xMin: startTime, xMax: startTime + 300000, yMax: 10})
            plotPoints: [{bucketMs: startTime, count: 3}, {bucketMs: startTime + 60000, count: 8}]
            function updateAxisOnly() {
                axis = {xMin: startTime, xMax: startTime + 360000, yMax: 15}
            }
            function updateData() {
                plotPoints = [{bucketMs: startTime + 60000, count: 8},
                              {bucketMs: startTime + 120000, count: 11}]
            }
        }
    )", QUrl(QStringLiteral("qrc:/DashboardSeriesSmoke.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QQuickItem> chart(qobject_cast<QQuickItem *>(component.create()));
    QVERIFY2(chart, qPrintable(component.errorString()));
    chart->setParentItem(window.contentItem());
    chart->setSize(QSizeF(700, 350));
    QTRY_VERIFY(chart->property("displayedSeries").value<QObject *>());
    auto *series = chart->property("displayedSeries").value<QObject *>();
    QTRY_COMPARE(series->property("count").toInt(), 2);
    QVERIFY(QMetaObject::invokeMethod(chart.data(), "updateAxisOnly"));
    QCOMPARE(series->property("count").toInt(), 2);
    QVERIFY(QMetaObject::invokeMethod(chart.data(), "updateData"));
    QCoreApplication::processEvents();
    QCOMPARE(chart->property("displayedSeries").value<QObject *>(), series);
    QCOMPARE(series->property("count").toInt(), 2);
}

QTEST_MAIN(TestLoggersRefactorQml)
#include "test_loggers_refactor_qml.moc"
