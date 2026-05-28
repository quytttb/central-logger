#pragma once

#include "data/models/AppSettings.h"

#include <QSqlDatabase>
#include <QString>

namespace CentralLogger::Data {

/// Singleton row repository for `app_settings` (id = 1).
class SettingsRepository
{
public:
    explicit SettingsRepository(QSqlDatabase db) : m_db(std::move(db)) {}

    /// Returns the seeded defaults from `001_initial.sql` for a fresh DB,
    /// or the stored values. Returns an empty AppSettings and sets
    /// @p errorOut on SQL failure.
    AppSettings get(QString *errorOut = nullptr) const;

    bool update(const AppSettings &settings, QString *errorOut = nullptr);

private:
    QSqlDatabase m_db;
};

} // namespace CentralLogger::Data
