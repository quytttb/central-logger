#pragma once

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <QTimeZone>
#include <QVector>

namespace CentralLogger::Core {

struct AverageExportParams
{
    qint64    loggerId = 0;
    QDateTime fromUtc;
    QDateTime toUtc;
    QTimeZone timeZone;
};

struct AverageExportResult
{
    QString csv;
    int     rowCount    = 0;    // data rows (excludes header + average row)
    int     columnCount = 0;    // sensor columns (excludes "Thời gian")
    QString error;
};

/// Builds an hourly sensor-average CSV with a final overall-average row.
/// Pure SQL + string building so
/// it can run on a worker thread with its own DB connection and be unit
/// tested without a UI.
class AverageExportService
{
public:
    explicit AverageExportService(QSqlDatabase db) : m_db(std::move(db)) {}

    AverageExportResult build(const AverageExportParams &params) const;

private:
    QSqlDatabase m_db;
};

} // namespace CentralLogger::Core
