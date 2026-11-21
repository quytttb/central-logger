#include "core/history/AverageExportService.h"
#include "data/db/Database.h"
#include "utils/CsvUtils.h"

#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

using CentralLogger::Core::AverageExportParams;
using CentralLogger::Core::AverageExportResult;
using CentralLogger::Core::AverageExportService;
using CentralLogger::Data::Database;

namespace {

QString csvCellAt(const QString &line, int index)
{
    // Test CSV has no quoted commas; the newline-in-header is only in row 1
    // which this helper is not used for.
    const QStringList cells = line.split(QLatin1Char(','));
    return index >= 0 && index < cells.size() ? cells.at(index) : QString();
}

QString lineStartingWith(const QStringList &lines, const QString &prefix)
{
    for (const QString &line : lines) {
        if (line.startsWith(prefix)) {
            return line;
        }
    }
    return {};
}

} // namespace

class TestAverageExportService : public QObject
{
    Q_OBJECT

private:
    Database m_db;

    void seedFixtures()
    {
        QSqlQuery q(m_db.connection());
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO logger_info (id, station_code, name, host) "
            "VALUES (1, 'ST-001', 'Test Logger', '127.0.0.1')")));
        // Analog sensors: O2 (2 decimals) + CO (1 decimal).
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO logger_sensor (id, logger_id, edge_sensor_id, sensor_type, name, unit, decimals) "
            "VALUES (1, 1, 1, 'ANALOG', 'O2', '%V', 2)")));
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO logger_sensor (id, logger_id, edge_sensor_id, sensor_type, name, unit, decimals) "
            "VALUES (2, 1, 2, 'ANALOG', 'CO', 'mg/Nm3', 1)")));
        // DI must never appear in the average table.
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO logger_sensor (id, logger_id, edge_sensor_id, sensor_type, name, unit) "
            "VALUES (3, 1, 0, 'DI', 'Pump', '')")));
    }

    void insertReading(qint64 sensorId, double value, const QString &isoUtc)
    {
        QSqlQuery q(m_db.connection());
        q.prepare(QStringLiteral(
            "INSERT INTO sensor_reading (sensor_id, value, status_code, logger_timestamp, recorded_at) "
            "VALUES (:sid, :val, '00', 0, :ts)"));
        q.bindValue(QStringLiteral(":sid"), sensorId);
        q.bindValue(QStringLiteral(":val"), value);
        q.bindValue(QStringLiteral(":ts"), isoUtc);
        QVERIFY(q.exec());
    }

    AverageExportParams hourlyParams() const
    {
        AverageExportParams p;
        p.loggerId = 1;
        p.timeZone = QTimeZone::UTC;
        p.fromUtc = QDateTime(QDate(2026, 11, 5), QTime(0, 0), QTimeZone::UTC);
        p.toUtc   = QDateTime(QDate(2026, 11, 5), QTime(23, 59, 59), QTimeZone::UTC);
        return p;
    }

private slots:
    void initTestCase()
    {
        QString err;
        QVERIFY2(m_db.open(QStringLiteral("test_average_export"),
                           QStringLiteral(":memory:"), &err),
                 qPrintable(err));
        seedFixtures();
    }

    void cleanup()
    {
        QSqlQuery q(m_db.connection());
        q.exec(QStringLiteral("DELETE FROM sensor_reading"));
    }

    void writeUtf8BomFile_supportsStringsAndStreamCallbacks()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString content = QStringLiteral("Giá trị,42\n");
        const QString constContent = content;
        const QByteArray expected = QByteArray::fromHex("efbbbf") + content.toUtf8();
        QString error;

        const auto verifyFile = [&](const QString &path) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), expected);
        };

        const QString mutablePath = dir.filePath(QStringLiteral("mutable.csv"));
        QVERIFY2(CentralLogger::Utils::writeUtf8BomFile(mutablePath, content, &error),
                 qPrintable(error));
        verifyFile(mutablePath);

        const QString constPath = dir.filePath(QStringLiteral("const.csv"));
        QVERIFY2(CentralLogger::Utils::writeUtf8BomFile(constPath, constContent, &error),
                 qPrintable(error));
        verifyFile(constPath);

        const QString callbackPath = dir.filePath(QStringLiteral("callback.csv"));
        QVERIFY2(CentralLogger::Utils::writeUtf8BomFile(
                     callbackPath, [&content](QTextStream &out) { out << content; }, &error),
                 qPrintable(error));
        verifyFile(callbackPath);
    }

    void hourlyAverage_matchesSampleLayout()
    {
        insertReading(1, 10.0, QStringLiteral("2026-11-05T10:00:00.000Z"));
        insertReading(1, 20.0, QStringLiteral("2026-11-05T10:30:00.000Z"));
        insertReading(1, 30.0, QStringLiteral("2026-11-05T11:00:00.000Z"));
        insertReading(2, 5.0,  QStringLiteral("2026-11-05T10:15:00.000Z"));

        AverageExportService svc(m_db.connection());
        const AverageExportResult result = svc.build(hourlyParams());

        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.rowCount, 24);   // 00:00 … 23:00
        QCOMPARE(result.columnCount, 2); // O2 + CO (DI filtered out)

        const QStringList lines = result.csv.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

        QVERIFY(result.csv.startsWith(
            QStringLiteral("Thời gian,\"O2\n%V\",\"CO\nmg/Nm3\"")));

        const QString tenOclock = lineStartingWith(lines, QStringLiteral("10:00 05/11/2026,"));
        QVERIFY(!tenOclock.isEmpty());
        QCOMPARE(csvCellAt(tenOclock, 1), QStringLiteral("15.00")); // (10+20)/2
        QCOMPARE(csvCellAt(tenOclock, 2), QStringLiteral("5.0"));

        const QString elevenOclock = lineStartingWith(lines, QStringLiteral("11:00 05/11/2026,"));
        QVERIFY(!elevenOclock.isEmpty());
        QCOMPARE(csvCellAt(elevenOclock, 1), QStringLiteral("30.00"));
        QCOMPARE(csvCellAt(elevenOclock, 2), QString()); // no CO reading

        const QString averageRow = lines.last();
        QVERIFY(averageRow.startsWith(QStringLiteral("Giá trị trung bình 24h,")));
        QCOMPARE(csvCellAt(averageRow, 1), QStringLiteral("20.00")); // (10+20+30)/3
        QCOMPARE(csvCellAt(averageRow, 2), QStringLiteral("5.0"));
    }

    void hourlyAverage_excludesNonMeasuringStatuses()
    {
        insertReading(1, 10.0, QStringLiteral("2026-11-05T10:00:00.000Z"));
        QSqlQuery q(m_db.connection());
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO sensor_reading (sensor_id,value,status_code,recorded_at) VALUES "
            "(1,100,'01','2026-11-05T10:10:00.000Z'),"
            "(1,200,'02','2026-11-05T10:20:00.000Z'),"
            "(1,300,'03','2026-11-05T10:30:00.000Z'),"
            "(1,400,'UNKNOWN','2026-11-05T10:40:00.000Z')")));
        AverageExportService svc(m_db.connection());
        const auto result = svc.build(hourlyParams());
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        const QStringList lines = result.csv.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        const QString tenOclock = lineStartingWith(lines, QStringLiteral("10:00 05/11/2026,"));
        QCOMPARE(csvCellAt(tenOclock, 1), QStringLiteral("10.00"));
    }

    void noLogger_selected_returnsError()
    {
        AverageExportParams p = hourlyParams();
        p.loggerId = 0;

        AverageExportService svc(m_db.connection());
        const AverageExportResult result = svc.build(p);
        QVERIFY(!result.error.isEmpty());
    }

    void analogColumnWithoutReadings_isEmpty()
    {
        // No readings at all: every cell blank, averages blank, rows still 24.
        AverageExportService svc(m_db.connection());
        const AverageExportResult result = svc.build(hourlyParams());

        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.rowCount, 24);
        const QStringList lines = result.csv.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        const QString midnight = lineStartingWith(lines, QStringLiteral("00:00 05/11/2026,"));
        QVERIFY(!midnight.isEmpty());
        QCOMPARE(csvCellAt(midnight, 1), QString());
        QCOMPARE(csvCellAt(lines.last(), 1), QString());
    }
};

QTEST_GUILESS_MAIN(TestAverageExportService)
#include "test_average_export_service.moc"
