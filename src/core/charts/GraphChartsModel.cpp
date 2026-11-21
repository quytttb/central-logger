#include "GraphChartsModel.h"

#include "utils/UiConstants.h"

namespace CentralLogger::Core {

int GraphChartsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_charts.size());
}

QVariant GraphChartsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_charts.size() || role != ChartRole)
        return {};
    return m_charts.at(index.row());
}

QHash<int, QByteArray> GraphChartsModel::roleNames() const
{
    return {{ChartRole, Ui::kRoleChart}};
}

void GraphChartsModel::setCharts(qint64 loggerId, const QVariantList &charts)
{
    if (loggerId != m_loggerId) {
        beginResetModel();
        m_loggerId = loggerId;
        m_charts = charts;
        endResetModel();
        return;
    }

    const auto id = [](const QVariant &chart) {
        return chart.toMap().value(QLatin1String(Ui::kChartEdgeSensorId)).toInt();
    };
    for (int row = 0; row < charts.size() || row < m_charts.size();) {
        if (row < m_charts.size()
            && (row == charts.size() || id(m_charts.at(row)) < id(charts.at(row)))) {
            beginRemoveRows({}, row, row);
            m_charts.removeAt(row);
            endRemoveRows();
        } else if (row == m_charts.size() || id(m_charts.at(row)) > id(charts.at(row))) {
            beginInsertRows({}, row, row);
            m_charts.insert(row, charts.at(row));
            endInsertRows();
            ++row;
        } else {
            if (m_charts.at(row) != charts.at(row)) {
                m_charts[row] = charts.at(row);
                emit dataChanged(index(row), index(row), {ChartRole});
            }
            ++row;
        }
    }
}

} // namespace CentralLogger::Core
