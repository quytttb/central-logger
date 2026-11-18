#include <QQmlComponent>
#include <QQmlEngine>
#include <QScopedPointer>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(CentralLogger_ComponentsPlugin)
Q_IMPORT_QML_PLUGIN(CentralLogger_ThemePlugin)
Q_IMPORT_QML_PLUGIN(CentralLogger_CorePlugin)
Q_IMPORT_QML_PLUGIN(LoggerKit_ComponentsPlugin)
Q_IMPORT_QML_PLUGIN(LoggerKit_ThemePlugin)

class TestStatusChipQml : public QObject
{
    Q_OBJECT
private slots:
    void englishLabelsAndSourceSpecific03();
};

void TestStatusChipQml::englishLabelsAndSourceSpecific03()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import CentralLogger.Components
        SensorStatusChip {}
    )", QUrl(QStringLiteral("qrc:/StatusChipTest.qml")));
    QScopedPointer<QObject> chip(component.create());
    QVERIFY2(chip, qPrintable(component.errorString()));
    QVERIFY(chip->setProperty("displayStatus", QStringLiteral("00")));
    QCOMPARE(chip->property("chipText").toString(), QStringLiteral("Measuring"));
    QVERIFY(chip->setProperty("alarmType", QStringLiteral("max")));
    QCOMPARE(chip->property("chipText").toString(), QStringLiteral("Measuring"));
    QVERIFY(chip->setProperty("displayStatus", QStringLiteral("01")));
    QCOMPARE(chip->property("chipText").toString(), QStringLiteral("Calibrating"));
    QVERIFY(chip->setProperty("displayStatus", QStringLiteral("02")));
    QCOMPARE(chip->property("chipText").toString(), QStringLiteral("Error"));
    QVERIFY(chip->setProperty("displayStatus", QStringLiteral("03")));
    QCOMPARE(chip->property("chipText").toString(), QStringLiteral("Maintenance"));
    QVERIFY(chip->setProperty("status03Auto", true));
    QCOMPARE(chip->property("chipText").toString(), QStringLiteral("Auto"));
    QVERIFY(chip->setProperty("displayStatus", QStringLiteral("UNKNOWN")));
    QCOMPARE(chip->property("chipText").toString(), QStringLiteral("Unknown"));
}

QTEST_MAIN(TestStatusChipQml)
#include "test_status_chip_qml.moc"
