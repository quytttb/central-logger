#include "LoggerRepository.h"

#include "utils/DbConstants.h"
#include "utils/time/DateTimeUtils.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QStringLiteral>
#include <QVariant>

namespace CentralLogger::Data {

namespace {

// Column order kept in lockstep with the SELECT lists below so we can read
// rows by positional index (which avoids the qt.sql.qsqlquery "unknown field
// name" warnings emitted by q.value(QString) on prepared queries).
using CentralLogger::Data::Db::kColumnsLoggerInfo;
constexpr auto kColumns = kColumnsLoggerInfo;

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
        "INSERT INTO %1 ("
        "  station_code, name, host, modbus_port, modbus_unit_id,"
        "  central_poll_interval_s, timeout_s, enabled, api_port, api_token,"
        "  last_revision, status, last_seen, note"
        ") VALUES ("
        "  :station_code, :name, :host, :modbus_port, :modbus_unit_id,"
        "  :central_poll_interval_s, :timeout_s, :enabled, :api_port, :api_token,"
        "  :last_revision, :status, :last_seen, :note"
        ")").arg(QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerInfo)));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindStationCode),           info.stationCode);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindName),                   info.name);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindHost),                   info.host);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindModbusPort),            info.modbusPort);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindModbusUnitId),         info.modbusUnitId);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindCentralPollIntervalS), info.centralPollIntervalS);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindTimeoutS),              info.timeoutS);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindEnabled),                info.enabled ? 1 : 0);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindApiPort),               info.apiPort);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindApiToken),
                info.apiToken.isEmpty() ? QVariant(QMetaType(QMetaType::QString))
                                        : QVariant(info.apiToken));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLastRevision),          info.lastRevision);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindStatus),                 info.status);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLastSeen),              isoUtcOrNull(info.lastSeen));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindNote),
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
    q.prepare(QStringLiteral("SELECT %1 FROM %2 WHERE id = :id")
                  .arg(QString::fromLatin1(kColumns),
                       QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerInfo)));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindId), id);
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
    q.prepare(QStringLiteral("SELECT %1 FROM %2 WHERE station_code = :code")
                  .arg(QString::fromLatin1(kColumns),
                       QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerInfo)));
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
    if (!q.exec(QStringLiteral("SELECT %1 FROM %2 ORDER BY id")
                    .arg(QString::fromLatin1(kColumns),
                         QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerInfo)))) {
        setErr(errorOut, q);
        return result;
    }
    while (q.next()) {
        result.append(rowToModel(q));
    }
    return result;
}

int LoggerRepository::countTotal(QString *errorOut) const
{
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM %1")
                    .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerInfo)))
        || !q.next()) {
        setErr(errorOut, q);
        return -1;
    }
    return q.value(0).toInt();
}

int LoggerRepository::countOnline(QString *errorOut) const
{
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM %1 WHERE status = '%2'")
                    .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerInfo),
                         CentralLogger::Sensor::kLoggerOnline))
        || !q.next()) {
        setErr(errorOut, q);
        return -1;
    }
    return q.value(0).toInt();
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
        "  (SELECT COUNT(*) FROM %1 s WHERE s.logger_id = l.id) AS sensor_count "
        "FROM %2 l "
        "ORDER BY l.id")
        .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerSensor),
             QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerInfo));
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
        "UPDATE %1 SET "
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
        "WHERE id = :id")
        .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerInfo)));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindStationCode),           info.stationCode);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindName),                   info.name);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindHost),                   info.host);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindModbusPort),            info.modbusPort);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindModbusUnitId),         info.modbusUnitId);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindCentralPollIntervalS), info.centralPollIntervalS);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindTimeoutS),              info.timeoutS);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindEnabled),                info.enabled ? 1 : 0);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindApiPort),               info.apiPort);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindApiToken),
                info.apiToken.isEmpty() ? QVariant(QMetaType(QMetaType::QString))
                                        : QVariant(info.apiToken));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLastRevision),          info.lastRevision);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindStatus),                 info.status);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLastSeen),              isoUtcOrNull(info.lastSeen));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindNote),
                info.note.isNull() ? QVariant(QMetaType(QMetaType::QString))
                                   : QVariant(info.note));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindId),                     info.id);

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
        "UPDATE %1 SET status = :status, last_seen = :last_seen WHERE id = :id")
        .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerInfo)));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindStatus),    status);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindLastSeen), isoUtcOrNull(lastSeenUtc));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindId),        id);
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
        "UPDATE %1 SET status = :status WHERE id = :id")
        .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerInfo)));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindStatus), status);
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindId),     id);
    if (!q.exec()) {
        setErr(errorOut, q);
        return false;
    }
    return q.numRowsAffected() > 0;
}

bool LoggerRepository::remove(qint64 id, QString *errorOut)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM %1 WHERE id = :id")
                   .arg(QString::fromLatin1(CentralLogger::Data::Db::kTableLoggerInfo)));
    q.bindValue(QLatin1String(CentralLogger::Data::Db::kBindId), id);
    if (!q.exec()) {
        setErr(errorOut, q);
        return false;
    }
    return q.numRowsAffected() > 0;
}

} // namespace CentralLogger::Data
