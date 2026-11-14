#include "RestV1Server.h"

#include "utils/AdapterConstants.h"
#include "utils/Version.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <QTimer>
#include <limits>
#include <memory>

namespace CentralLogger::Adapters {

namespace {
using namespace CentralLogger::AdapterDefaults;

struct RequestState {
    QByteArray buffer;
    qsizetype bodyStart = -1;
    qsizetype bodySize = 0;
    QByteArray method;
    QByteArray path;
    QByteArray authorization;
    bool answered = false;
};

QByteArray jsonBody(const QString &detail)
{
    return QJsonDocument(QJsonObject{{QStringLiteral("detail"), detail}})
        .toJson(QJsonDocument::Compact);
}

void respond(QTcpSocket *socket, RequestState &state, int status,
             const QByteArray &reason, const QByteArray &body)
{
    if (state.answered)
        return;
    state.answered = true;
    QByteArray response = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason
        + "\r\nContent-Type: application/json\r\nCache-Control: no-store\r\n"
          "X-Content-Type-Options: nosniff\r\nConnection: close\r\nContent-Length: "
        + QByteArray::number(body.size()) + "\r\n\r\n" + body;
    socket->write(response);
    socket->disconnectFromHost();
}

bool parseHeaders(RequestState &state)
{
    const qsizetype end = state.buffer.indexOf("\r\n\r\n");
    if (end < 0)
        return false;
    const auto lines = state.buffer.first(end).split('\n');
    if (lines.isEmpty())
        return false;
    const auto first = lines.first().trimmed().split(' ');
    if (first.size() != 3 || (first.at(2) != "HTTP/1.1" && first.at(2) != "HTTP/1.0"))
        return false;
    state.method = first.at(0);
    state.path = first.at(1);

    bool contentLengthSeen = false;
    bool authorizationSeen = false;
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        const qsizetype colon = line.indexOf(':');
        if (colon <= 0)
            return false;
        const auto key = line.first(colon).trimmed().toLower();
        const auto value = line.sliced(colon + 1).trimmed();
        if (key == "transfer-encoding")
            return false;
        if (key == "content-length") {
            if (contentLengthSeen || value.isEmpty())
                return false;
            contentLengthSeen = true;
            for (const char c : value) {
                if (c < '0' || c > '9')
                    return false;
            }
            bool ok = false;
            const qlonglong parsed = value.toLongLong(&ok);
            if (!ok || parsed < 0 || parsed > kMaxHttpBodyBytes)
                return false;
            state.bodySize = static_cast<qsizetype>(parsed);
        } else if (key == "authorization") {
            if (authorizationSeen)
                return false;
            authorizationSeen = true;
            state.authorization = value;
        }
    }
    if (state.method == "POST" && !contentLengthSeen)
        return false;
    state.bodyStart = end + 4;
    return true;
}
} // namespace

RestV1Server::RestV1Server(const EdgeSource &source, VirtualStation station,
                           const QByteArray &bearerToken, QObject *parent)
    : QObject(parent), m_source(source), m_station(std::move(station)),
      m_tokenHash(QCryptographicHash::hash(bearerToken, QCryptographicHash::Sha256)),
      m_revision(QRandomGenerator::global()->bounded(1, std::numeric_limits<int>::max() / 2))
{
    connect(&m_server, &QTcpServer::newConnection, this, &RestV1Server::acceptConnections);
}

bool RestV1Server::listen(const QHostAddress &address, quint16 port)
{
    return m_server.listen(address, port);
}

bool RestV1Server::authorized(const QByteArray &header) const
{
    if (!header.startsWith("Bearer ") || header.size() <= 7)
        return false;
    const auto hash = QCryptographicHash::hash(header.sliced(7), QCryptographicHash::Sha256);
    unsigned char diff = 0;
    for (qsizetype i = 0; i < hash.size(); ++i)
        diff |= static_cast<unsigned char>(hash.at(i) ^ m_tokenHash.at(i));
    return diff == 0;
}

void RestV1Server::updateRevision(const QJsonObject &config)
{
    const auto hash = QCryptographicHash::hash(
        QJsonDocument(config).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256);
    if (!m_configHash.isEmpty() && hash != m_configHash) {
        m_revision = m_revision == std::numeric_limits<int>::max() ? 1 : m_revision + 1;
    }
    m_configHash = hash;
}

QJsonObject RestV1Server::configResponse()
{
    const auto snapshot = m_source.snapshot();
    QJsonArray sensors;
    for (const auto &point : snapshot.analogs) {
        sensors.append(QJsonObject{
            {QStringLiteral("sensor_id"), point.id},
            {QStringLiteral("sensor_type"), QStringLiteral("ANALOG")},
            {QStringLiteral("name"), point.name},
            {QStringLiteral("unit"), point.unit},
            {QStringLiteral("active"), point.active}
        });
    }
    QJsonObject config{
        {QStringLiteral("station_code"), m_station.stationCode},
        {QStringLiteral("station_name"), m_station.stationName},
        {QStringLiteral("poll_interval"), m_station.pollIntervalSeconds},
        {QStringLiteral("modbus_tcp_bind"), m_station.modbusBind.toString()},
        {QStringLiteral("modbus_tcp_enabled"), true},
        {QStringLiteral("modbus_tcp_unit_id"), m_station.modbusUnitId},
        {QStringLiteral("sensors"), sensors}
    };
    updateRevision(config);
    return {{QStringLiteral("revision"), m_revision}, {QStringLiteral("config"), config}};
}

void RestV1Server::acceptConnections()
{
    while (auto *socket = m_server.nextPendingConnection()) {
        if (m_clients >= kMaxHttpClients) {
            socket->disconnectFromHost();
            socket->deleteLater();
            continue;
        }
        ++m_clients;
        auto state = std::make_shared<RequestState>();
        socket->setReadBufferSize(kMaxHttpHeaderBytes + kMaxHttpBodyBytes + 1);
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            --m_clients;
            socket->deleteLater();
        });
        auto *timer = new QTimer(socket);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, socket, &QTcpSocket::abort);
        timer->start(kHttpRequestTimeoutMs);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket, state]() {
            if (state->answered)
                return;
            state->buffer.append(socket->readAll());
            if (state->bodyStart < 0) {
                const auto end = state->buffer.indexOf("\r\n\r\n");
                if ((end < 0 && state->buffer.size() > kMaxHttpHeaderBytes)
                    || end > kMaxHttpHeaderBytes) {
                    respond(socket, *state, 431, "Request Header Fields Too Large",
                            jsonBody(QStringLiteral("Request headers too large.")));
                    return;
                }
                if (end < 0)
                    return;
                if (!parseHeaders(*state)) {
                    respond(socket, *state, 400, "Bad Request",
                            jsonBody(QStringLiteral("Invalid HTTP request.")));
                    return;
                }
            }
            if (state->buffer.size() - state->bodyStart < state->bodySize)
                return;
            if (!authorized(state->authorization)) {
                respond(socket, *state, 401, "Unauthorized",
                        jsonBody(QStringLiteral("Invalid bearer token.")));
                return;
            }

            const QByteArray &method = state->method;
            const QByteArray &path = state->path;
            if (method == "GET" && path == "/api/v1/config") {
                if (!m_source.snapshot().catalogAvailable) {
                    respond(socket, *state, 503, "Service Unavailable",
                            jsonBody(QStringLiteral("Upstream catalog is not ready for this adapter.")));
                    return;
                }
                respond(socket, *state, 200, "OK",
                        QJsonDocument(configResponse()).toJson(QJsonDocument::Compact));
            } else if (method == "POST" && path == "/api/v1/config") {
                const auto body = state->buffer.sliced(state->bodyStart, state->bodySize);
                const auto document = QJsonDocument::fromJson(body);
                const auto request = document.object();
                const auto current = configResponse();
                const auto revision = request.value(QStringLiteral("expected_revision"));
                const auto requestId = request.value(QStringLiteral("request_id"));
                const auto apiVersion = request.value(QStringLiteral("api_version"));
                if (!document.isObject() || !revision.isDouble()
                    || !requestId.isString() || requestId.toString().isEmpty()
                    || requestId.toString().size() > 128
                    || !apiVersion.isDouble()
                    || apiVersion.toInt(-1) != CentralLogger::Version::kApiVersion
                    || !request.value(QStringLiteral("config")).isObject()) {
                    respond(socket, *state, 422, "Unprocessable Content",
                            jsonBody(QStringLiteral("Invalid v1 config envelope.")));
                } else if (revision.toInt(-1) != current.value(QStringLiteral("revision")).toInt()) {
                    respond(socket, *state, 409, "Conflict",
                            jsonBody(QStringLiteral("Revision mismatch; reconnect and reload config.")));
                } else {
                    respond(socket, *state, 422, "Unprocessable Content",
                            jsonBody(QStringLiteral("Adapter is read-only; no changes were applied.")));
                }
            } else if (method == "GET" && path == "/api/v1/readings") {
                respond(socket, *state, 200, "OK",
                        QJsonDocument(m_source.debugReadings()).toJson(QJsonDocument::Compact));
            } else if (method == "GET" && path == "/api/v1/reports/latest") {
                respond(socket, *state, 404, "Not Found",
                        jsonBody(QStringLiteral("No report is provided by this adapter.")));
            } else {
                respond(socket, *state, 404, "Not Found",
                        jsonBody(QStringLiteral("Route not found.")));
            }
        });
    }
}

} // namespace CentralLogger::Adapters
