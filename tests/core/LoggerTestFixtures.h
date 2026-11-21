#pragma once

#include "core/DashboardController.h"
#include "core/sensors/LoggerLiveCardsModel.h"
#include "data/db/Database.h"
#include "data/repositories/LoggerRepository.h"

namespace LoggerTestFixtures {

// Seed an existing logger without pretending to exercise the asynchronous
// Connect & Save form flow; that flow is covered by test_logger_form_save.
inline qint64 seedLogger(CentralLogger::Data::Database &db,
                         CentralLogger::Core::DashboardController &dashboard,
                         const QString &code, const QString &name,
                         const QString &host, int modbusPort, int apiPort,
                         const QString &token, const QString &note = {})
{
    CentralLogger::Data::LoggerInfo logger;
    logger.stationCode = code;
    logger.name = name;
    logger.host = host;
    logger.modbusPort = modbusPort;
    logger.apiPort = apiPort;
    logger.apiToken = token;
    logger.note = note;
    CentralLogger::Data::LoggerRepository repo(db.connection());
    if (!repo.insert(logger))
        return -1;
    dashboard.reloadLoggers();
    return logger.id;
}

inline QVariantMap cardSection(CentralLogger::Core::DashboardController &dashboard,
                               int row)
{
    using Model = CentralLogger::Core::LoggerLiveCardsModel;
    auto *model = dashboard.liveCards();
    const QModelIndex index = model->index(row, 0);
    return {
        {QStringLiteral("loggerId"), model->data(index, Model::LoggerIdRole)},
        {QStringLiteral("name"), model->data(index, Model::NameRole)},
        {QStringLiteral("enabled"), model->data(index, Model::EnabledRole)},
        {QStringLiteral("online"), model->data(index, Model::OnlineRole)},
        {QStringLiteral("polling"), model->data(index, Model::PollingRole)},
        {QStringLiteral("anyAlarm"), model->data(index, Model::AnyAlarmRole)},
        {QStringLiteral("sensorCount"), model->data(index, Model::SensorCountRole)},
        {QStringLiteral("lastUpdate"), model->data(index, Model::LastUpdateRole)},
        {QStringLiteral("sensors"), model->data(index, Model::SensorsRole)},
    };
}

} // namespace LoggerTestFixtures
