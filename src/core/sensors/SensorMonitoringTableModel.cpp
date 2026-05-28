#include "core/sensors/SensorMonitoringTableModel.h"

namespace CentralLogger::Core {

SensorMonitoringTableModel::SensorMonitoringTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int SensorMonitoringTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int SensorMonitoringTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(ColumnCount);
}

QVariant SensorMonitoringTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }
    const auto &row = m_rows.at(index.row());

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case SensorIdColumn:      return row.edgeSensorId;
        case NameColumn:          return row.name;
        case ValueColumn:         return row.value;
        case UnitColumn:          return row.unit;
        case DisplayStatusColumn: return row.displayStatus;
        default:                  return {};
        }
    }

    switch (role) {
    case SensorIdRole:      return row.edgeSensorId;
    case NameRole:          return row.name;
    case ValueRole:         return row.value;
    case UnitRole:          return row.unit;
    case DisplayStatusRole: return row.displayStatus;
    case DiStatusCodeRole:  return row.diStatusCode;
    case AlarmTypeRole:     return row.alarmType;
    case ShowAlarmBadgeRole:return row.showAlarmBadge;
    case SensorTypeRole:    return row.sensorType;
    case ValidRole:         return row.valid;
    case AlarmRole:         return row.alarm;
    case StaleRole:         return row.stale;
    case TimestampRole:     return row.timestamp;
    default:                return {};
    }
}

QVariant SensorMonitoringTableModel::headerData(int section,
                                                Qt::Orientation orientation,
                                                int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }
    switch (section) {
    case SensorIdColumn:      return tr("ID");
    case NameColumn:          return tr("Name");
    case ValueColumn:         return tr("Value");
    case UnitColumn:          return tr("Unit");
    case DisplayStatusColumn: return tr("Status");
    default:                  return {};
    }
}

QHash<int, QByteArray> SensorMonitoringTableModel::roleNames() const
{
    return {
        { Qt::DisplayRole,   "display" },
        { SensorIdRole,      "sensorId" },
        { NameRole,          "name" },
        { ValueRole,         "value" },
        { UnitRole,          "unit" },
        { DisplayStatusRole, "displayStatus" },
        { DiStatusCodeRole,  "diStatusCode" },
        { AlarmTypeRole,     "alarmType" },
        { ShowAlarmBadgeRole,"showAlarmBadge" },
        { SensorTypeRole,    "sensorType" },
        { ValidRole,         "valid" },
        { AlarmRole,         "alarm" },
        { StaleRole,         "stale" },
        { TimestampRole,     "timestamp" },
    };
}

void SensorMonitoringTableModel::setLoggerId(qint64 id)
{
    if (m_loggerId == id) return;
    m_loggerId = id;
    clear();
    emit loggerIdChanged();
}

void SensorMonitoringTableModel::setRows(const QVector<SensorLiveRow> &rows)
{
    const int oldCount = m_rows.size();
    const int newCount = rows.size();

    if (oldCount == newCount && oldCount > 0) {
        m_rows = rows;
        emit dataChanged(index(0, 0),
                         index(oldCount - 1, ColumnCount - 1));
        return;
    }

    beginResetModel();
    m_rows = rows;
    endResetModel();
    emit rowsSizeChanged();
}

void SensorMonitoringTableModel::clear()
{
    if (m_rows.isEmpty()) return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
    emit rowsSizeChanged();
}

} // namespace CentralLogger::Core
