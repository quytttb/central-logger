#include "EventRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTimeZone>
#include <QVariant>

namespace CentralLogger::Data {

namespace {

QDateTime parseUtc(const QString &iso)
{
    if (iso.isEmpty()) {
        return {};
    }
    QDateTime dt = QDateTime::fromString(iso, Qt::ISODateWithMs);
    if (!dt.isValid()) {
        dt = QDateTime::fromString(iso, Qt::ISODate);
    }
    if (dt.isValid() && dt.timeSpec() == Qt::LocalTime) {
        dt.setTimeZone(QTimeZone::UTC);
    }
    return dt;
}

SystemEvent rowToModel(const QSqlQuery &q)
{
    SystemEvent e;
    e.id        = q.value(QStringLiteral("id")).toLongLong();
    const QVariant lid = q.value(QStringLiteral("logger_id"));
    if (!lid.isNull()) {
        e.loggerId = lid.toLongLong();
    }
    e.eventType = q.value(QStringLiteral("event_type")).toString();
    e.message   = q.value(QStringLiteral("message")).toString();
    e.level     = q.value(QStringLiteral("level")).toString();
    e.createdAt = parseUtc(q.value(QStringLiteral("created_at")).toString());
    return e;
}

void setErr(QString *out, const QSqlQuery &q)
{
    if (out) {
        *out = q.lastError().text();
    }
}

} // namespace

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
        item.loggerName = q.value(QStringLiteral("logger_name")).toString();
        result.append(item);
    }
    return result;
}

} // namespace CentralLogger::Data
