#pragma once

#include "utils/AppConstants.h"

#include <QDateTime>
#include <QString>

namespace CentralLogger::Data {

struct LoggerInfo
{
    qint64    id = 0;
    QString   stationCode;
    QString   name;
    QString   host;
    int       modbusPort = Defaults::kDefaultModbusPort;
    int       modbusUnitId = Defaults::kDefaultModbusUnitId;
    int       centralPollIntervalS = Defaults::kDefaultPollIntervalSec;
    double    timeoutS = Defaults::kDefaultTimeoutSec;
    bool      enabled = true;
    int       apiPort = Defaults::kDefaultApiPort;
    QString   apiToken;            // may be empty
    int       lastRevision = -1;
    QString   status = QStringLiteral("offline");
    QDateTime lastSeen;            // UTC; null when never seen
    QString   note;
    QDateTime createdAt;           // UTC; populated by DB default on insert
};

} // namespace CentralLogger::Data
