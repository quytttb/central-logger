#include "utils/network/HostValidator.h"

#include <QTest>

using namespace CentralLogger::Utils;

class TestHostValidator : public QObject
{
    Q_OBJECT

private slots:
    void ipv4Accepts();
    void ipv4Rejects();
    void hostnameAccepts();
    void hostnameRejects();
    void isValidHostCombines();
};

void TestHostValidator::ipv4Accepts()
{
    QVERIFY(HostValidator::isValidIpv4(QStringLiteral("192.168.1.50")));
    QVERIFY(HostValidator::isValidIpv4(QStringLiteral("127.0.0.1")));
    QVERIFY(HostValidator::isValidIpv4(QStringLiteral("10.0.0.1")));
}

void TestHostValidator::ipv4Rejects()
{
    QVERIFY(!HostValidator::isValidIpv4(QStringLiteral("999.999.999.999")));
    QVERIFY(!HostValidator::isValidIpv4(QStringLiteral("192.168.1")));
    QVERIFY(!HostValidator::isValidIpv4(QStringLiteral("192.168.1.50:8080")));
    QVERIFY(!HostValidator::isValidIpv4(QStringLiteral("")));
}

void TestHostValidator::hostnameAccepts()
{
    QVERIFY(HostValidator::isValidHostname(QStringLiteral("localhost")));
    QVERIFY(HostValidator::isValidHostname(QStringLiteral("tram-01")));
    QVERIFY(HostValidator::isValidHostname(QStringLiteral("logger.plant.local")));
    QVERIFY(HostValidator::isValidHostname(QStringLiteral("h")));
}

void TestHostValidator::hostnameRejects()
{
    QVERIFY(!HostValidator::isValidHostname(QStringLiteral("http://192.168.1.50")));
    QVERIFY(!HostValidator::isValidHostname(QStringLiteral("192.168.1.50:8080")));
    QVERIFY(!HostValidator::isValidHostname(QStringLiteral("bad host")));
    QVERIFY(!HostValidator::isValidHostname(QStringLiteral("a..b")));
    QVERIFY(!HostValidator::isValidHostname(QStringLiteral("-bad")));
}

void TestHostValidator::isValidHostCombines()
{
    QVERIFY(HostValidator::isValidHost(QStringLiteral("192.168.1.50")));
    QVERIFY(HostValidator::isValidHost(QStringLiteral("edge-logger")));
    QVERIFY(!HostValidator::isValidHost(QStringLiteral("192.168.1")));
    QVERIFY(!HostValidator::isValidHost(QStringLiteral("abc!!!")));
    QVERIFY(!HostValidator::isValidHost(QStringLiteral("  ")));
}

QTEST_MAIN(TestHostValidator)
#include "test_host_validator.moc"
