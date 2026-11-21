#include "core/history/HistoryTableModel.h"

#include <QSignalSpy>
#include <QTest>

using CentralLogger::Core::HistoryTableModel;
using CentralLogger::Data::HistoryRow;

namespace {

HistoryRow reading(qint64 id, const QDateTime &ts, double value = 1.0)
{
    HistoryRow row;
    row.readingId = id;
    row.recordedAt = ts;
    row.value = value;
    row.statusCode = QStringLiteral("00");
    return row;
}

} // namespace

class TestHistoryTableModel : public QObject {
    Q_OBJECT

private slots:
    void refreshKeepsIdentityAndHandlesSameTimestamp()
    {
        HistoryTableModel model;
        const auto ts = QDateTime::currentDateTimeUtc();
        const auto first = reading(4, ts);
        const auto next = reading(3, ts);
        const auto older = reading(2, ts.addSecs(-1));
        const auto expired = reading(1, ts.addSecs(-2));
        model.setRows({first, next, older, expired});

        QPersistentModelIndex retained(model.index(1, HistoryTableModel::ValueColumn));
        QSignalSpy inserts(&model, &HistoryTableModel::rowsInserted);
        QSignalSpy removals(&model, &HistoryTableModel::rowsRemoved);
        QSignalSpy changes(&model, &HistoryTableModel::dataChanged);
        QSignalSpy resets(&model, &HistoryTableModel::modelReset);
        QSignalSpy sizeChanges(&model, &HistoryTableModel::rowsSizeChanged);

        auto updated = next;
        updated.value = 2.0;
        const QVector<HistoryRow> latest{reading(5, ts), first, updated, older};
        model.mergeLatest(latest);
        QCOMPARE(inserts.size(), 1);
        QCOMPARE(removals.size(), 1);
        QCOMPARE(changes.size(), 1);
        QCOMPARE(sizeChanges.size(), 0);
        QCOMPARE(resets.size(), 0);
        QVERIFY(retained.isValid());
        QCOMPARE(retained.row(), 2);
        QCOMPARE(retained.data().toString(), QStringLiteral("2.0000"));
        QCOMPARE(model.readingIdAt(2), 3);
        QCOMPARE(model.rowForReadingId(3), 2);
        QCOMPARE(model.rowForReadingId(1), -1);
        QVERIFY(model.rows() == latest);

        model.mergeLatest(latest);
        QCOMPARE(inserts.size(), 1);
        QCOMPARE(removals.size(), 1);
        QCOMPARE(changes.size(), 1);
    }

    void interleavedChangesAndEmptyResultDoNotReset()
    {
        HistoryTableModel model;
        const auto ts = QDateTime::currentDateTimeUtc();
        model.setRows({reading(6, ts), reading(4, ts), reading(2, ts)});
        QSignalSpy resets(&model, &HistoryTableModel::modelReset);
        const QVector<HistoryRow> latest{reading(5, ts), reading(4, ts), reading(3, ts), reading(1, ts)};
        model.mergeLatest(latest);
        QVERIFY(model.rows() == latest);
        model.mergeLatest({});
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(resets.size(), 0);
    }
};

QTEST_GUILESS_MAIN(TestHistoryTableModel)
#include "test_history_table_model.moc"
