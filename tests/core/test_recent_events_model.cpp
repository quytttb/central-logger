#include "core/DashboardController.h"
#include "core/events/RecentEventsModel.h"
#include "data/db/Database.h"
#include "data/models/SystemEvent.h"
#include "data/repositories/EventRepository.h"
#include "network/modbus/ModbusTypes.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QTimeZone>
#include <QVariantMap>

using CentralLogger::Core::DashboardController;
using CentralLogger::Core::RecentEventsModel;
using CentralLogger::Data::Database;
using CentralLogger::Data::EventRepository;
using CentralLogger::Data::SystemEvent;
using CentralLogger::Network::PollSnapshot;

namespace {

QString uniqueConnection(const char *suffix)
{
    static int counter = 0;
    return QStringLiteral("recent_events_%1_%2")
        .arg(QString::fromLatin1(suffix))
        .arg(++counter);
}

void insertEvent(Database &db,
                 std::optional<qint64> loggerId,
                 const QString &type,
                 const QString &message,
                 const QString &level = QStringLiteral("info"))
{
    EventRepository repo(db.connection());
    SystemEvent ev;
    ev.loggerId  = loggerId;
    ev.eventType = type;
    ev.message   = message;
    ev.level     = level;
    QVERIFY(repo.insert(ev));
}

PollSnapshot makeSnapshot(qint64 loggerId, bool success)
{
    PollSnapshot snap;
    snap.loggerId   = loggerId;
    snap.success    = success;
    snap.measuredAt = QDateTime::currentDateTimeUtc();
    return snap;
}

} // namespace

class TestRecentEventsModel : public QObject
{
    Q_OBJECT

private slots:
    void emptyByDefault();
    void reloadReturnsRecentEventsNewestFirst();
    void rolesExposeLoggerNameViaJoin();
    void appWideEventsHaveNullLoggerId();
    void limitOverrideRestrictsRows();
    void dashboardCrudReloadsModel();
    void dashboardOnlineTransitionLogsEvent();
    void dashboardOfflineTransitionLogsWarningEvent();
    void dashboardSkipsFirstSnapshotAndDoesNotSpamRepeats();
    void displayLevelPrefersEventTypeOverStaleLevel();
};

void TestRecentEventsModel::emptyByDefault()
{
    Database db;
    QVERIFY(db.open(uniqueConnection("empty"), QStringLiteral(":memory:")));

    RecentEventsModel model;
    model.setDatabase(&db);
    model.reload();

    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.limit(), 20);
}

void TestRecentEventsModel::reloadReturnsRecentEventsNewestFirst()
{
    Database db;
    QVERIFY(db.open(uniqueConnection("order"), QStringLiteral(":memory:")));

    for (int i = 0; i < 3; ++i) {
        insertEvent(db, std::nullopt,
                    QStringLiteral("Info"),
                    QStringLiteral("event-%1").arg(i));
    }

    RecentEventsModel model;
    model.setDatabase(&db);
    model.reload();

    QCOMPARE(model.rowCount(), 3);

    const QString first = model
        .data(model.index(0), RecentEventsModel::MessageRole).toString();
    const QString last = model
        .data(model.index(2), RecentEventsModel::MessageRole).toString();
    QCOMPARE(first, QStringLiteral("event-2"));
    QCOMPARE(last,  QStringLiteral("event-0"));
}

void TestRecentEventsModel::rolesExposeLoggerNameViaJoin()
{
    Database db;
    QVERIFY(db.open(uniqueConnection("join"), QStringLiteral(":memory:")));

    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);
    const qint64 loggerId = ctrl.addLogger(QStringLiteral("TRAM-RX"),
                                           QStringLiteral("Trạm RX"),
                                           QStringLiteral("h"),
                                           5020, 8080, {});
    QVERIFY(loggerId > 0);

    insertEvent(db, loggerId,
                QStringLiteral("Online"),
                QStringLiteral("Logger TRAM-RX is online"));

    RecentEventsModel model;
    model.setDatabase(&db);
    model.reload();

    QVERIFY(model.rowCount() >= 1);
    const QModelIndex idx = model.index(0);
    QCOMPARE(model.data(idx, RecentEventsModel::EventTypeRole).toString(),
             QStringLiteral("Online"));
    QCOMPARE(model.data(idx, RecentEventsModel::LoggerIdRole).toLongLong(),
             loggerId);
    QCOMPARE(model.data(idx, RecentEventsModel::LoggerNameRole).toString(),
             QStringLiteral("Trạm RX"));
    QCOMPARE(model.data(idx, RecentEventsModel::LevelRole).toString(),
             QStringLiteral("info"));
    QCOMPARE(model.data(idx, RecentEventsModel::DisplayLevelRole).toString(),
             QStringLiteral("info"));
    QVERIFY(model.data(idx, RecentEventsModel::CreatedAtRole)
                .toDateTime().isValid());

    const auto names = model.roleNames();
    QCOMPARE(names.value(RecentEventsModel::LoggerNameRole), QByteArray("loggerName"));
    QCOMPARE(names.value(RecentEventsModel::EventTypeRole),  QByteArray("eventType"));
}

void TestRecentEventsModel::appWideEventsHaveNullLoggerId()
{
    Database db;
    QVERIFY(db.open(uniqueConnection("appwide"), QStringLiteral(":memory:")));

    insertEvent(db, std::nullopt,
                QStringLiteral("Info"),
                QStringLiteral("Application started"));

    RecentEventsModel model;
    model.setDatabase(&db);
    model.reload();

    QCOMPARE(model.rowCount(), 1);
    const QVariant loggerId =
        model.data(model.index(0), RecentEventsModel::LoggerIdRole);
    QVERIFY(!loggerId.isValid() || loggerId.isNull());
    QCOMPARE(model.data(model.index(0), RecentEventsModel::LoggerNameRole)
                 .toString(),
             QString{});
}

void TestRecentEventsModel::limitOverrideRestrictsRows()
{
    Database db;
    QVERIFY(db.open(uniqueConnection("limit"), QStringLiteral(":memory:")));

    for (int i = 0; i < 25; ++i) {
        insertEvent(db, std::nullopt,
                    QStringLiteral("Info"),
                    QStringLiteral("evt-%1").arg(i));
    }

    RecentEventsModel model;
    model.setDatabase(&db);
    model.reload();
    QCOMPARE(model.rowCount(), 20);

    QSignalSpy limitSpy(&model, &RecentEventsModel::limitChanged);
    model.setLimit(5);
    QCOMPARE(limitSpy.count(), 1);
    QCOMPARE(model.rowCount(), 5);
}

void TestRecentEventsModel::dashboardCrudReloadsModel()
{
    Database db;
    QVERIFY(db.open(uniqueConnection("crud"), QStringLiteral(":memory:")));

    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);

    auto *model = ctrl.recentEvents();
    QVERIFY(model);
    model->reload();
    QCOMPARE(model->rowCount(), 0);

    const qint64 id = ctrl.addLogger(QStringLiteral("TRAM-EV"),
                                     QStringLiteral("Trạm Ev"),
                                     QStringLiteral("h"),
                                     5020, 8080, {});
    QVERIFY(id > 0);
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(model->data(model->index(0), RecentEventsModel::EventTypeRole)
                 .toString(),
             QStringLiteral("Info"));
}

void TestRecentEventsModel::dashboardOnlineTransitionLogsEvent()
{
    Database db;
    QVERIFY(db.open(uniqueConnection("online"), QStringLiteral(":memory:")));

    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);
    const qint64 id = ctrl.addLogger(QStringLiteral("TRAM-ON"),
                                     QStringLiteral("Trạm On"),
                                     QStringLiteral("h"),
                                     5020, 8080, {});
    QVERIFY(id > 0);

    EventRepository repo(db.connection());
    const int crudEvents = repo.listRecent().size();

    // First snapshot seeds the tracker without an event.
    ctrl.onSnapshotApplied(makeSnapshot(id, /*success*/ false), 0);
    QCOMPARE(repo.listRecent().size(), crudEvents);

    // Transition offline → online: one Online event with level=info.
    ctrl.onSnapshotApplied(makeSnapshot(id, /*success*/ true), 0);
    const auto recent = repo.listRecent();
    QCOMPARE(recent.size(), crudEvents + 1);
    QCOMPARE(recent.first().eventType, QStringLiteral("Online"));
    QCOMPARE(recent.first().level,     QStringLiteral("info"));
    QVERIFY(recent.first().message.contains(QStringLiteral("TRAM-ON")));

    auto *model = ctrl.recentEvents();
    QCOMPARE(model->data(model->index(0), RecentEventsModel::EventTypeRole)
                 .toString(),
             QStringLiteral("Online"));
}

void TestRecentEventsModel::dashboardOfflineTransitionLogsWarningEvent()
{
    Database db;
    QVERIFY(db.open(uniqueConnection("offline"), QStringLiteral(":memory:")));

    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);
    const qint64 id = ctrl.addLogger(QStringLiteral("TRAM-OFF"),
                                     QStringLiteral("Trạm Off"),
                                     QStringLiteral("h"),
                                     5020, 8080, {});
    QVERIFY(id > 0);

    // First successful poll logs Online; capture the count after so the
    // Offline assertion below is always +1 regardless.
    ctrl.onSnapshotApplied(makeSnapshot(id, /*success*/ true), 0);

    EventRepository repo(db.connection());
    const int beforeOffline = repo.listRecent().size();

    ctrl.onSnapshotApplied(makeSnapshot(id, /*success*/ false), 0);
    const auto recent = repo.listRecent();
    QCOMPARE(recent.size(), beforeOffline + 1);
    QCOMPARE(recent.first().eventType, QStringLiteral("Offline"));
    QCOMPARE(recent.first().level,     QStringLiteral("warning"));
}

void TestRecentEventsModel::dashboardSkipsFirstSnapshotAndDoesNotSpamRepeats()
{
    Database db;
    QVERIFY(db.open(uniqueConnection("noSpam"), QStringLiteral(":memory:")));

    DashboardController ctrl(nullptr);
    ctrl.setDatabase(&db);
    const qint64 id = ctrl.addLogger(QStringLiteral("TRAM-NS"),
                                     QStringLiteral("Trạm NS"),
                                     QStringLiteral("h"),
                                     5020, 8020, {});
    QVERIFY(id > 0);

    EventRepository repo(db.connection());
    const int crudEvents = repo.listRecent().size();

    // First successful poll logs an Online event immediately so the user
    // sees the initial connection succeed.
    ctrl.onSnapshotApplied(makeSnapshot(id, true), 0);
    QCOMPARE(repo.listRecent().size(), crudEvents + 1);

    // Repeated online polls must not spam events.
    ctrl.onSnapshotApplied(makeSnapshot(id, true), 0);
    ctrl.onSnapshotApplied(makeSnapshot(id, true), 0);
    QCOMPARE(repo.listRecent().size(), crudEvents + 1);

    ctrl.onSnapshotApplied(makeSnapshot(id, false), 0); // Online → Offline
    QCOMPARE(repo.listRecent().size(), crudEvents + 2);

    ctrl.onSnapshotApplied(makeSnapshot(id, false), 0); // no edge
    QCOMPARE(repo.listRecent().size(), crudEvents + 2);

    ctrl.onSnapshotApplied(makeSnapshot(id, true), 0);  // Offline → Online
    QCOMPARE(repo.listRecent().size(), crudEvents + 3);
}

void TestRecentEventsModel::displayLevelPrefersEventTypeOverStaleLevel()
{
    Database db;
    QVERIFY(db.open(uniqueConnection("display"), QStringLiteral(":memory:")));

    // Legacy rows: eventType Warning but level wrongly stored as info.
    insertEvent(db, std::nullopt, QStringLiteral("Warning"),
                QStringLiteral("config push failed"),
                QStringLiteral("info"));

    RecentEventsModel model;
    model.setDatabase(&db);
    model.reload();

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), RecentEventsModel::DisplayLevelRole).toString(),
             QStringLiteral("warning"));
}

QTEST_MAIN(TestRecentEventsModel)
#include "test_recent_events_model.moc"
