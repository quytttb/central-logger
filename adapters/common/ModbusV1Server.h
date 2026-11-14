#pragma once

#include "EdgeSource.h"

#include <QElapsedTimer>
#include <QModbusTcpServer>
#include <QPointer>

class QTcpSocket;

namespace CentralLogger::Adapters {

// One active TCP client at a time. Qt's Modbus request callback has no client
// argument, so this constraint lets us pin a snapshot across all of that
// client's header/chunk reads without relying on Qt private internals.
class ModbusV1Server final : public QModbusTcpServer
{
    Q_OBJECT

public:
    explicit ModbusV1Server(const EdgeSource &source, int snapshotLeaseMs,
                            QObject *parent = nullptr);
    ~ModbusV1Server() override;

    static QVector<quint16> encodeSnapshot(const EdgeSnapshot &snapshot);

protected:
    QModbusResponse processRequest(const QModbusPdu &request) override;

private:
    class ConnectionObserver;
    bool acceptNewConnection(QTcpSocket *newClient);

    const EdgeSource &m_source;
    const int m_snapshotLeaseMs;
    QPointer<QTcpSocket> m_client;
    QVector<quint16> m_latchedRegisters;
    QElapsedTimer m_latchAge;
};

} // namespace CentralLogger::Adapters
