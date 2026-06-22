#include "PollHistoryStore.h"

namespace CentralLogger::Core {

void PollHistoryStore::append(const Network::PollSnapshot &snapshot)
{
    if (!snapshot.success) return;

    const qint64 loggerId = snapshot.loggerId;
    auto &loggerStore = m_store[loggerId];

    for (const auto &analog : snapshot.analogs) {
        if (!analog.isValid()) continue;

        auto &deque = loggerStore[analog.edgeSensorId];

        TrendingPoint pt;
        pt.timestamp = snapshot.measuredAt;
        pt.value     = analog.value;
        deque.push_back(pt);

        while (deque.size() > static_cast<std::size_t>(kMaxPoints)) {
            deque.pop_front();
        }
    }
}

QVariantList PollHistoryStore::seriesForLogger(qint64 loggerId) const
{
    QVariantList result;

    const auto loggerIt = m_store.constFind(loggerId);
    if (loggerIt == m_store.constEnd()) return result;

    const auto &loggerStore = *loggerIt;
    for (auto it = loggerStore.constBegin(); it != loggerStore.constEnd(); ++it) {
        const int edgeSensorId = it.key();
        const auto &deque = it.value();
        if (deque.empty()) continue;

        QVariantList points;
        points.reserve(static_cast<QList<QVariant>::size_type>(deque.size()));
        for (const auto &pt : deque) {
            QVariantMap p;
            p.insert(QStringLiteral("x"),
                     static_cast<double>(pt.timestamp.toMSecsSinceEpoch()));
            p.insert(QStringLiteral("y"), static_cast<double>(pt.value));
            p.insert(QStringLiteral("time"),
                     pt.timestamp.toLocalTime().toString(QStringLiteral("HH:mm:ss")));
            points.append(p);
        }

        QVariantMap series;
        series.insert(QStringLiteral("edgeSensorId"), edgeSensorId);
        // L-21: prefer the catalog name stored via updateSensorNames();
        // fall back to "Sensor #N" when name is unavailable.
        const QString label = m_sensorNames.value(loggerId).value(
            edgeSensorId,
            QStringLiteral("Sensor #%1").arg(edgeSensorId));
        series.insert(QStringLiteral("label"), label);
        series.insert(QStringLiteral("decimals"),
                      m_sensorDecimals.value(loggerId).value(edgeSensorId, 4));
        series.insert(QStringLiteral("points"), points);
        result.append(series);
    }

    return result;
}

bool PollHistoryStore::has(qint64 loggerId) const
{
    return m_store.contains(loggerId);
}

void PollHistoryStore::updateSensorNames(qint64 loggerId, const QHash<int, QString> &names)
{
    m_sensorNames[loggerId] = names;
}

void PollHistoryStore::updateSensorDecimals(qint64 loggerId, const QHash<int, int> &decimals)
{
    m_sensorDecimals[loggerId] = decimals;
}

void PollHistoryStore::remove(qint64 loggerId)
{
    m_store.remove(loggerId);
    m_sensorNames.remove(loggerId);
    m_sensorDecimals.remove(loggerId);
}

void PollHistoryStore::clear()
{
    m_store.clear();
    m_sensorNames.clear();
    m_sensorDecimals.clear();
}

int PollHistoryStore::pointCount(qint64 loggerId, int edgeSensorId) const
{
    const auto loggerIt = m_store.constFind(loggerId);
    if (loggerIt == m_store.constEnd()) return 0;
    const auto sensorIt = loggerIt->constFind(edgeSensorId);
    if (sensorIt == loggerIt->constEnd()) return 0;
    return static_cast<int>(sensorIt->size());
}

} // namespace CentralLogger::Core
