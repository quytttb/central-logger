#pragma once

#include "core/sensors/SensorLiveRow.h"
#include "data/models/LoggerSensor.h"
#include "network/modbus/ModbusTypes.h"

#include <QVector>

namespace CentralLogger::Core {

/// Pure merge: catalog rows + Modbus snapshot → display rows.
/// No DB, no REST, no QML. See docs/thiet_ke_db.md §3.3 and
/// docs/contracts/modbus-map-v1.md (FC03 analog + FC02 DI + FC01 DO).
class SensorMerger
{
public:
    static QVector<SensorLiveRow> buildRows(
        qint64 loggerId,
        const Network::PollSnapshot &snapshot,
        const QVector<Data::LoggerSensor> &catalog);

    /// Catalog-only rows (WAIT) when Modbus cache is empty — e.g. after
    /// GET /config before the first successful poll.
    static QVector<SensorLiveRow> buildCatalogPlaceholders(
        qint64 loggerId,
        const QVector<Data::LoggerSensor> &catalog);
};

} // namespace CentralLogger::Core
