#include "core/adapters/AdapterProcessManager.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

using CentralLogger::Core::AdapterProcessManager;

class TestAdapterProcessManager : public QObject
{
    Q_OBJECT
private slots:
    void startsAndStopsOnlyOwnedProcesses();
    void rejectsDuplicatePortsWithoutStarting();
};

namespace {
QJsonObject instance(const QString &name, int modbusPort, int restPort)
{
    return {{QStringLiteral("station_code"), name},
            {QStringLiteral("station_name"), name},
            {QStringLiteral("osg_url"), QStringLiteral("http://127.0.0.1:8000")},
            {QStringLiteral("credentials_file"), QStringLiteral("/private/credentials")},
            {QStringLiteral("adapter_token_file"), QStringLiteral("/private/token")},
            {QStringLiteral("quality_policy"), QStringLiteral("/private/policy")},
            {QStringLiteral("modbus_port"), modbusPort},
            {QStringLiteral("rest_port"), restPort}};
}
bool writeConfig(const QString &path, const QJsonArray &instances)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly)
        && file.write(QJsonDocument(QJsonObject{{QStringLiteral("instances"), instances}})
                          .toJson(QJsonDocument::Compact)) > 0;
}
}

void TestAdapterProcessManager::startsAndStopsOnlyOwnedProcesses()
{
#ifdef Q_OS_WIN
    QSKIP("This process fixture uses a Unix shell.");
#else
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString program = dir.filePath(QStringLiteral("fake-adapter"));
    QFile script(program);
    QVERIFY(script.open(QIODevice::WriteOnly));
    QCOMPARE(script.write("#!/bin/sh\nexec sleep 60\n"), qint64(24));
    script.close();
    QVERIFY(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    const QString config = dir.filePath(QStringLiteral("config.json"));
    QVERIFY(writeConfig(config, QJsonArray{instance(QStringLiteral("TEST"), 15020, 18080)}));

    QProcess external;
    external.start(QStringLiteral("/bin/sleep"), {QStringLiteral("60")});
    QVERIFY(external.waitForStarted(3000));
    AdapterProcessManager manager;
    QString error;
    QVERIFY2(manager.startFromFile(config, program, &error), qPrintable(error));
    QCOMPARE(manager.processCount(), 1);
    const auto children = manager.findChildren<QProcess *>();
    QCOMPARE(children.size(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(children.first()->state(), QProcess::Running, 3000);
    manager.stop();
    QCOMPARE(manager.processCount(), 0);
    QCOMPARE(external.state(), QProcess::Running);
    external.terminate();
    QVERIFY(external.waitForFinished(3000));
#endif
}

void TestAdapterProcessManager::rejectsDuplicatePortsWithoutStarting()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString config = dir.filePath(QStringLiteral("config.json"));
    QVERIFY(writeConfig(config, QJsonArray{instance(QStringLiteral("A"), 15020, 18080),
                                           instance(QStringLiteral("B"), 15021, 18080)}));
    AdapterProcessManager manager;
    QString error;
    QVERIFY(!manager.startFromFile(config, QCoreApplication::applicationFilePath(), &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(manager.processCount(), 0);
}

QTEST_GUILESS_MAIN(TestAdapterProcessManager)
#include "test_adapter_process_manager.moc"
