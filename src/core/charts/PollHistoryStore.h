#pragma once

#include "Downsampler.h"
#include "network/modbus/ModbusTypes.h"

#include <QHash>

#include <deque>
#include <limits>
#include <optional>

namespace CentralLogger::Core {

/// Per-logger, per-analog-sensor history bounded by both age and sample count.
/// Latest validity is independent of the retained measurements.
class PollHistoryStore {
public:
  static constexpr int kMaxPoints = Defaults::kGraphRamMaxPoints;

  /// Appends one data point per analog sample from a successful snapshot.
  /// Failed snapshots clear live validity but keep existing history.
  void append(const Network::PollSnapshot &snapshot);

  GraphSeries seriesForLogger(qint64 loggerId, qint64 fromMs = std::numeric_limits<qint64>::min(),
                             qint64 toMs = std::numeric_limits<qint64>::max()) const;
  bool coversStart(qint64 loggerId, qint64 fromMs) const;
  qint64 latestTimestamp(qint64 loggerId) const { return m_lastSampleMs.value(loggerId); }
  qint64 latestEvictedTimestamp(qint64 loggerId) const { return m_lastEvictedMs.value(loggerId); }
  std::optional<double> currentValue(qint64 loggerId, int edgeSensorId) const;

  /// Marks all sensors for @p loggerId as having no current live sample
  /// (e.g. when the logger is disabled) without discarding buffered points.
  void markOffline(qint64 loggerId);

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
  using SensorDeque = QHash<int, std::deque<GraphPoint>>;
  QHash<qint64, SensorDeque> m_store;
  QHash<qint64, QHash<int, double>> m_currentValues;
  QHash<qint64, qint64> m_lastSampleMs;
  QHash<qint64, qint64> m_lastEvictedMs;
};

} // namespace CentralLogger::Core
