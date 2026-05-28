#pragma once

#include "data/models/SystemEvent.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>

namespace CentralLogger::Data {

/// `SystemEvent` augmented with the joined `logger_info.name` so list models
/// can render the station label without an N+1 lookup. `loggerName` is
/// empty for app-wide events (`logger_id IS NULL`) and rows whose logger
/// was already deleted (the FK uses ON DELETE SET NULL).
struct SystemEventListItem
{
    SystemEvent event;
    QString     loggerName;
};

class EventRepository
{
public:
    explicit EventRepository(QSqlDatabase db) : m_db(std::move(db)) {}

    bool insert(SystemEvent &event, QString *errorOut = nullptr);

    QVector<SystemEvent> listRecent(int limit = 20, QString *errorOut = nullptr) const;

    /// LEFT JOIN against `logger_info` so callers (RecentEventsModel) can
    /// render a station name without re-querying per row. Same ordering
    /// as `listRecent`.
    QVector<SystemEventListItem> listRecentWithLoggerName(
        int limit = 20, QString *errorOut = nullptr) const;

private:
    QSqlDatabase m_db;
};

} // namespace CentralLogger::Data
