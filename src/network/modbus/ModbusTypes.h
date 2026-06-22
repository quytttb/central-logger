#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>
#include <cstdint>

namespace CentralLogger::Network {

/// HR0..HR9 — fixed layout per docs/contracts/modbus-map-v1.md §1.
struct ModbusHeader
{
    quint16  mapVersion    = 0;
    quint16  statusFlags   = 0;
    quint32  unixTimestamp = 0; // HR2 high, HR3 low
    quint16  na            = 0; // HR4 — analog count
    quint16  ndi           = 0; // HR5 — DI bit count
    quint16  ndo           = 0; // HR6 — DO bit count

    bool isValid()         const { return mapVersion == 1; }
    bool isPolling()       const { return (statusFlags & 0x01) != 0; }
    bool isRtuConnected()  const { return (statusFlags & 0x02) != 0; }
    bool isAnyAlarm()      const { return (statusFlags & 0x04) != 0; }
};

/// One ANALOG block (8 holding registers) from HR10 + i*8.
struct AnalogSample
{
    quint16 edgeSensorId = 0;
    quint16 flags        = 0; // bit0=valid, bit1=alarm, bit2=stale
    float   value        = 0.0f;

    bool isValid() const { return (flags & 0x01) != 0; }
    bool isAlarm() const { return (flags & 0x02) != 0; }
    bool isStale() const { return (flags & 0x04) != 0; }
};

/// Result of one full poll cycle for one logger. Emitted by ModbusService
/// to ModbusDataDispatcher via Qt::QueuedConnection.
struct PollSnapshot
{
    qint64               loggerId = 0;
    bool                 success  = false;
    QString              errorMessage;
    ModbusHeader         header;
    QVector<AnalogSample> analogs;
    QVector<bool>        diBits;
    QVector<bool>        doBits;
    QDateTime            measuredAt; // UTC, set by worker right after reads
};

} // namespace CentralLogger::Network

#include <QMetaType>
Q_DECLARE_METATYPE(CentralLogger::Network::PollSnapshot)
