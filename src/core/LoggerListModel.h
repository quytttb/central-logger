#pragma once

#include "data/repositories/LoggerRepository.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QVector>
#include <QtQmlIntegration/qqmlintegration.h>

namespace CentralLogger::Data {
class Database;
} // namespace CentralLogger::Data

namespace CentralLogger::Core {

/// Read model that mirrors `logger_info` joined with a denormalized sensor
/// count. Owns 6 display columns (Name, Host, Modbus port, Sensor count,
/// Status, Actions placeholder) so `HorizontalHeaderView` and
/// multi-column `TableView` work without any proxy wrapper. Owned by
/// `DashboardController`; exposed to QML as the `loggers` property of that
/// controller (no `QML_ELEMENT` here on purpose).
class LoggerListModel : public QAbstractTableModel
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    // Display columns — matches QML columnWidthProvider indices.
    enum Column {
        NameColumn = 0,
        HostColumn,
        ModbusPortColumn,
        SensorCountColumn,
        StatusColumn,
        ActionsColumn,
        ColumnCount,
    };

    enum Roles {
        IdRole = Qt::UserRole + 1,
        LoggerIdRole,    // "loggerId" — QML-safe alias for IdRole (avoids reserved keyword 'id')
        StationCodeRole,
        NameRole,
        HostRole,
        ModbusPortRole,
        ModbusUnitIdRole,
        ApiPortRole,
        StatusRole,
        SensorCountRole,
        OnlineRole,
        PollingRole,
        AnyAlarmRole,
        RtuConnectedRole,
    };

    explicit LoggerListModel(QObject *parent = nullptr);

    void setDatabase(Data::Database *db) { m_db = db; }

    int      rowCount(const QModelIndex &parent = {}) const override;
    int      columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Reloads from `LoggerRepository::findAllWithSensorCounts()`. Emits
    /// modelReset; safe to call from the UI thread.
    Q_INVOKABLE void reload();

    /// Returns the row index of @p loggerId, or -1 if not found.
    Q_INVOKABLE int indexOfLogger(qint64 loggerId) const;

    /// Patches a single row's live-state columns (status, sensor count,
    /// polling, any-alarm). Used by `ModbusBridge::snapshotApplied` so we
    /// don't reset the whole model every poll cycle.
    Q_INVOKABLE void updateLoggerRow(qint64 loggerId,
                                     const QString &status,
                                     int sensorCount,
                                     bool polling,
                                     bool anyAlarm,
                                     bool rtuConnected);

private:
    struct LiveState {
        bool polling = false;
        bool anyAlarm = false;
        bool rtuConnected = false;
    };

    Data::Database *m_db = nullptr;
    QVector<Data::LoggerListRow> m_rows;
    QVector<LiveState> m_live;
};

} // namespace CentralLogger::Core
