#include "core/threshold/ThresholdTableModel.h"

#include "utils/UiConstants.h"

namespace CentralLogger::Core {

ThresholdTableModel::ThresholdTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int ThresholdTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_visible.size();
}

int ThresholdTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 3;
}

QVariant ThresholdTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_visible.size()) {
        return {};
    }
    const auto &row = m_rows.at(m_visible.at(index.row()));
    if (role == UnitRole) {
        return row.unit;
    }
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return row.name;
        case 1: return row.minText;
        case 2: return row.maxText;
        default: break;
        }
    }
    return {};
}

QVariant ThresholdTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }
    switch (section) {
    case 0: return tr("Sensor");
    case 1: return tr("Min");
    case 2: return tr("Max");
    default: return {};
    }
}

QHash<int, QByteArray> ThresholdTableModel::roleNames() const
{
    return {
        { Qt::DisplayRole, Ui::kRoleDisplay },
        { UnitRole,        Ui::kRoleUnit },
    };
}

void ThresholdTableModel::setRows(const QVector<ThresholdRow> &rows)
{
    const int previousSize = m_visible.size();
    beginResetModel();
    m_rows = rows;
    rebuildVisible();
    endResetModel();
    if (previousSize != m_visible.size()) {
        emit rowsSizeChanged();
    }
}

void ThresholdTableModel::setFilterText(const QString &text)
{
    const QString needle = text.trimmed();
    if (m_filterText == needle) {
        return;
    }
    const int previousSize = m_visible.size();
    beginResetModel();
    m_filterText = needle;
    rebuildVisible();
    endResetModel();
    if (previousSize != m_visible.size()) {
        emit rowsSizeChanged();
    }
}

void ThresholdTableModel::rebuildVisible()
{
    m_visible.clear();
    m_visible.reserve(m_rows.size());
    for (int i = 0; i < m_rows.size(); ++i) {
        const auto &row = m_rows.at(i);
        if (m_filterText.isEmpty()
            || row.name.contains(m_filterText, Qt::CaseInsensitive)
            || row.unit.contains(m_filterText, Qt::CaseInsensitive)) {
            m_visible.append(i);
        }
    }
}

} // namespace CentralLogger::Core
