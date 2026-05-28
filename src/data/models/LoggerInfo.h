#pragma once

#include <QDateTime>
#include <QString>

namespace CentralLogger::Data {

struct LoggerInfo
{
    qint64    id = 0;
    QString   stationCode;
    QString   name;
    QString   host;
    int       modbusPort = 5020;
    int       modbusUnitId = 1;
    int       centralPollIntervalS = 2;
    double    timeoutS = 2.0;
    bool      enabled = true;
    int       apiPort = 8080;
    QString   apiToken;            // may be empty
    int       lastRevision = -1;
    QString   status = QStringLiteral("offline");
    QDateTime lastSeen;            // UTC; null when never seen
    QString   note;
    QDateTime createdAt;           // UTC; populated by DB default on insert
};

} // namespace CentralLogger::Data
