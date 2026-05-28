#include "RecentEventsModel.h"

#include "core/events/EventLevels.h"
#include "data/db/Database.h"

namespace CentralLogger::Core {

RecentEventsModel::RecentEventsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void RecentEventsModel::setDatabase(Data::Database *db)
{
    m_db = db;
}

int RecentEventsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_rows.size());
}

QVariant RecentEventsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }
    const Data::SystemEventListItem &item  = m_rows[index.row()];
    const Data::SystemEvent         &event = item.event;
    switch (role) {
        case IdRole:
            return QVariant::fromValue(event.id);
        case LoggerIdRole:
            return event.loggerId
                   ? QVariant::fromValue(*event.loggerId)
                   : QVariant{};
        case LoggerNameRole:
            return item.loggerName;
        case EventTypeRole:
            return event.eventType;
        case MessageRole:
            return event.message;
        case LevelRole:
            return event.level;
        case DisplayLevelRole:
            return displayLevelForEvent(event.eventType, event.level);
        case CreatedAtRole:
            return event.createdAt;
        default:
            return {};
    }
}

QHash<int, QByteArray> RecentEventsModel::roleNames() const
{
    return {
        { IdRole,          "id" },
        { LoggerIdRole,    "loggerId" },
        { LoggerNameRole,  "loggerName" },
        { EventTypeRole,   "eventType" },
        { MessageRole,     "message" },
        { LevelRole,          "level" },
        { DisplayLevelRole,   "displayLevel" },
        { CreatedAtRole,      "createdAt" },
    };
}

void RecentEventsModel::setLimit(int limit)
{
    if (limit <= 0 || limit == m_limit) return;
    m_limit = limit;
    emit limitChanged();
    reload();
}

void RecentEventsModel::reload()
{
    beginResetModel();
    m_rows.clear();
    if (m_db && m_db->isOpen()) {
        Data::EventRepository repo(m_db->connection());
        m_rows = repo.listRecentWithLoggerName(m_limit);
    }
    endResetModel();
}

} // namespace CentralLogger::Core
