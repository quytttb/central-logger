#pragma once

#include "utils/charts/ChartDisplayLimits.h"
#include "network/modbus/ModbusTypes.h"

#include <QDateTime>
#include <QHash>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <deque>

namespace CentralLogger::Core {

/// One data point in the trending ring buffer.
struct TrendingPoint
{
    QDateTime timestamp;
    float     value = 0.0f;
};

/// Per-logger, per-analog-sensor ring buffer (max kChartDisplayPointCount entries).
/// Fed by `DashboardController::onSnapshotApplied` on every successful poll.
/// Consumed by `LoggerDetailViewModel` to expose a `QVariantList` of series
/// for the Qt Graphs trending chart (Task 15).
///
/// Only **analog** samples are stored — DI/DO are 0/1 toggles and don't
/// benefit from a continuous line chart.
class PollHistoryStore
{
public:
    static constexpr int kMaxPoints = kChartDisplayPointCount;

    /// Appends one data point per analog sample from a successful snapshot.
    /// Failed snapshots (`!snapshot.success`) are silently ignored.
    void append(const Network::PollSnapshot &snapshot);

    /// L-21: Update the display name map for a logger's analog sensors.
    /// Called by DashboardController after each successful snapshot when the
    /// catalog rows are available. Names are used as series labels instead of
    /// the generic "Sensor #N" fallback.
    void updateSensorNames(qint64 loggerId, const QHash<int, QString> &names);

    /// Returns a list of series suitable for QML / Qt Graphs:
    ///
    /// ```
    /// [
    ///   { "edgeSensorId": 1, "label": "Sensor #1",
    ///     "points": [ { "x": 0, "y": 25.3, "time": "14:30:05" }, … ] },
    ///   …
    /// ]
    /// ```
    ///
    /// `x` is measurement time as UTC epoch milliseconds (for DateTimeAxis);
    /// `y` is the analog value; `time` is formatted `HH:mm:ss` for legends.
    QVariantList seriesForLogger(qint64 loggerId) const;

    /// True when there is at least one sensor with history for this logger.
    bool has(qint64 loggerId) const;

    /// Remove all history for one logger (e.g. on CRUD remove).
    void remove(qint64 loggerId);

    /// Remove all history.
    void clear();

    /// Number of loggers with stored history.
    int loggerCount() const { return m_store.size(); }

    /// Number of data points stored for a specific sensor of a logger.
    int pointCount(qint64 loggerId, int edgeSensorId) const;

private:
    // loggerId → (edgeSensorId → deque of points)
    using SensorDeque = QHash<int, std::deque<TrendingPoint>>;
    QHash<qint64, SensorDeque>         m_store;
    // loggerId → (edgeSensorId → display name)
    QHash<qint64, QHash<int, QString>> m_sensorNames;
};

} // namespace CentralLogger::Core
