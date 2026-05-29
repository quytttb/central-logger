#include "LoggerRepository.h"

#include "utils/DateTimeUtils.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QStringLiteral>
#include <QVariant>

namespace CentralLogger::Data {

namespace {

// Column order kept in lockstep with the SELECT lists below so we can read
// rows by positional index (which avoids the qt.sql.qsqlquery "unknown field
// name" warnings emitted by q.value(QString) on prepared queries).
constexpr auto kColumns =
    "id, station_code, name, host, modbus_port, modbus_unit_id, "
    "central_poll_interval_s, timeout_s, enabled, api_port, api_token, "
    "last_revision, status, last_seen, note, created_at";

enum Col {
    ColId = 0,
    ColStationCode,
    ColName,
    ColHost,
    ColModbusPort,
    ColModbusUnitId,
    ColCentralPollIntervalS,
    ColTimeoutS,
    ColEnabled,
    ColApiPort,
    ColApiToken,
    ColLastRevision,
    ColStatus,
    ColLastSeen,
    ColNote,
    ColCreatedAt,
};

using CentralLogger::Utils::isoUtc;
using CentralLogger::Utils::isoUtcOrNull;
using CentralLogger::Utils::parseUtc;

LoggerInfo rowToModel(const QSqlQuery &q)
{
    LoggerInfo info;
    info.id                   = q.value(ColId).toLongLong();
    info.stationCode          = q.value(ColStationCode).toString();
    info.name                 = q.value(ColName).toString();
    info.host                 = q.value(ColHost).toString();
    info.modbusPort           = q.value(ColModbusPort).toInt();
    info.modbusUnitId         = q.value(ColModbusUnitId).toInt();
    info.centralPollIntervalS = q.value(ColCentralPollIntervalS).toInt();
    info.timeoutS             = q.value(ColTimeoutS).toDouble();
    info.enabled              = q.value(ColEnabled).toInt() != 0;
    info.apiPort              = q.value(ColApiPort).toInt();
    info.apiToken             = q.value(ColApiToken).toString();
    info.lastRevision         = q.value(ColLastRevision).toInt();
    info.status               = q.value(ColStatus).toString();
    info.lastSeen             = parseUtc(q.value(ColLastSeen).toString());
    info.note                 = q.value(ColNote).toString();
    info.createdAt            = parseUtc(q.value(ColCreatedAt).toString());
    return info;
}

void setErr(QString *out, const QSqlQuery &q)
{
    if (out) {
        *out = q.lastError().text();
    }
}

} // namespace

bool LoggerRepository::insert(LoggerInfo &info, QString *errorOut)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO logger_info ("
        "  station_code, name, host, modbus_port, modbus_unit_id,"
        "  central_poll_interval_s, timeout_s, enabled, api_port, api_token,"
        "  last_revision, status, last_seen, note"
        ") VALUES ("
        "  :station_code, :name, :host, :modbus_port, :modbus_unit_id,"
        "  :central_poll_interval_s, :timeout_s, :enabled, :api_port, :api_token,"
        "  :last_revision, :status, :last_seen, :note"
        ")"));
    q.bindValue(QStringLiteral(":station_code"),           info.stationCode);
    q.bindValue(QStringLiteral(":name"),                   info.name);
    q.bindValue(QStringLiteral(":host"),                   info.host);
    q.bindValue(QStringLiteral(":modbus_port"),            info.modbusPort);
    q.bindValue(QStringLiteral(":modbus_unit_id"),         info.modbusUnitId);
    q.bindValue(QStringLiteral(":central_poll_interval_s"), info.centralPollIntervalS);
    q.bindValue(QStringLiteral(":timeout_s"),              info.timeoutS);
    q.bindValue(QStringLiteral(":enabled"),                info.enabled ? 1 : 0);
    q.bindValue(QStringLiteral(":api_port"),               info.apiPort);
    q.bindValue(QStringLiteral(":api_token"),
                info.apiToken.isEmpty() ? QVariant(QMetaType(QMetaType::QString))
                                        : QVariant(info.apiToken));
    q.bindValue(QStringLiteral(":last_revision"),          info.lastRevision);
    q.bindValue(QStringLiteral(":status"),                 info.status);
    q.bindValue(QStringLiteral(":last_seen"),              isoUtcOrNull(info.lastSeen));
    q.bindValue(QStringLiteral(":note"),
                info.note.isNull() ? QVariant(QMetaType(QMetaType::QString))
                                   : QVariant(info.note));

    if (!q.exec()) {
        setErr(errorOut, q);
        return false;
    }
    info.id = q.lastInsertId().toLongLong();

    // Read back created_at to keep the in-memory model consistent with DB.
    auto stored = findById(info.id, errorOut);
    if (stored) {
        info.createdAt = stored->createdAt;
    }
    return true;
}

std::optional<LoggerInfo> LoggerRepository::findById(qint64 id, QString *errorOut) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT %1 FROM logger_info WHERE id = :id")
                  .arg(QString::fromLatin1(kColumns)));
    q.bindValue(QStringLiteral(":id"), id);
    if (!q.exec()) {
        setErr(errorOut, q);
        return std::nullopt;
    }
    if (!q.next()) {
        return std::nullopt;
    }
    return rowToModel(q);
}

std::optional<LoggerInfo> LoggerRepository::findByStationCode(const QString &stationCode,
                                                              QString *errorOut) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT %1 FROM logger_info WHERE station_code = :code")
                  .arg(QString::fromLatin1(kColumns)));
    q.bindValue(QStringLiteral(":code"), stationCode);
    if (!q.exec()) {
        setErr(errorOut, q);
        return std::nullopt;
    }
    if (!q.next()) {
        return std::nullopt;
    }
    return rowToModel(q);
}

QVector<LoggerInfo> LoggerRepository::findAll(QString *errorOut) const
{
    QVector<LoggerInfo> result;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT %1 FROM logger_info ORDER BY id")
                    .arg(QString::fromLatin1(kColumns)))) {
        setErr(errorOut, q);
        return result;
    }
    while (q.next()) {
        result.append(rowToModel(q));
    }
    return result;
}

QVector<LoggerListRow> LoggerRepository::findAllWithSensorCounts(QString *errorOut) const
{
    QVector<LoggerListRow> result;
    QSqlQuery q(m_db);
    // M-3: qualify every column with the table alias "l." so the query is
    // unambiguous even if a JOIN is added later.  Column order matches the
    // positional enum (ColId … ColCreatedAt) used by rowToModel(); sensor_count
    // lands at ColCreatedAt+1 as before.
    static const QString sql = QStringLiteral(
        "SELECT "
        "  l.id, l.station_code, l.name, l.host,"
        "  l.modbus_port, l.modbus_unit_id, l.central_poll_interval_s,"
        "  l.timeout_s, l.enabled, l.api_port, l.api_token,"
        "  l.last_revision, l.status, l.last_seen, l.note, l.created_at,"
        "  (SELECT COUNT(*) FROM logger_sensor s WHERE s.logger_id = l.id) AS sensor_count "
        "FROM logger_info l "
        "ORDER BY l.id");
    if (!q.exec(sql)) {
        setErr(errorOut, q);
        return result;
    }
    while (q.next()) {
        LoggerListRow row;
        row.info        = rowToModel(q);
        row.sensorCount = q.value(ColCreatedAt + 1).toInt();
        result.append(row);
    }
    return result;
}

bool LoggerRepository::update(const LoggerInfo &info, QString *errorOut)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE logger_info SET "
        "  station_code = :station_code,"
        "  name = :name,"
        "  host = :host,"
        "  modbus_port = :modbus_port,"
        "  modbus_unit_id = :modbus_unit_id,"
        "  central_poll_interval_s = :central_poll_interval_s,"
        "  timeout_s = :timeout_s,"
        "  enabled = :enabled,"
        "  api_port = :api_port,"
        "  api_token = :api_token,"
        "  last_revision = :last_revision,"
        "  status = :status,"
        "  last_seen = :last_seen,"
        "  note = :note "
        "WHERE id = :id"));
    q.bindValue(QStringLiteral(":station_code"),           info.stationCode);
    q.bindValue(QStringLiteral(":name"),                   info.name);
    q.bindValue(QStringLiteral(":host"),                   info.host);
    q.bindValue(QStringLiteral(":modbus_port"),            info.modbusPort);
    q.bindValue(QStringLiteral(":modbus_unit_id"),         info.modbusUnitId);
    q.bindValue(QStringLiteral(":central_poll_interval_s"), info.centralPollIntervalS);
    q.bindValue(QStringLiteral(":timeout_s"),              info.timeoutS);
    q.bindValue(QStringLiteral(":enabled"),                info.enabled ? 1 : 0);
    q.bindValue(QStringLiteral(":api_port"),               info.apiPort);
    q.bindValue(QStringLiteral(":api_token"),
                info.apiToken.isEmpty() ? QVariant(QMetaType(QMetaType::QString))
                                        : QVariant(info.apiToken));
    q.bindValue(QStringLiteral(":last_revision"),          info.lastRevision);
    q.bindValue(QStringLiteral(":status"),                 info.status);
    q.bindValue(QStringLiteral(":last_seen"),              isoUtcOrNull(info.lastSeen));
    q.bindValue(QStringLiteral(":note"),
                info.note.isNull() ? QVariant(QMetaType(QMetaType::QString))
                                   : QVariant(info.note));
    q.bindValue(QStringLiteral(":id"),                     info.id);

    if (!q.exec()) {
        setErr(errorOut, q);
        return false;
    }
    return q.numRowsAffected() > 0;
}

bool LoggerRepository::updateStatusAndLastSeen(qint64 id,
                                               const QString &status,
                                               const QDateTime &lastSeenUtc,
                                               QString *errorOut)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE logger_info SET status = :status, last_seen = :last_seen WHERE id = :id"));
    q.bindValue(QStringLiteral(":status"),    status);
    q.bindValue(QStringLiteral(":last_seen"), isoUtcOrNull(lastSeenUtc));
    q.bindValue(QStringLiteral(":id"),        id);
    if (!q.exec()) {
        setErr(errorOut, q);
        return false;
    }
    return q.numRowsAffected() > 0;
}

bool LoggerRepository::updateStatus(qint64 id, const QString &status, QString *errorOut)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE logger_info SET status = :status WHERE id = :id"));
    q.bindValue(QStringLiteral(":status"), status);
    q.bindValue(QStringLiteral(":id"),     id);
    if (!q.exec()) {
        setErr(errorOut, q);
        return false;
    }
    return q.numRowsAffected() > 0;
}

bool LoggerRepository::remove(qint64 id, QString *errorOut)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM logger_info WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    if (!q.exec()) {
        setErr(errorOut, q);
        return false;
    }
    return q.numRowsAffected() > 0;
}

} // namespace CentralLogger::Data
