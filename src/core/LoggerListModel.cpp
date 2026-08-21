#include "LoggerListModel.h"

#include "data/db/Database.h"
#include "utils/SensorConstants.h"
#include "utils/UiConstants.h"

namespace CentralLogger::Core {

LoggerListModel::LoggerListModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int LoggerListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int LoggerListModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(ColumnCount);
}

QVariant LoggerListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }
    const Data::LoggerListRow &row  = m_rows[index.row()];
    const LiveState           &live = m_live[index.row()];

    // Column-specific display value for TableView cells and HorizontalHeaderView.
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case NameColumn:        return row.info.name;
        case HostColumn:        return row.info.host;
        case ModbusPortColumn:  return row.info.modbusPort;
        case SensorCountColumn: return row.sensorCount;
        case StatusColumn:      return row.info.status == CentralLogger::Sensor::kLoggerOnline
                                        ? tr("Online") : tr("Offline");
        case ActionsColumn:     return QString{};
        default:                return {};
        }
    }

    // Row-level named roles — column is intentionally ignored so that every
    // cell in a row can declare the same required property and get the
    // same row-scoped value (mirrors SensorMonitoringTableModel pattern).
    switch (role) {
    case IdRole:
    case LoggerIdRole:     return QVariant::fromValue(row.info.id);
    case StationCodeRole:  return row.info.stationCode;
    case NameRole:         return row.info.name;
    case HostRole:         return row.info.host;
    case ModbusPortRole:   return row.info.modbusPort;
    case ModbusUnitIdRole: return row.info.modbusUnitId;
    case ApiPortRole:      return row.info.apiPort;
    case StatusRole:       return row.info.status;
    case SensorCountRole:  return row.sensorCount;
    case OnlineRole:       return row.info.status == CentralLogger::Sensor::kLoggerOnline;
    case PollingRole:      return live.polling;
    case AnyAlarmRole:     return live.anyAlarm;
    case RtuConnectedRole: return live.rtuConnected;
    default:               return {};
    }
}

QVariant LoggerListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }
    switch (section) {
    case NameColumn:        return tr("Name");
    case HostColumn:        return tr("Host");
    case ModbusPortColumn:  return tr("Modbus");
    case SensorCountColumn: return tr("Sensors");
    case StatusColumn:      return tr("Status");
    case ActionsColumn:     return tr("Actions");
    default:                return {};
    }
}

QHash<int, QByteArray> LoggerListModel::roleNames() const
{
    return {
        { Qt::DisplayRole,  CentralLogger::Ui::kRoleDisplay },  // HorizontalHeaderView + per-column TableView cells
        { IdRole,           CentralLogger::Ui::kRoleId },
        { LoggerIdRole,     CentralLogger::Ui::kRoleLoggerId },  // QML-safe alias — 'id' is reserved keyword
        { StationCodeRole,  CentralLogger::Ui::kRoleStationCode },
        { NameRole,         CentralLogger::Ui::kRoleName },
        { HostRole,         CentralLogger::Ui::kRoleHost },
        { ModbusPortRole,   CentralLogger::Ui::kRoleModbusPort },
        { ModbusUnitIdRole, CentralLogger::Ui::kRoleModbusUnitId },
        { ApiPortRole,      CentralLogger::Ui::kRoleApiPort },
        { StatusRole,       CentralLogger::Ui::kRoleStatus },
        { SensorCountRole,  CentralLogger::Ui::kRoleSensorCount },
        { OnlineRole,       CentralLogger::Ui::kRoleOnline },
        { PollingRole,      CentralLogger::Ui::kRolePolling },
        { AnyAlarmRole,     CentralLogger::Ui::kRoleAnyAlarm },
        { RtuConnectedRole, CentralLogger::Ui::kRoleRtuConnected },
    };
}

void LoggerListModel::reload()
{
    // Preserve previously-known live state across the reset; status comes
    // from the DB (LoggerInfo.status) so we don't need to keep it here.
    QHash<qint64, LiveState> prevLive;
    prevLive.reserve(m_rows.size());
    for (int i = 0; i < m_rows.size(); ++i) {
        prevLive.insert(m_rows[i].info.id, m_live[i]);
    }

    beginResetModel();
    m_rows.clear();
    m_live.clear();
    if (m_db && m_db->isOpen()) {
        Data::LoggerRepository repo(m_db->connection());
        m_rows = repo.findAllWithSensorCounts();
    }
    m_live.resize(m_rows.size());
    for (int i = 0; i < m_rows.size(); ++i) {
        m_live[i] = prevLive.value(m_rows[i].info.id);
    }
    endResetModel();
}

int LoggerListModel::indexOfLogger(qint64 loggerId) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].info.id == loggerId) {
            return i;
        }
    }
    return -1;
}

void LoggerListModel::updateLoggerRow(qint64 loggerId,
                                      const QString &status,
                                      int sensorCount,
                                      bool polling,
                                      bool anyAlarm,
                                      bool rtuConnected)
{
    const int row = indexOfLogger(loggerId);
    if (row < 0) return;

    auto &info = m_rows[row].info;
    auto &live = m_live[row];

    bool changed = false;
    if (info.status != status)       { info.status      = status;      changed = true; }
    if (m_rows[row].sensorCount != sensorCount) {
        m_rows[row].sensorCount = sensorCount;
        changed = true;
    }
    if (live.polling != polling)           { live.polling      = polling;      changed = true; }
    if (live.anyAlarm != anyAlarm)         { live.anyAlarm     = anyAlarm;     changed = true; }
    if (live.rtuConnected != rtuConnected) { live.rtuConnected = rtuConnected; changed = true; }

    if (changed) {
        // Span the full row so every TableView column cell is refreshed.
        const QModelIndex topLeft     = index(row, 0);
        const QModelIndex bottomRight = index(row, ColumnCount - 1);
        // M-11 fix: include Qt::DisplayRole so TableView cells driven by the
        // display role (Status, SensorCount text) repaint on snapshot updates.
        emit dataChanged(topLeft, bottomRight, {
            Qt::DisplayRole,
            StatusRole, SensorCountRole, OnlineRole, PollingRole, AnyAlarmRole,
            RtuConnectedRole,
        });
    }
}

} // namespace CentralLogger::Core
