#pragma once

#include "core/sensors/SensorLiveRow.h"

#include <QAbstractTableModel>
#include <QVector>
#include <QtQmlIntegration/qqmlintegration.h>

namespace CentralLogger::Core {

/// Read-only table model exposing one merged row per sensor for the
/// currently-selected logger. Five columns, plus named roles for QML
/// `TableView` / `Repeater`. Owned by `DashboardController`; rows are
/// pushed via `setRows` when `onSnapshotApplied` matches `loggerId`.
class SensorMonitoringTableModel : public QAbstractTableModel
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(qint64 loggerId READ loggerId WRITE setLoggerId NOTIFY loggerIdChanged)
    Q_PROPERTY(int    rowsSize READ rowsSize NOTIFY rowsSizeChanged)

public:
    enum Column {
        SensorIdColumn = 0,
        NameColumn,
        ValueColumn,
        UnitColumn,
        DisplayStatusColumn,
        ColumnCount,
    };

    enum Role {
        SensorIdRole = Qt::UserRole + 1,
        NameRole,
        ValueRole,
        UnitRole,
        DisplayStatusRole,
        AttachDiTypeCodesRole,
        AttachDiTypeLabelsRole,
        AlarmTypeRole,
        SensorTypeRole,
        ValidRole,
        AlarmRole,
        StaleRole,
        TimestampRole,
    };
    Q_ENUM(Role)

    explicit SensorMonitoringTableModel(QObject *parent = nullptr);

    int      rowCount(const QModelIndex &parent = {}) const override;
    int      columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    qint64 loggerId() const { return m_loggerId; }
    void   setLoggerId(qint64 id);

    /// Replaces the internal vector wholesale and emits dataChanged on
    /// the overlap or layoutChanged when the size changed. Safe to call
    /// from the UI thread only.
    void setRows(const QVector<SensorLiveRow> &rows);

    /// Empties the model.
    void clear();

    int rowsSize() const { return m_rows.size(); }

signals:
    void loggerIdChanged();
    void rowsSizeChanged();

private:
    qint64                  m_loggerId = -1;
    QVector<SensorLiveRow>  m_rows;
};

} // namespace CentralLogger::Core
