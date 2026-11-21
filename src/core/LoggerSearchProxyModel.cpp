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
    const QString needle = m_filterText.trimmed();
    if (needle.isEmpty()) return true;

    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    const QString code = idx.data(LoggerListModel::StationCodeRole).toString();
    const QString name = idx.data(LoggerListModel::NameRole).toString();
    const QString host = idx.data(LoggerListModel::HostRole).toString();

    return code.contains(needle, Qt::CaseInsensitive)
        || name.contains(needle, Qt::CaseInsensitive)
        || host.contains(needle, Qt::CaseInsensitive);
}

} // namespace CentralLogger::Core
