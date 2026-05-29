#include "SensorReadingRepository.h"

#include "utils/DateTimeUtils.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace CentralLogger::Data {

namespace {

using CentralLogger::Utils::isoUtc;

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
        "INSERT INTO sensor_reading ("
        "  sensor_id, value, valid, alarm, stale, logger_timestamp, recorded_at"
        ") VALUES ("
        "  :sensor_id, :value, :valid, :alarm, :stale, :logger_timestamp, :recorded_at"
        ")"));

    for (const SensorReading &r : readings) {
        const QDateTime when = r.recordedAt.isValid() ? r.recordedAt : QDateTime::currentDateTimeUtc();
        q.bindValue(QStringLiteral(":sensor_id"),        r.sensorId);
        q.bindValue(QStringLiteral(":value"),            r.value);
        q.bindValue(QStringLiteral(":valid"),            r.valid ? 1 : 0);
        q.bindValue(QStringLiteral(":alarm"),            r.alarm ? 1 : 0);
        q.bindValue(QStringLiteral(":stale"),            r.stale ? 1 : 0);
        q.bindValue(QStringLiteral(":logger_timestamp"), r.loggerTimestamp);
        q.bindValue(QStringLiteral(":recorded_at"),      isoUtc(when));
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

int SensorReadingRepository::purgeOlderThan(const QDateTime &cutoffUtc, QString *errorOut)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM sensor_reading WHERE recorded_at < :cutoff"));
    q.bindValue(QStringLiteral(":cutoff"), isoUtc(cutoffUtc));
    if (!q.exec()) {
        setErr(errorOut, q);
        return -1;
    }
    return q.numRowsAffected();
}

int SensorReadingRepository::countForSensor(qint64 sensorId, QString *errorOut) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM sensor_reading WHERE sensor_id = :sid"));
    q.bindValue(QStringLiteral(":sid"), sensorId);
    if (!q.exec() || !q.next()) {
        setErr(errorOut, q);
        return -1;
    }
    return q.value(0).toInt();
}

} // namespace CentralLogger::Data
