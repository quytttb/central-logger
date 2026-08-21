#include "core/sensors/SensorMonitoringTableModel.h"

#include "utils/UiConstants.h"

namespace CentralLogger::Core {

namespace {

QVariantList toVariantList(const QStringList &strings)
{
    QVariantList out;
    out.reserve(strings.size());
    for (const QString &s : strings) {
        out.append(s);
    }
    return out;
}

} // namespace

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
    case SensorIdRole:           return row.edgeSensorId;
    case NameRole:               return row.name;
    case ValueRole:              return row.value;
    case UnitRole:               return row.unit;
    case DisplayStatusRole:      return row.displayStatus;
    case AttachDiTypeCodesRole:  return toVariantList(row.attachDiTypeCodes);
    case AttachDiTypeLabelsRole: return toVariantList(row.attachDiTypeLabels);
    case AlarmTypeRole:          return row.alarmType;
    case SensorTypeRole:         return row.sensorType;
    case ValidRole:              return row.valid;
    case AlarmRole:              return row.alarm;
    case StaleRole:              return row.stale;
    case TimestampRole:          return row.timestamp;
    default:                     return {};
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
        { Qt::DisplayRole,          CentralLogger::Ui::kRoleDisplay },
        { SensorIdRole,             CentralLogger::Ui::kRoleSensorId },
        { NameRole,                 CentralLogger::Ui::kRoleName },
        { ValueRole,                CentralLogger::Ui::kRoleValue },
        { UnitRole,                 CentralLogger::Ui::kRoleUnit },
        { DisplayStatusRole,        CentralLogger::Ui::kRoleDisplayStatus },
        { AttachDiTypeCodesRole,    CentralLogger::Ui::kRoleAttachDiCodes },
        { AttachDiTypeLabelsRole,   CentralLogger::Ui::kRoleAttachDiLabels },
        { AlarmTypeRole,            CentralLogger::Ui::kRoleAlarmType },
        { SensorTypeRole,           CentralLogger::Ui::kRoleSensorType },
        { ValidRole,                CentralLogger::Ui::kRoleValid },
        { AlarmRole,                CentralLogger::Ui::kRoleAlarm },
        { StaleRole,                CentralLogger::Ui::kRoleStale },
        { TimestampRole,            CentralLogger::Ui::kRoleTimestamp },
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
        // Emit dataChanged for every role the QML delegate binds to,
        // including ones that rarely change (NameRole, UnitRole,
        // TimestampRole, SensorTypeRole). Otherwise, when a Fetch-config
        // rewrites the catalog's display name / unit / timestamp for an
        // already-known sensor, the existing row keeps stale text.
        const QVector<int> rowRoles = {
            Qt::DisplayRole,
            SensorIdRole,
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
        emit dataChanged(index(0, 0),
                         index(oldCount - 1, ColumnCount - 1),
                         rowRoles);
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
