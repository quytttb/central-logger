#include "core/threshold/ThresholdController.h"

#include "data/db/Database.h"
#include "data/repositories/SensorCatalogRepository.h"
#include "utils/AppConstants.h"
#include "utils/SensorConstants.h"

#include <QDebug>
#include <QQmlEngine>

namespace CentralLogger::Core {

namespace {

ThresholdController *g_instance = nullptr;

QString formatThreshold(const std::optional<double> &value)
{
    if (!value) {
        return {};
    }
    QString text = QString::number(*value, 'f', Defaults::kDecimalsMax);
    if (text.contains(QLatin1Char('.'))) {
        while (text.endsWith(QLatin1Char('0'))) {
            text.chop(1);
        }
        if (text.endsWith(QLatin1Char('.'))) {
            text.chop(1);
        }
    }
    return text == QLatin1String("-0") ? QStringLiteral("0") : text;
}

} // namespace

ThresholdController::ThresholdController(QObject *parent) : QObject(parent) {}

void ThresholdController::setInstance(ThresholdController *controller)
{
    g_instance = controller;
}

ThresholdController *ThresholdController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(g_instance);
    QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
    return g_instance;
}

void ThresholdController::setSelectedLoggerId(qint64 id)
{
    if (m_selectedLoggerId == id) {
        return;
    }
    m_selectedLoggerId = id;
    refresh();
    emit selectedLoggerChanged();
}

void ThresholdController::setFilterText(const QString &text)
{
    m_table.setFilterText(text);
}

void ThresholdController::refresh()
{
    QVector<ThresholdRow> rows;
    QString error;
    if (m_db && m_db->isOpen() && m_selectedLoggerId > 0) {
        Data::SensorCatalogRepository repo(m_db->connection());
        const auto sensors = repo.listByLoggerId(m_selectedLoggerId, &error);
        if (!error.isEmpty()) {
            qWarning() << "ThresholdController: catalog read failed:" << error;
        } else {
            for (const auto &sensor : sensors) {
                if (sensor.sensorType != QLatin1String(Sensor::kTypeAnalog) || !sensor.active) {
                    continue;
                }
                const QString name = sensor.name.isEmpty()
                    ? QString(QLatin1String(Sensor::kFallbackNameFmt)).arg(sensor.edgeSensorId)
                    : sensor.name;
                rows.append({name, sensor.unit, formatThreshold(sensor.minThreshold),
                             formatThreshold(sensor.maxThreshold)});
            }
        }
    }
    m_table.setRows(rows);
    if (m_lastError != error) {
        m_lastError = error;
        emit lastErrorChanged();
    }
}

} // namespace CentralLogger::Core
