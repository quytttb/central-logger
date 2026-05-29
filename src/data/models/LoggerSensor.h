#pragma once

#include <QString>
#include <QVector>
#include <optional>

namespace CentralLogger::Data {

struct LoggerSensor
{
    qint64               id = 0;
    qint64               loggerId = 0;
    int                  edgeSensorId = 0;
    QString              sensorType = QStringLiteral("UNKNOWN"); // ANALOG|DI|DO|UNKNOWN
    QString              name;
    QString              unit;
    std::optional<double> minThreshold;
    std::optional<double> maxThreshold;
    bool                 active = true;
    /// Edge PK of primary parent analog (from GET /config `parent_id`); null = top-level.
    std::optional<int>   parentEdgeSensorId;
    /// DI status code for reports / attach-DI (00–03 or custom); ANALOG usually empty.
    QString              diType;
    /// All analog parent edge IDs when a DI is linked to multiple analogs.
    /// Stored as JSON array in DB column `all_parent_ids`.
    /// Empty = use parentEdgeSensorId only.
    QVector<int>         allParentIds;
};

} // namespace CentralLogger::Data
