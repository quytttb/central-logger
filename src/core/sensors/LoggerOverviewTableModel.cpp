#include "core/sensors/LoggerOverviewTableModel.h"

#include "utils/UiConstants.h"

namespace CentralLogger::Core {

namespace {

bool sameColumns(const QVector<OverviewColumn> &a, const QVector<OverviewColumn> &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (int i = 0; i < a.size(); ++i) {
        if (a.at(i).edgeSensorId != b.at(i).edgeSensorId ||
            a.at(i).header != b.at(i).header) {
            return false;
        }
    }
    return true;
}

} // namespace

LoggerOverviewTableModel::LoggerOverviewTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int LoggerOverviewTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int LoggerOverviewTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    // Empty model → no columns (AppTableView hides the table). Otherwise the
    // logger name column always exists, even when no sensor columns are known.
    if (m_rows.isEmpty() && m_columns.isEmpty()) {
        return 0;
    }
    return m_columns.size() + 1;
}

QVariant LoggerOverviewTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }
    const auto &row = m_rows.at(index.row());
    const bool loggerColumn = (index.column() == 0);

    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        if (loggerColumn) {
            return row.loggerName;
        }
        if (index.column() - 1 >= m_columns.size()) {
            return {};
        }
        return row.values.value(m_columns.at(index.column() - 1).edgeSensorId);
    case LoggerIdRole:
        return row.loggerId;
    case LoggerNameRole:
        return row.loggerName;
    case StatusRole:
        return row.status;
    case OnlineRole:
        return row.online;
    case IsLoggerColumnRole:
        return loggerColumn;
    case ValueRole:
        return data(index, Qt::DisplayRole);
    default:
        return {};
    }
}

QVariant LoggerOverviewTableModel::headerData(int section,
                                              Qt::Orientation orientation,
                                              int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }
    if (section == 0) {
        return tr("Logger");
    }
    const int col = section - 1;
    if (col < 0 || col >= m_columns.size()) {
        return {};
    }
    return m_columns.at(col).header;
}

QHash<int, QByteArray> LoggerOverviewTableModel::roleNames() const
{
    return {
        { DisplayRole,      CentralLogger::Ui::kRoleDisplay },
        { LoggerIdRole,     CentralLogger::Ui::kRoleLoggerId },
        { LoggerNameRole,   CentralLogger::Ui::kRoleLoggerName },
        { StatusRole,       CentralLogger::Ui::kRoleStatus },
        { OnlineRole,       CentralLogger::Ui::kRoleOnline },
        { IsLoggerColumnRole, CentralLogger::Ui::kRoleIsLoggerColumn },
        { ValueRole,        CentralLogger::Ui::kRoleValue },
    };
}

void LoggerOverviewTableModel::setData(const QVector<OverviewRow> &rows,
                                       const QVector<OverviewColumn> &columns)
{
    const bool layoutChanged = !sameColumns(m_columns, columns)
                               || m_rows.size() != rows.size();

    if (layoutChanged) {
        const bool columnsActuallyChanged = !sameColumns(m_columns, columns)
                                            || m_columns.size() != columns.size();
        const bool rowsActuallyChanged = m_rows.size() != rows.size();
        beginResetModel();
        m_rows = rows;
        m_columns = columns;
        endResetModel();
        if (rowsActuallyChanged) {
            emit rowsSizeChanged();
        }
        if (columnsActuallyChanged) {
            emit columnsChanged();
        }
        return;
    }

    m_rows = rows;
    if (!m_rows.isEmpty() && columnCount() > 0) {
        emit dataChanged(index(0, 0),
                         index(m_rows.size() - 1, columnCount() - 1),
                         { Qt::DisplayRole, LoggerIdRole, LoggerNameRole,
                           StatusRole, OnlineRole, IsLoggerColumnRole,
                           ValueRole });
    }
}

void LoggerOverviewTableModel::clear()
{
    if (m_rows.isEmpty() && m_columns.isEmpty()) {
        return;
    }
    const bool hadRows = !m_rows.isEmpty();
    const bool hadColumns = !m_columns.isEmpty();
    beginResetModel();
    m_rows.clear();
    m_columns.clear();
    endResetModel();
    if (hadRows) {
        emit rowsSizeChanged();
    }
    if (hadColumns) {
        emit columnsChanged();
    }
}

} // namespace CentralLogger::Core
