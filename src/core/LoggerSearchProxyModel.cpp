#include "LoggerSearchProxyModel.h"
#include "LoggerListModel.h"

namespace CentralLogger::Core {

LoggerSearchProxyModel::LoggerSearchProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

void LoggerSearchProxyModel::setFilterText(const QString &text)
{
    if (m_filterText == text) return;
    m_filterText = text;
    emit filterTextChanged();
    beginFilterChange();
    endFilterChange();
}

bool LoggerSearchProxyModel::filterAcceptsRow(int sourceRow,
                                              const QModelIndex &sourceParent) const
{
    if (m_filterText.isEmpty()) return true;

    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    const QString station = idx.data(LoggerListModel::StationCodeRole).toString();
    const QString name    = idx.data(LoggerListModel::NameRole).toString();
    const QString host    = idx.data(LoggerListModel::HostRole).toString();

    return station.contains(m_filterText, Qt::CaseInsensitive)
        || name.contains(m_filterText, Qt::CaseInsensitive)
        || host.contains(m_filterText, Qt::CaseInsensitive);
}

} // namespace CentralLogger::Core
