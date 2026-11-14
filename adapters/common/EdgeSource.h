#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace CentralLogger::Adapters {

// Vendor-independent, in-memory edge representation. An unavailable numeric
// value is encoded as 0 on Modbus only with valid=false; consumers must never
// treat that transport placeholder as a reading.
struct AnalogPoint
{
    quint16 id = 0;
    QString name;
    QString unit;
    bool active = true;
    bool valid = false;
    bool alarm = false;
    bool stale = false;
    float value = 0.0f;
    qint64 timestampMs = 0;
};

struct EdgeSnapshot
{
    QVector<AnalogPoint> analogs;
    bool polling = false;
    bool rtuConnected = false;
    bool upstreamConnected = false;
    bool catalogAvailable = false;
};

// Adapter vendor plugins implement this interface. The v1 servers only see
// this representation and never send commands to the source device.
class EdgeSource
{
public:
    virtual ~EdgeSource() = default;
    virtual EdgeSnapshot snapshot() const = 0;
    // Redacted, cached diagnostics only; this method must not issue requests.
    virtual QJsonObject debugReadings() const = 0;
};

} // namespace CentralLogger::Adapters
