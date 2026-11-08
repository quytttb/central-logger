#include "core/sensors/LoggerOverviewTableModel.h"

#include <QSignalSpy>
#include <QTest>

using CentralLogger::Core::LoggerOverviewTableModel;
using CentralLogger::Core::OverviewColumn;
using CentralLogger::Core::OverviewRow;

class TestLoggerOverviewTableModel : public QObject
{
    Q_OBJECT

private:
    static OverviewColumn column(int edgeSensorId, const QString &header)
    {
        OverviewColumn col;
        col.edgeSensorId = edgeSensorId;
        col.header = header;
        return col;
    }

    static OverviewRow row(qint64 id, const QString &name, bool online,
                           const QHash<int, QString> &values)
    {
        OverviewRow r;
        r.loggerId = id;
        r.loggerName = name;
        r.status = online ? QStringLiteral("online") : QStringLiteral("offline");
        r.online = online;
        r.values = values;
        return r;
    }

private slots:
    void emptyModel_hasNoRowsAndOnlyLoggerColumn()
    {
        LoggerOverviewTableModel model;
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.columnCount(), 0); // no sensor columns → table hidden
    }

    void setData_exposesLoggerColumnAndSensorValues()
    {
        LoggerOverviewTableModel model;
        const QVector<OverviewColumn> columns = {
            column(1, QStringLiteral("O2 (%V)")),
            column(2, QStringLiteral("CO (mg/Nm3)")),
        };
        const QVector<OverviewRow> rows = {
            row(10, QStringLiteral("Logger A"), true, { { 1, "11.70" }, { 2, "22.96" } }),
            row(11, QStringLiteral("Logger B"), false, { { 1, "9.50" } }),
        };

        model.setData(rows, columns);

        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.columnCount(), 3);
        QCOMPARE(model.sensorColumnCount(), 2);
        QCOMPARE(model.rowsSize(), 2);

        QCOMPARE(model.headerData(0, Qt::Horizontal, Qt::DisplayRole).toString(),
                 QStringLiteral("Logger"));
        QCOMPARE(model.headerData(1, Qt::Horizontal, Qt::DisplayRole).toString(),
                 QStringLiteral("O2 (%V)"));
        QCOMPARE(model.headerData(2, Qt::Horizontal, Qt::DisplayRole).toString(),
                 QStringLiteral("CO (mg/Nm3)"));

        QCOMPARE(model.data(model.index(0, 0), Qt::DisplayRole).toString(),
                 QStringLiteral("Logger A"));
        QCOMPARE(model.data(model.index(0, 1), Qt::DisplayRole).toString(),
                 QStringLiteral("11.70"));
        // ItemDelegate has a FINAL `display` property, so QML reads "value".
        QCOMPARE(model.data(model.index(0, 1), LoggerOverviewTableModel::ValueRole).toString(),
                 QStringLiteral("11.70"));
        // Logger B has no CO value → empty cell, not "0".
        QCOMPARE(model.data(model.index(1, 2), Qt::DisplayRole).toString(), QString());
        QCOMPARE(model.data(model.index(1, 0), LoggerOverviewTableModel::OnlineRole).toBool(),
                 false);
        QCOMPARE(model.data(model.index(0, 0), LoggerOverviewTableModel::LoggerIdRole).toLongLong(),
                 10);
    }

    void sameColumns_reusesModelAndEmitsDataChanged()
    {
        LoggerOverviewTableModel model;
        const QVector<OverviewColumn> columns = { column(1, QStringLiteral("O2 (%V)")) };
        model.setData({ row(10, QStringLiteral("Logger A"), true, { { 1, "1.0" } }) },
                      columns);

        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
        QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);

        model.setData({ row(10, QStringLiteral("Logger A"), false, { { 1, "2.0" } }) },
                      columns);

        QCOMPARE(resetSpy.count(), 0);
        QCOMPARE(changedSpy.count(), 1);
        QCOMPARE(model.data(model.index(0, 1), Qt::DisplayRole).toString(),
                 QStringLiteral("2.0"));
    }

    void changedColumns_resetModel()
    {
        LoggerOverviewTableModel model;
        model.setData({ row(10, QStringLiteral("Logger A"), true, { { 1, "1.0" } }) },
                      { column(1, QStringLiteral("O2 (%V)")) });

        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
        QSignalSpy columnsSpy(&model, &LoggerOverviewTableModel::columnsChanged);

        model.setData({ row(10, QStringLiteral("Logger A"), true, { { 2, "3.0" } }) },
                      { column(2, QStringLiteral("CO (mg/Nm3)")) });

        QCOMPARE(resetSpy.count(), 1);
        QCOMPARE(columnsSpy.count(), 1);
        QCOMPARE(model.columnCount(), 2);
        QCOMPARE(model.headerData(1, Qt::Horizontal, Qt::DisplayRole).toString(),
                 QStringLiteral("CO (mg/Nm3)"));
    }

    void clear_removesRowsAndColumns()
    {
        LoggerOverviewTableModel model;
        model.setData({ row(10, QStringLiteral("Logger A"), true, { { 1, "1.0" } }) },
                      { column(1, QStringLiteral("O2 (%V)")) });
        model.clear();
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.columnCount(), 0);
    }
};

QTEST_GUILESS_MAIN(TestLoggerOverviewTableModel)
#include "test_logger_overview_table_model.moc"
