#include "HistoryTableModel.h"

#include "utils/AppConstants.h"
#include "utils/FormatConstants.h"
#include "utils/SensorConstants.h"
#include "utils/UiConstants.h"

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
    case TimeRole:      return r.recordedAt.toLocalTime()
                            .toString(QLatin1String(CentralLogger::Format::kDateTimeDdMmYyyyHms));
    case LoggerRole:    return r.loggerName;
    case SensorRole:    return r.sensorName;
    case UnitRole:      return r.unit;
    case ValueRole:     return valueText(r);
    case StatusRole:    return statusText(r);
    case StatusCodeRole:return r.statusCode;
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
        { Qt::DisplayRole, CentralLogger::Ui::kRoleDisplay },
        { TimeRole,     CentralLogger::Ui::kRoleTime     },
        { LoggerRole,   CentralLogger::Ui::kRoleLogger   },
        { SensorRole,   CentralLogger::Ui::kRoleSensor   },
        { UnitRole,     CentralLogger::Ui::kRoleUnit     },
        { ValueRole,    CentralLogger::Ui::kRoleValue    },
        { StatusRole,   CentralLogger::Ui::kRoleStatus   },
        { StatusCodeRole, CentralLogger::Ui::kRoleStatusCode },
        { SensorIdRole, CentralLogger::Ui::kRoleSensorId },
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

QString HistoryTableModel::valueText(const Data::HistoryRow &r)
{
    if (r.statusCode.isEmpty())
        return QString::fromUtf8(CentralLogger::Sensor::kValuePlaceholder);
    return QString::number(r.value, 'f',
                           qBound(CentralLogger::Defaults::kDecimalsMin,
                                  r.decimals,
                                  CentralLogger::Defaults::kDecimalsMax));
}

QString HistoryTableModel::statusText(const Data::HistoryRow &r)
{
    return r.statusCode;
}

} // namespace CentralLogger::Core
