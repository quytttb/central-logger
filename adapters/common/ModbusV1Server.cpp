#include "ModbusV1Server.h"

#include "utils/AdapterConstants.h"
#include "utils/AppConstants.h"
#include "utils/Version.h"

#include <QModbusPdu>
#include <QTcpSocket>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace CentralLogger::Adapters {

namespace {
using namespace CentralLogger::AdapterDefaults;
}

// QModbusTcpServer owns and deletes its installed observer. It must therefore
// be a separate heap object, never a base subobject of ModbusV1Server itself.
class ModbusV1Server::ConnectionObserver final : public QModbusTcpConnectionObserver
{
public:
    explicit ConnectionObserver(ModbusV1Server *owner) : m_owner(owner) {}
    bool acceptNewConnection(QTcpSocket *socket) override
    {
        return m_owner->acceptNewConnection(socket);
    }

private:
    ModbusV1Server *m_owner;
};

ModbusV1Server::ModbusV1Server(const EdgeSource &source, int snapshotLeaseMs,
                               QObject *parent)
    : QModbusTcpServer(parent), m_source(source),
      m_snapshotLeaseMs(snapshotLeaseMs)
{
    installConnectionObserver(new ConnectionObserver(this));
}

ModbusV1Server::~ModbusV1Server()
{
    disconnectDevice();
    installConnectionObserver(nullptr);
}

bool ModbusV1Server::acceptNewConnection(QTcpSocket *newClient)
{
    if (m_client)
        return false;
    m_client = newClient;
    m_latchedRegisters.clear();
    connect(newClient, &QTcpSocket::disconnected, this, [this, newClient]() {
        if (m_client == newClient) {
            m_client.clear();
            m_latchedRegisters.clear();
        }
    });
    return true;
}

QVector<quint16> ModbusV1Server::encodeSnapshot(const EdgeSnapshot &snapshot)
{
    QVector<quint16> regs(kHeaderRegisters, 0);
    regs[0] = CentralLogger::Version::kModbusMapVersion;
    if (snapshot.polling)
        regs[1] |= 0x01;
    if (snapshot.rtuConnected)
        regs[1] |= 0x02;

    // Fail closed rather than publish a truncated or out-of-range register map.
    if (snapshot.analogs.size() > kMaxAnalogs)
        return regs;

    qint64 oldestMs = 0;
    bool anyAlarm = false;
    for (const auto &point : snapshot.analogs) {
        if (point.valid && !point.stale && point.timestampMs > 0
            && (oldestMs == 0 || point.timestampMs < oldestMs))
            oldestMs = point.timestampMs;
        anyAlarm |= point.valid && point.alarm;
    }
    if (anyAlarm)
        regs[1] |= 0x04;

    const qint64 seconds = oldestMs / CentralLogger::Defaults::kMsPerSecond;
    if (seconds > 0 && seconds <= std::numeric_limits<quint32>::max()) {
        regs[2] = static_cast<quint16>(seconds >> 16);
        regs[3] = static_cast<quint16>(seconds & 0xffff);
    }
    regs[4] = static_cast<quint16>(snapshot.analogs.size());
    // HR5/HR6 stay zero: no reliable DI/DO mapping has been supplied.

    regs.reserve(kHeaderRegisters + snapshot.analogs.size() * kAnalogRegisters);
    for (const auto &point : snapshot.analogs) {
        const bool valid = point.valid && std::isfinite(point.value);
        quint16 flags = valid ? 0x01 : 0;
        if (valid && point.alarm)
            flags |= 0x02;
        if (point.stale)
            flags |= 0x04;
        const float value = valid ? point.value : 0.0f;
        quint32 raw = 0;
        static_assert(sizeof(raw) == sizeof(value));
        std::memcpy(&raw, &value, sizeof(raw));
        regs.append(point.id);
        regs.append(flags);
        regs.append(static_cast<quint16>(raw >> 16));
        regs.append(static_cast<quint16>(raw & 0xffff));
        for (int i = 0; i < 4; ++i)
            regs.append(0);
    }
    return regs;
}

QModbusResponse ModbusV1Server::processRequest(const QModbusPdu &request)
{
    const auto function = request.functionCode();
    if (function != QModbusPdu::ReadHoldingRegisters)
        return QModbusExceptionResponse(function, QModbusPdu::IllegalFunction);
    if (request.dataSize() != 4)
        return QModbusExceptionResponse(function, QModbusPdu::IllegalDataValue);

    quint16 address = 0;
    quint16 quantity = 0;
    request.decodeData(&address, &quantity);
    if (quantity == 0 || quantity > kMaxReadRegisters)
        return QModbusExceptionResponse(function, QModbusPdu::IllegalDataValue);

    if (address == 0 && quantity == kHeaderRegisters) {
        m_latchedRegisters = encodeSnapshot(m_source.snapshot());
        m_latchAge.start();
    } else if (m_latchedRegisters.isEmpty() || !m_latchAge.isValid()
               || m_latchAge.hasExpired(m_snapshotLeaseMs)) {
        m_latchedRegisters.clear();
        return QModbusExceptionResponse(function, QModbusPdu::ServerDeviceBusy);
    }

    const int end = static_cast<int>(address) + quantity;
    if (end > m_latchedRegisters.size())
        return QModbusExceptionResponse(function, QModbusPdu::IllegalDataAddress);

    QByteArray data;
    data.reserve(1 + quantity * 2);
    data.append(static_cast<char>(quantity * 2));
    for (int i = address; i < end; ++i) {
        const quint16 value = m_latchedRegisters.at(i);
        data.append(static_cast<char>(value >> 8));
        data.append(static_cast<char>(value & 0xff));
    }
    return QModbusResponse(function, data);
}

} // namespace CentralLogger::Adapters
