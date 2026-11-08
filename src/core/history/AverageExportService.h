#pragma once

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <QTimeZone>
#include <QVector>

namespace CentralLogger::Core {

/// Sensor-average CSV layout: header row per sensor (name + unit), one row per
/// hour/shift, and a final overall-average row.
///   row 1  : "Thời gian", "<sensor>\n<unit>", …
///   rows   : one per hour (Hourly) or per 8h shift (Shift), avg per sensor
///   last   : overall average row
enum class AverageExportMode {
    Hourly,
    Shift,
};

struct AverageExportParams
{
    qint64    loggerId = 0;
    QDateTime fromUtc;
    QDateTime toUtc;
    QTimeZone timeZone;
    int       shiftStartHour = 0;   // 0..23, Shift mode only
    AverageExportMode mode = AverageExportMode::Hourly;
};

struct AverageExportResult
{
    QString csv;
    int     rowCount    = 0;    // data rows (excludes header + average row)
    int     columnCount = 0;    // sensor columns (excludes "Thời gian")
    QString error;
};

/// Builds a sensor-average CSV for one logger. Pure SQL + string building so
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
