#pragma once

#include "EdgeSource.h"

#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QTcpServer>

namespace CentralLogger::Adapters {

struct VirtualStation
{
    QString stationCode;
    QString stationName;
    int pollIntervalSeconds = 0;
    QHostAddress modbusBind;
    int modbusUnitId = 1;
};

// Minimal, bounded HTTP/1.1 server for the four v1 routes Central uses.
// It never sends a request to the upstream device.
class RestV1Server final : public QObject
{
    Q_OBJECT

public:
    RestV1Server(const EdgeSource &source, VirtualStation station,
                 const QByteArray &bearerToken, QObject *parent = nullptr);

    bool listen(const QHostAddress &address, quint16 port);
    quint16 serverPort() const { return m_server.serverPort(); }
    QString errorString() const { return m_server.errorString(); }

private:
    void acceptConnections();
    QJsonObject configResponse();
    void updateRevision(const QJsonObject &config);
    bool authorized(const QByteArray &header) const;

    const EdgeSource &m_source;
    VirtualStation m_station;
    QByteArray m_tokenHash;
    QByteArray m_configHash;
    int m_revision;
    int m_clients = 0;
    QTcpServer m_server;
};

} // namespace CentralLogger::Adapters
