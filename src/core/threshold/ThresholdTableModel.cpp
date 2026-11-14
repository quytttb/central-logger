#include "core/threshold/ThresholdTableModel.h"

#include "utils/UiConstants.h"

#include <algorithm>

namespace CentralLogger::Core {

namespace {

bool sameEdit(const ThresholdRow &row)
{
    return row.minText == row.originalMinText
        && row.maxText == row.originalMaxText;
}

} // namespace

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
    const int src = sourceRow(index.row());
    if (!index.isValid() || src < 0) {
        return {};
    }
    const auto &row = m_rows.at(src);

    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        if (index.column() == 0) {
            return row.name;
        }
        if (index.column() == 1) {
            return row.minText;
        }
        if (index.column() == 2) {
            return row.maxText;
        }
        return {};
    case SensorIdRole:
        return QVariant::fromValue(row.sensorId);
    case NameRole:
        return row.name;
    case UnitRole:
        return row.unit;
    case MinTextRole:
        return row.minText;
    case MaxTextRole:
        return row.maxText;
    case IsNameColumnRole:
        return index.column() == 0;
    case RowDirtyRole:
        return !sameEdit(row);
    default:
        return {};
    }
}

QVariant ThresholdTableModel::headerData(int section, Qt::Orientation orientation,
                                         int role) const
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
        { DisplayRole,       CentralLogger::Ui::kRoleDisplay },
        { SensorIdRole,      CentralLogger::Ui::kRoleSensorId },
        { NameRole,          CentralLogger::Ui::kRoleName },
        { UnitRole,          CentralLogger::Ui::kRoleUnit },
        { MinTextRole,       CentralLogger::Ui::kRoleMinText },
        { MaxTextRole,       CentralLogger::Ui::kRoleMaxText },
        { IsNameColumnRole,  CentralLogger::Ui::kRoleIsNameColumn },
        { RowDirtyRole,      CentralLogger::Ui::kRoleRowDirty },
    };
}

void ThresholdTableModel::setRows(const QVector<ThresholdRow> &rows)
{
    const bool wasDirty = dirty();
    const int previousSize = m_visible.size();

    beginResetModel();
    m_rows = rows;
    rebuildVisible();
    endResetModel();

    if (previousSize != m_visible.size()) {
        emit rowsSizeChanged();
    }
    if (wasDirty || dirty()) {
        emit dirtyChanged();
    }
}

void ThresholdTableModel::setFilterText(const QString &text)
{
    if (m_filterText == text) {
        return;
    }
    const int previousSize = m_visible.size();

    beginResetModel();
    m_filterText = text;
    rebuildVisible();
    endResetModel();

    if (previousSize != m_visible.size()) {
        emit rowsSizeChanged();
    }
}

void ThresholdTableModel::setCellText(int row, int column, const QString &text)
{
    const int src = sourceRow(row);
    if (src < 0 || (column != 1 && column != 2)) {
        return;
    }

    auto &entry = m_rows[src];
    const QString previous =
        column == 1 ? entry.minText : entry.maxText;
    if (previous == text) {
        return;
    }

    const bool wasDirty = dirty();
    if (column == 1) {
        entry.minText = text;
    } else {
        entry.maxText = text;
    }

    const QModelIndex idx = index(row, column);
    emit dataChanged(idx, idx,
                     { Qt::DisplayRole, Qt::EditRole,
                       MinTextRole, MaxTextRole, RowDirtyRole });
    if (wasDirty != dirty()) {
        emit dirtyChanged();
    }
}

void ThresholdTableModel::resetEdits()
{
    const bool wasDirty = dirty();
    beginResetModel();
    for (auto &row : m_rows) {
        row.minText = row.originalMinText;
        row.maxText = row.originalMaxText;
    }
    rebuildVisible();
    endResetModel();
    if (wasDirty) {
        emit dirtyChanged();
    }
}

void ThresholdTableModel::markSaved()
{
    const bool wasDirty = dirty();
    beginResetModel();
    for (auto &row : m_rows) {
        row.originalMinText = row.minText;
        row.originalMaxText = row.maxText;
    }
    rebuildVisible();
    endResetModel();
    if (wasDirty) {
        emit dirtyChanged();
    }
}

bool ThresholdTableModel::dirty() const
{
    return dirtyCount() > 0;
}

int ThresholdTableModel::dirtyCount() const
{
    int count = 0;
    for (const auto &row : m_rows) {
        if (!sameEdit(row)) {
            ++count;
        }
    }
    return count;
}

int ThresholdTableModel::sourceRow(int visibleRow) const
{
    if (visibleRow < 0 || visibleRow >= m_visible.size()) {
        return -1;
    }
    return m_visible.at(visibleRow);
}

void ThresholdTableModel::rebuildVisible()
{
    m_visible.clear();
    m_visible.reserve(m_rows.size());

    const QString needle = m_filterText.trimmed();
    for (int i = 0; i < m_rows.size(); ++i) {
        const auto &row = m_rows.at(i);
        if (needle.isEmpty()
            || row.name.contains(needle, Qt::CaseInsensitive)
            || row.unit.contains(needle, Qt::CaseInsensitive)) {
            m_visible.append(i);
        }
    }
}

} // namespace CentralLogger::Core
