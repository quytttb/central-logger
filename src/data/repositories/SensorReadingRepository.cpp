#include "SensorReadingRepository.h"

#include "utils/DbConstants.h"
#include "utils/time/DateTimeUtils.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QPair>

namespace CentralLogger::Data {

namespace {

using CentralLogger::Utils::isoUtc;
using CentralLogger::Utils::parseUtc;

void setErr(QString *out, const QSqlQuery &q)
{
    if (out) {
        *out = q.lastError().text();
    }
}

} // namespace

bool SensorReadingRepository::insertBatch(const QVector<SensorReading> &readings,
                                          QString *errorOut,
                                          bool manageTransaction)
{
    if (readings.isEmpty()) {
        return true;
    }

    if (manageTransaction) {
        if (!m_db.transaction()) {
            if (errorOut) {
                *errorOut = m_db.lastError().text();
            }
            return false;
        }
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO %1 ("
        "  sensor_id, value, valid, alarm, stale, logger_timestamp, recorded_at"
        ") VALUES ("
        "  :sensor_id, :value, :valid, :alarm, :stale, :logger_timestamp, :recorded_at"
        ")").arg(QString::fromLatin1(CentralLogger::Data::Db::kTableSensorReading)));

    for (const SensorReading &r : readings) {
        const QDateTime when = r.recordedAt.isValid() ? r.recordedAt : QDateTime::currentDateTimeUtc();
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindSensorId),        r.sensorId);
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindValue),            r.value);
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindValid),            r.valid ? 1 : 0);
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindAlarm),            r.alarm ? 1 : 0);
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindStale),            r.stale ? 1 : 0);
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLoggerTimestamp), r.loggerTimestamp);
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindRecordedAt),      isoUtc(when));
        if (!q.exec()) {
            setErr(errorOut, q);
            if (manageTransaction) {
                m_db.rollback();
            }
            return false;
        }
    }

    if (manageTransaction) {
        if (!m_db.commit()) {
            if (errorOut) {
                *errorOut = m_db.lastError().text();
            }
            m_db.rollback();
            return false;
        }
    }
    return true;
}

int SensorReadingRepository::purgeOlderThan(const QDateTime &cutoffUtc,
                                            QString *errorOut,
                                            int chunkSize)
{
    const QString cutoff = isoUtc(cutoffUtc);
    QSqlQuery q(m_db);

    if (chunkSize <= 0) {
        q.prepare(QStringLiteral("DELETE FROM %1 WHERE recorded_at < :cutoff")
                      .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableSensorReading)));
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindCutoff), cutoff);
        if (!q.exec()) {
            setErr(errorOut, q);
            return -1;
        }
        return q.numRowsAffected();
    }

    q.prepare(QStringLiteral(
        "DELETE FROM %1 WHERE id IN ("
        "SELECT id FROM %1 WHERE recorded_at < :cutoff "
        "ORDER BY recorded_at LIMIT :lim)")
        .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableSensorReading)));
    int deleted = 0;
    for (;;) {
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindCutoff), cutoff);
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLim), chunkSize);
        if (!q.exec()) {
            setErr(errorOut, q);
            return -1;
        }
        const int affected = q.numRowsAffected();
        if (affected <= 0) {
            break;
        }
        deleted += affected;
        if (affected < chunkSize) {
            break;
        }
    }
    return deleted;
}

int SensorReadingRepository::countForSensor(qint64 sensorId, QString *errorOut) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM %1 WHERE sensor_id = :sid")
                   .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableSensorReading)));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindSid), sensorId);
    if (!q.exec() || !q.next()) {
        setErr(errorOut, q);
        return -1;
    }
    return q.value(0).toInt();
}

QVector<HistoryRow> SensorReadingRepository::searchHistory(
    qint64 loggerId,
    const QDateTime &fromUtc,
    const QDateTime &toUtc,
    qint64 sensorId,
    int limit,
    QString *errorOut) const
{
    QVector<HistoryRow> result;

    QString sql =
        QStringLiteral(
            "SELECT r.id, r.recorded_at, li.name, s.name, s.unit, r.value, r.valid, r.alarm, r.stale, r.sensor_id, s.decimals "
            "FROM %1 r "
            "JOIN %2 s ON r.sensor_id = s.id "
            "JOIN %3 li ON s.logger_id = li.id "
            "WHERE r.recorded_at >= :from "
            "  AND r.recorded_at <= :to")
            .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableSensorReading),
                 QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerSensor),
                 QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerInfo));
    if (loggerId > 0) {
        sql += QStringLiteral(" AND s.logger_id = :logger_id");
    }
    if (sensorId > 0) {
        sql += QStringLiteral(" AND r.sensor_id = :sensor_id");
    }
    sql += QStringLiteral(" ORDER BY r.recorded_at DESC LIMIT :lim");

    QSqlQuery q(m_db);
    q.prepare(sql);
    if (loggerId > 0) {
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLoggerId), loggerId);
    }
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindFrom), isoUtc(fromUtc));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindTo),   isoUtc(toUtc));
    if (sensorId > 0) {
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindSensorId), sensorId);
    }
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLim), limit);

    if (!q.exec()) {
        setErr(errorOut, q);
        return result;
    }

    while (q.next()) {
        HistoryRow row;
        row.id          = q.value(0).toLongLong();
        row.recordedAt  = parseUtc(q.value(1).toString());
        row.loggerName  = q.value(2).toString();
        row.sensorName  = q.value(3).toString();
        row.unit        = q.value(4).toString();
        row.value       = q.value(5).toDouble();
        row.valid       = q.value(6).toInt() != 0;
        row.alarm       = q.value(7).toInt() != 0;
        row.stale       = q.value(8).toInt() != 0;
        row.sensorId    = q.value(9).toLongLong();
        row.decimals    = q.value(10).toInt();
        result.append(row);
    }
    return result;
}

int SensorReadingRepository::countHistory(qint64 loggerId,
                                          const QDateTime &fromUtc,
                                          const QDateTime &toUtc,
                                          qint64 sensorId,
                                          QString *errorOut) const
{
    QString sql =
        QStringLiteral(
            "SELECT COUNT(*) "
            "FROM %1 r "
            "JOIN %2 s ON r.sensor_id = s.id "
            "JOIN %3 li ON s.logger_id = li.id "
            "WHERE r.recorded_at >= :from "
            "  AND r.recorded_at <= :to")
            .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableSensorReading),
                 QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerSensor),
                 QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerInfo));
    if (loggerId > 0) {
        sql += QStringLiteral(" AND s.logger_id = :logger_id");
    }
    if (sensorId > 0) {
        sql += QStringLiteral(" AND r.sensor_id = :sensor_id");
    }

    QSqlQuery q(m_db);
    q.prepare(sql);
    if (loggerId > 0) {
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLoggerId), loggerId);
    }
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindFrom), isoUtc(fromUtc));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindTo),   isoUtc(toUtc));
    if (sensorId > 0) {
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindSensorId), sensorId);
    }

    if (!q.exec() || !q.next()) {
        setErr(errorOut, q);
        return -1;
    }
    return q.value(0).toInt();
}

QVector<QPair<qint64, QString>> SensorReadingRepository::sensorsWithReadings(
    qint64 loggerId, QString *errorOut) const
{
    QVector<QPair<qint64, QString>> result;

    QString sql;
    if (loggerId > 0) {
        sql = QStringLiteral(
            "SELECT DISTINCT s.id, s.name "
            "FROM %1 s "
            "INNER JOIN %2 r ON r.sensor_id = s.id "
            "WHERE s.logger_id = :logger_id "
            "ORDER BY s.name ASC")
            .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerSensor),
                 QString::fromLatin1(CentralLogger::Data::Db::kTableSensorReading));
    } else {
        sql = QStringLiteral(
            "SELECT DISTINCT s.id, li.name || ' — ' || s.name "
            "FROM %1 s "
            "INNER JOIN %2 r ON r.sensor_id = s.id "
            "JOIN %3 li ON s.logger_id = li.id "
            "ORDER BY li.name ASC, s.name ASC")
            .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerSensor),
                 QString::fromLatin1(CentralLogger::Data::Db::kTableSensorReading),
                 QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerInfo));
    }

    QSqlQuery q(m_db);
    q.prepare(sql);
    if (loggerId > 0) {
        q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLoggerId), loggerId);
    }

    if (!q.exec()) {
        setErr(errorOut, q);
        return result;
    }

    while (q.next()) {
        result.append({ q.value(0).toLongLong(), q.value(1).toString() });
    }
    return result;
}

} // namespace CentralLogger::Data
