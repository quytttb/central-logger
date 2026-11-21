#include "HistoryTableModel.h"

#include "core/SettingsController.h"
#include "utils/AppConstants.h"
#include "utils/FormatConstants.h"
#include "utils/SensorConstants.h"
#include "utils/UiConstants.h"

#include <algorithm>

namespace CentralLogger::Core {

namespace {

constexpr const char *kHeaders[] = {
    "Time", "Logger", "Sensor", "Unit", "Value", "Status"
};
static_assert(sizeof(kHeaders) / sizeof(kHeaders[0]) == HistoryTableModel::ColumnCount,
              "kHeaders size mismatch");

bool newer(const Data::HistoryRow &a, const Data::HistoryRow &b)
{
    return a.recordedAt == b.recordedAt ? a.readingId > b.readingId : a.recordedAt > b.recordedAt;
}

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
    case TimeRole:      return r.recordedAt.toTimeZone(SettingsController::resolveTimeZone())
                            .toString(QLatin1String(CentralLogger::Format::kDateTimeDdMmYyyyHms));
    case LoggerRole:    return r.loggerName;
    case SensorRole:    return r.sensorName;
    case UnitRole:      return r.unit;
    case ValueRole:     return valueText(r);
    case StatusRole:    return statusText(r);
    case StatusCodeRole:return r.statusCode;
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
        { ValueRole,      CentralLogger::Ui::kRoleValue      },
        { StatusRole,     CentralLogger::Ui::kRoleStatus     },
        { StatusCodeRole, CentralLogger::Ui::kRoleStatusCode },
    };
}

void HistoryTableModel::setRows(const QVector<Data::HistoryRow> &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
    emit rowsSizeChanged();
}

void HistoryTableModel::mergeLatest(const QVector<Data::HistoryRow> &rows)
{
    if (m_rows == rows)
        return;
    emit aboutToMergeRows();
    const auto oldSize = m_rows.size();
    for (int row = 0; row < rows.size() || row < m_rows.size();) {
        if (row < m_rows.size() && (row >= rows.size() || newer(m_rows[row], rows[row]))) {
            int end = row + 1;
            while (end < m_rows.size() && (row >= rows.size() || newer(m_rows[end], rows[row])))
                ++end;
            beginRemoveRows({}, row, end - 1);
            m_rows.remove(row, end - row);
            endRemoveRows();
        } else if (row >= m_rows.size() || newer(rows[row], m_rows[row])) {
            int end = row + 1;
            while (end < rows.size() && (row >= m_rows.size() || newer(rows[end], m_rows[row])))
                ++end;
            beginInsertRows({}, row, end - 1);
            m_rows.insert(row, end - row, Data::HistoryRow{});
            std::copy(rows.cbegin() + row, rows.cbegin() + end, m_rows.begin() + row);
            endInsertRows();
            row = end;
        } else {
            if (m_rows.at(row) != rows.at(row)) {
                m_rows[row] = rows.at(row);
                emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
            }
            ++row;
        }
    }
    if (oldSize != m_rows.size())
        emit rowsSizeChanged();
    emit rowsMerged();
}

qint64 HistoryTableModel::readingIdAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row).readingId : 0;
}

int HistoryTableModel::rowForReadingId(qint64 readingId) const
{
    const auto it = std::find_if(m_rows.cbegin(), m_rows.cend(), [readingId](const auto &row) {
        return row.readingId == readingId;
    });
    return it == m_rows.cend() ? -1 : int(std::distance(m_rows.cbegin(), it));
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
    return CentralLogger::Sensor::formatAnalogValue(r.value, r.decimals);
}

QString HistoryTableModel::statusText(const Data::HistoryRow &r)
{
    return CentralLogger::Sensor::statusText(r.statusCode);
}

} // namespace CentralLogger::Core
