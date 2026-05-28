#pragma once

#include "data/repositories/EventRepository.h"

#include <QAbstractListModel>
#include <QVector>
#include <QtQmlIntegration/qqmlintegration.h>

namespace CentralLogger::Data {
class Database;
} // namespace CentralLogger::Data

namespace CentralLogger::Core {

/// Read-only list model over the most recent rows in `system_event`,
/// joined with `logger_info.name`. Owned by `DashboardController` and
/// exposed to QML as the `recentEvents` property. Reload is explicit —
/// the controller calls `reload()` after CRUD and after Modbus-driven
/// online/offline transitions (Task 19).
class RecentEventsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(int limit READ limit WRITE setLimit NOTIFY limitChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        LoggerIdRole,
        LoggerNameRole,
        EventTypeRole,
        MessageRole,
        LevelRole,
        DisplayLevelRole,
        CreatedAtRole,
    };

    explicit RecentEventsModel(QObject *parent = nullptr);

    void setDatabase(Data::Database *db);

    int      rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int  limit() const { return m_limit; }
    void setLimit(int limit);

    Q_INVOKABLE void reload();

signals:
    void limitChanged();

private:
    Data::Database                       *m_db = nullptr;
    int                                   m_limit = 20;
    QVector<Data::SystemEventListItem>    m_rows;
};

} // namespace CentralLogger::Core
