#include "EventRepository.h"

#include "utils/time/DateTimeUtils.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QDateTime>

namespace CentralLogger::Data {

namespace {

using CentralLogger::Utils::parseUtc;

// Column positions for the listRecent / listRecentWithLoggerName SELECT
// lists. Using positional access avoids qt.sql.qsqlquery "unknown field name"
// warnings on prepared queries and is faster than named lookups.
enum ColEvent {
    ColEventId = 0,
    ColEventLoggerId,
    ColEventEventType,
    ColEventMessage,
    ColEventLevel,
    ColEventCreatedAt,
    ColEventLoggerName, // only present in the JOIN variant
};

SystemEvent rowToModel(const QSqlQuery &q)
{
    SystemEvent e;
    e.id        = q.value(ColEventId).toLongLong();
    const QVariant lid = q.value(ColEventLoggerId);
    if (!lid.isNull()) {
        e.loggerId = lid.toLongLong();
    }
    e.eventType = q.value(ColEventEventType).toString();
    e.message   = q.value(ColEventMessage).toString();
    e.level     = q.value(ColEventLevel).toString();
    e.createdAt = parseUtc(q.value(ColEventCreatedAt).toString());
    return e;
}

void setErr(QString *out, const QSqlQuery &q)
{
    if (out) {
        *out = q.lastError().text();
    }
}

} // namespace

int EventRepository::purgeOlderThan(const QDateTime &cutoffUtc,
                                    QString *errorOut,
                                    int chunkSize)
{
    const QString cutoff = CentralLogger::Utils::isoUtc(cutoffUtc);
    QSqlQuery q(m_db);

    if (chunkSize <= 0) {
        q.prepare(QStringLiteral("DELETE FROM system_event WHERE created_at < :cutoff"));
        q.bindValue(QStringLiteral(":cutoff"), cutoff);
        if (!q.exec()) {
            if (errorOut) *errorOut = q.lastError().text();
            return -1;
        }
        return q.numRowsAffected();
    }

    q.prepare(QStringLiteral(
        "DELETE FROM system_event WHERE id IN ("
        "SELECT id FROM system_event WHERE created_at < :cutoff "
        "ORDER BY created_at LIMIT :lim)"));
    int deleted = 0;
    for (;;) {
        q.bindValue(QStringLiteral(":cutoff"), cutoff);
        q.bindValue(QStringLiteral(":lim"), chunkSize);
        if (!q.exec()) {
            if (errorOut) *errorOut = q.lastError().text();
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

bool EventRepository::insert(SystemEvent &event, QString *errorOut)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO system_event (logger_id, event_type, message, level) "
        "VALUES (:logger_id, :event_type, :message, :level)"));
    q.bindValue(QStringLiteral(":logger_id"),
                event.loggerId ? QVariant(*event.loggerId)
                               : QVariant(QMetaType(QMetaType::LongLong)));
    q.bindValue(QStringLiteral(":event_type"), event.eventType);
    q.bindValue(QStringLiteral(":message"),    event.message);
    q.bindValue(QStringLiteral(":level"),      event.level);
    if (!q.exec()) {
        setErr(errorOut, q);
        return false;
    }
    event.id = q.lastInsertId().toLongLong();

    // M-1: read back the DB-generated created_at so the in-memory model
    // stays consistent with what was persisted (callers don't need a
    // separate fetch to render the correct timestamp).
    QSqlQuery sel(m_db);
    sel.prepare(QStringLiteral(
        "SELECT created_at FROM system_event WHERE id = :id"));
    sel.bindValue(QStringLiteral(":id"), event.id);
    if (sel.exec() && sel.next()) {
        event.createdAt = parseUtc(sel.value(0).toString());
    }
    return true;
}

QVector<SystemEvent> EventRepository::listRecent(int limit, QString *errorOut) const
{
    QVector<SystemEvent> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT * FROM system_event ORDER BY created_at DESC, id DESC LIMIT :limit"));
    q.bindValue(QStringLiteral(":limit"), limit);
    if (!q.exec()) {
        setErr(errorOut, q);
        return result;
    }
    while (q.next()) {
        result.append(rowToModel(q));
    }
    return result;
}

QVector<SystemEventListItem> EventRepository::listRecentWithLoggerName(
    int limit, QString *errorOut) const
{
    QVector<SystemEventListItem> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT e.id AS id, e.logger_id AS logger_id, e.event_type AS event_type, "
        "       e.message AS message, e.level AS level, e.created_at AS created_at, "
        "       l.name AS logger_name "
        "FROM system_event e "
        "LEFT JOIN logger_info l ON l.id = e.logger_id "
        "ORDER BY e.created_at DESC, e.id DESC LIMIT :limit"));
    q.bindValue(QStringLiteral(":limit"), limit);
    if (!q.exec()) {
        setErr(errorOut, q);
        return result;
    }
    while (q.next()) {
        SystemEventListItem item;
        item.event      = rowToModel(q);
        item.loggerName = q.value(ColEventLoggerName).toString();
        result.append(item);
    }
    return result;
}

} // namespace CentralLogger::Data
