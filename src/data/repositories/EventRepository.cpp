#include "EventRepository.h"

#include "SqlHelper.h"
#include "utils/DbConstants.h"
#include "utils/time/DateTimeUtils.h"

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

} // namespace

int EventRepository::purgeOlderThan(const QDateTime &cutoffUtc,
                                    QString *errorOut,
                                    int chunkSize)
{
    return purgeOlderThanChunked(m_db,
                                 CentralLogger::Data::Db::kTableSystemEvent,
                                 "created_at",
                                 cutoffUtc,
                                 chunkSize,
                                 errorOut);
}

bool EventRepository::insert(SystemEvent &event, QString *errorOut)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO %1 (logger_id, event_type, message, level) "
        "VALUES (:logger_id, :event_type, :message, :level)")
        .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableSystemEvent)));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLoggerId),
                event.loggerId ? QVariant(*event.loggerId)
                               : QVariant(QMetaType(QMetaType::LongLong)));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindEventType), event.eventType);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindMessage),    event.message);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLevel),      event.level);
    if (!q.exec()) {
        setSqlError(errorOut, q);
        return false;
    }
    event.id = q.lastInsertId().toLongLong();

    // M-1: read back the DB-generated created_at so the in-memory model
    // stays consistent with what was persisted (callers don't need a
    // separate fetch to render the correct timestamp).
    QSqlQuery sel(m_db);
    sel.prepare(QStringLiteral(
        "SELECT created_at FROM %1 WHERE id = :id")
        .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableSystemEvent)));
    sel.bindValue(QLatin1String(CentralLogger::Data::Db::kBindId), event.id);
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
        "SELECT * FROM %1 ORDER BY created_at DESC, id DESC LIMIT :limit")
        .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableSystemEvent)));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLimit), limit);
    if (!q.exec()) {
        setSqlError(errorOut, q);
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
        "FROM %1 e "
        "LEFT JOIN %2 l ON l.id = e.logger_id "
        "ORDER BY e.created_at DESC, e.id DESC LIMIT :limit")
        .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableSystemEvent),
             QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerInfo)));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLimit), limit);
    if (!q.exec()) {
        setSqlError(errorOut, q);
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
