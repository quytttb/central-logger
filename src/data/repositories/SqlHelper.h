#pragma once

#include "utils/DbConstants.h"
#include "utils/time/DateTimeUtils.h"

#include <QDateTime>
#include <QDebug>
#include <QObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>

namespace CentralLogger::Data {

/// Maps a raw SQLite driver error into a user-facing message.
///
/// Centralises the error-string sniffing that used to live in controllers so
/// CRUD code stays thin. Currently handles the `logger_info.station_code`
/// UNIQUE collision; falls back to the raw text for anything else.
inline QString humanizeSqlError(const QString &raw, const QString &stationCode)
{
    if (raw.contains(QStringLiteral("UNIQUE"), Qt::CaseInsensitive) &&
        raw.contains(QStringLiteral("station_code"))) {
        return QObject::tr("Station code \"%1\" already exists.").arg(stationCode);
    }
    return raw;
}

/// Copies `q.lastError().text()` into `*errorOut` when provided, otherwise logs
/// a warning if `logContext` is specified and an error message exists.
inline void setSqlError(QString *errorOut, const QSqlQuery &q,
                        const char *logContext = nullptr)
{
    const QString text = q.lastError().text();
    if (errorOut) {
        *errorOut = text;
    } else if (logContext && !text.isEmpty()) {
        qWarning() << logContext << "SQL error:" << text;
    }
}

/// Deletes rows older than @p cutoffUtc in bounded chunks so WAL write locks
/// release between batches. @p chunkSize <= 0 deletes in a single statement.
inline int purgeOlderThanChunked(QSqlDatabase db,
                                 const char *tableName,
                                 const char *timestampColumn,
                                 const QDateTime &cutoffUtc,
                                 int chunkSize,
                                 QString *errorOut = nullptr)
{
    const QString cutoff = CentralLogger::Utils::isoUtc(cutoffUtc);
    const QString table = QString::fromLatin1(tableName);
    const QString tsCol = QString::fromLatin1(timestampColumn);
    QSqlQuery q(db);

    if (chunkSize <= 0) {
        q.prepare(QStringLiteral("DELETE FROM %1 WHERE %2 < :cutoff")
                      .arg(table, tsCol));
        q.bindValue(QLatin1String(Db::kBindCutoff), cutoff);
        if (!q.exec()) {
            setSqlError(errorOut, q);
            return -1;
        }
        return q.numRowsAffected();
    }

    q.prepare(QStringLiteral(
                  "DELETE FROM %1 WHERE id IN ("
                  "SELECT id FROM %1 WHERE %2 < :cutoff "
                  "ORDER BY %2 LIMIT :limit)")
                  .arg(table, tsCol));
    int deleted = 0;
    for (;;) {
        q.bindValue(QLatin1String(Db::kBindCutoff), cutoff);
        q.bindValue(QLatin1String(Db::kBindLimit), chunkSize);
        if (!q.exec()) {
            setSqlError(errorOut, q);
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

} // namespace CentralLogger::Data
