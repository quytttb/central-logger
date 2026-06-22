#include "HistoryTableModel.h"

namespace CentralLogger::Core {

namespace {

constexpr const char *kHeaders[] = {
    "Time", "Logger", "Sensor", "Unit", "Value", "Status"
};
static_assert(sizeof(kHeaders) / sizeof(kHeaders[0]) == HistoryTableModel::ColumnCount,
              "kHeaders size mismatch");

} // namespace

HistoryTableModel::HistoryTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{}

int HistoryTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int HistoryTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant HistoryTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const Data::HistoryRow &r = m_rows.at(index.row());

    if (role == Qt::DisplayRole)
        role = static_cast<int>(TimeRole) + index.column();

    switch (role) {
    case TimeRole:      return r.recordedAt.toLocalTime().toString(QStringLiteral("dd/MM/yyyy HH:mm:ss"));
    case LoggerRole:    return r.loggerName;
    case SensorRole:    return r.sensorName;
    case UnitRole:      return r.unit;
    case ValueRole:     return QString::number(r.value, 'f', qBound(0, r.decimals, 6));
    case StatusRole:    return statusText(r);
    case ValidRole:     return r.valid;
    case AlarmRole:     return r.alarm;
    case StaleRole:     return r.stale;
    case SensorIdRole:  return r.sensorId;
    default:            return {};
    }
}

QVariant HistoryTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    if (section < 0 || section >= ColumnCount)
        return {};
    return QString::fromLatin1(kHeaders[section]);
}

QHash<int, QByteArray> HistoryTableModel::roleNames() const
{
    return {
        { Qt::DisplayRole, "display" },
        { TimeRole,     "time"     },
        { LoggerRole,   "logger"   },
        { SensorRole,   "sensor"   },
        { UnitRole,     "unit"     },
        { ValueRole,    "value"    },
        { StatusRole,   "status"   },
        { ValidRole,    "valid"    },
        { AlarmRole,    "alarm"    },
        { StaleRole,    "stale"    },
        { SensorIdRole, "sensorId" },
    };
}

void HistoryTableModel::setRows(const QVector<Data::HistoryRow> &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
    emit rowsSizeChanged();
}

void HistoryTableModel::clear()
{
    if (m_rows.isEmpty())
        return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
    emit rowsSizeChanged();
}

QString HistoryTableModel::statusText(const Data::HistoryRow &r)
{
    if (!r.valid)  return QStringLiteral("INVALID");
    if (r.stale)   return QStringLiteral("STALE");
    if (r.alarm)   return QStringLiteral("ALARM");
    return QStringLiteral("OK");
}

} // namespace CentralLogger::Core
