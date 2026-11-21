#pragma once

#include <QByteArray>
#include <QList>
#include <QTcpServer>
#include <QTcpSocket>
#include <memory>
#include <utility>

// Loopback logger REST fixture. Record all request lines, including requests
// that fail authentication or that a read-only adapter would reject.
class ReadOnlyEdgeHttpServer
{
public:
    ReadOnlyEdgeHttpServer()
    {
        m_server.listen(QHostAddress::LocalHost);
        QObject::connect(&m_server, &QTcpServer::newConnection, &m_server, [this]() {
            while (auto *socket = m_server.nextPendingConnection()) {
                QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                auto buffer = std::make_shared<QByteArray>();
                QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer]() {
                    buffer->append(socket->readAll());
                    const auto headerEnd = buffer->indexOf("\r\n\r\n");
                    if (headerEnd < 0)
                        return;
                    const QByteArray line = buffer->left(buffer->indexOf("\r\n"));
                    requests.append(line);
                    headers.append(buffer->left(headerEnd));
                    const bool configRead = line == "GET /api/v1/config HTTP/1.1";
                    const QByteArray status = configRead ? m_status : QByteArray("405 Method Not Allowed");
                    const QByteArray body = configRead ? m_body : QByteArray("{}");
                    socket->write("HTTP/1.1 " + status
                                  + "\r\nContent-Type: application/json\r\nConnection: close\r\n"
                                  + "Content-Length: " + QByteArray::number(body.size())
                                  + "\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
            }
        });
    }

    quint16 port() const { return m_server.serverPort(); }

    void respondWith(QByteArray body, QByteArray status = "200 OK")
    {
        m_body = std::move(body);
        m_status = std::move(status);
    }

    QList<QByteArray> requests;
    QList<QByteArray> headers;

private:
    QByteArray m_body = R"({"revision":5,"thresholds_writable":true,
        "config":{"station_code":"ST-01","station_name":"Edge station",
                  "poll_interval":16,"modbus_tcp_unit_id":3},
        "sensors":[{"sensor_id":1,"sensor_type":"ANALOG","name":"pH",
                    "unit":"pH","min_threshold":6,"max_threshold":9}]})";
    QByteArray m_status = "200 OK";
    QTcpServer m_server;
};
