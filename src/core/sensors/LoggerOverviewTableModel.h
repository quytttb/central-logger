#pragma once

#include <QAbstractTableModel>
#include <QHash>
#include <QPair>
#include <QString>
#include <QVector>
#include <QtQmlIntegration/qqmlintegration.h>

namespace CentralLogger::Core {

/// One sensor column in the logger overview table (union across loggers).
struct OverviewColumn
{
    int     edgeSensorId = 0;
    QString header;      // e.g. "O2 (%V)" — name + unit
    QString sensorName;
    QString sensorUnit;
    bool matchMetadata = false;
};

/// One logger row: logger identity + formatted analog values keyed by
/// edge_sensor_id. Missing key = no cached reading for that sensor.
struct OverviewRow
{
    qint64                loggerId = 0;
    QString               loggerName;
    QString               status;     // "online" | "offline" | ...
    bool                  online = false;
    QHash<int, QString>   values;     // edge_sensor_id → formatted value
    QHash<int, QPair<QString, QString>> identities; // edge ID → (name, unit)
};

/// Dashboard summary table: one row per secondary logger, one dynamic column
/// per analog sensor (union of all cached snapshots). Column 0 is the logger
/// name. Fed by `DashboardController` from `SensorSnapshotCache`; no DB.
class LoggerOverviewTableModel : public QAbstractTableModel
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(int rowsSize READ rowsSize NOTIFY rowsSizeChanged)
    Q_PROPERTY(int sensorColumnCount READ sensorColumnCount NOTIFY columnsChanged)

public:
    enum Role {
        DisplayRole = Qt::DisplayRole,
        LoggerIdRole = Qt::UserRole + 1,
        LoggerNameRole,
        StatusRole,
        OnlineRole,
        IsLoggerColumnRole,
        /// Formatted cell text (logger name or sensor value). Exposed under
        /// "value" because ItemDelegate already has a FINAL `display` property.
        ValueRole,
    };
    Q_ENUM(Role)

    explicit LoggerOverviewTableModel(QObject *parent = nullptr);

    int      rowCount(const QModelIndex &parent = {}) const override;
    int      columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Replaces rows + dynamic columns. A change in the column layout or row
    /// count resets the model; otherwise only dataChanged is emitted.
    void setData(const QVector<OverviewRow> &rows,
                 const QVector<OverviewColumn> &columns);

    void clear();

    int rowsSize() const { return m_rows.size(); }
    int sensorColumnCount() const { return m_columns.size(); }

signals:
    void rowsSizeChanged();
    void columnsChanged();

private:
    QVector<OverviewRow>    m_rows;
    QVector<OverviewColumn> m_columns;
};

} // namespace CentralLogger::Core
