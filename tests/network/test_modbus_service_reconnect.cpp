#include "network/modbus/ModbusService.h"

#include <QModbusTcpServer>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <algorithm>

using namespace CentralLogger::Network;

namespace {

LoggerRuntimeConfig configFor(quint16 port)
{
    LoggerRuntimeConfig cfg;
    cfg.loggerId = 1;
    cfg.host = QStringLiteral("127.0.0.1");
    cfg.modbusPort = port;
    cfg.pollIntervalMs = 60;
    cfg.timeoutMs = 160;
    return cfg;
}

bool startModbusServer(QModbusTcpServer &server, quint16 port)
{
    server.setServerAddress(1);
    server.setConnectionParameter(QModbusDevice::NetworkAddressParameter, QStringLiteral("127.0.0.1"));
    server.setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
    if (!server.setMap({{QModbusDataUnit::HoldingRegisters,
                         QModbusDataUnit(QModbusDataUnit::HoldingRegisters, 0, 10)}})
        || !server.setData(QModbusDataUnit::HoldingRegisters, 0, 1))
        return false;
    return server.connectDevice();
}

bool sawPoll(const QSignalSpy &spy, bool success)
{
    return std::any_of(spy.cbegin(), spy.cend(), [success](const auto &signal) {
        return signal.first().template value<PollSnapshot>().success == success;
    });
}

} // namespace

class TestModbusServiceReconnect : public QObject
{
    Q_OBJECT

private slots:
    void resumesAfterServerRestarts()
    {
        QTcpServer portReservation;
        QVERIFY(portReservation.listen(QHostAddress::LocalHost));
        const auto port = portReservation.serverPort();
        portReservation.close();
        QModbusTcpServer server;
        QVERIFY(startModbusServer(server, port));

        ModbusService service;
        QSignalSpy polls(&service, &ModbusService::pollFinished);
        service.syncLoggers({configFor(port)});
        QTRY_VERIFY_WITH_TIMEOUT(sawPoll(polls, true), 3000);

        server.disconnectDevice();
        polls.clear();
        QTRY_VERIFY_WITH_TIMEOUT(sawPoll(polls, false), 3000);
        QVERIFY(!sawPoll(polls, true));

        QVERIFY(server.connectDevice());
        QTRY_VERIFY_WITH_TIMEOUT(sawPoll(polls, true), 3000);
    }

    void replacesSilentTcpPeerAndDoesNotPollDisabledLogger()
    {
        QTcpServer silent;
        QVERIFY(silent.listen(QHostAddress::LocalHost));
        const auto port = silent.serverPort();
        int connects = 0;
        connect(&silent, &QTcpServer::newConnection, &silent, [&]() {
            while (auto *socket = silent.nextPendingConnection()) {
                ++connects;
                // Accept TCP but never answer an FC03 request.
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });

        ModbusService service;
        QSignalSpy polls(&service, &ModbusService::pollFinished);
        auto cfg = configFor(port);
        service.syncLoggers({cfg});
        QTRY_VERIFY_WITH_TIMEOUT(sawPoll(polls, false), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(connects >= 2, 3000);
        QVERIFY(!sawPoll(polls, true));

        cfg.enabled = false;
        service.syncLoggers({cfg});
        const auto before = polls.size();
        service.reconnectLogger(cfg.loggerId);
        QTest::qWait(cfg.timeoutMs + cfg.pollIntervalMs);
        QCOMPARE(polls.size(), before);

        silent.close();
        QModbusTcpServer server;
        QVERIFY(startModbusServer(server, port));
        cfg.enabled = true;
        service.syncLoggers({cfg});
        QTRY_VERIFY_WITH_TIMEOUT(sawPoll(polls, true), 3000);
    }
};

QTEST_GUILESS_MAIN(TestModbusServiceReconnect)
#include "test_modbus_service_reconnect.moc"
