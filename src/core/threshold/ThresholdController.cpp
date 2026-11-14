#include "core/threshold/ThresholdController.h"

#include "DashboardController.h"
#include "data/db/Database.h"
#include "data/models/LoggerInfo.h"
#include "data/models/LoggerSensor.h"
#include "data/repositories/LoggerRepository.h"
#include "data/repositories/SensorCatalogRepository.h"
#include "utils/FormatConstants.h"
#include "utils/SensorConstants.h"
#include "utils/UiConstants.h"

#include <QDebug>
#include <QJSEngine>
#include <QQmlEngine>
#include <QSqlDatabase>
#include <QSqlError>
#include <QVariantMap>

#include <cmath>
#include <optional>

namespace CentralLogger::Core {

namespace {

ThresholdController *g_instance = nullptr;

QString formatThreshold(const std::optional<double> &value)
{
    if (!value.has_value()) {
        return {};
    }
    QString text = QString::number(*value, 'f', 6);
    if (text.contains(QLatin1Char('.'))) {
        while (text.endsWith(QLatin1Char('0'))) {
            text.chop(1);
        }
        if (text.endsWith(QLatin1Char('.'))) {
            text.chop(1);
        }
    }
    if (text == QLatin1String("-0")) {
        text = QStringLiteral("0");
    }
    return text;
}

bool parseThreshold(const QString &text, std::optional<double> *out,
                    QString *errorOut)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        *out = std::nullopt;
        return true;
    }
    QString normalized = trimmed;
    normalized.replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    const double value = normalized.toDouble(&ok);
    if (!ok || !std::isfinite(value)) {
        if (errorOut) {
            *errorOut = QStringLiteral("\"%1\" is not a valid number.").arg(trimmed);
        }
        return false;
    }
    *out = value;
    return true;
}

} // namespace

ThresholdController::ThresholdController(QObject *parent) : QObject(parent) {}

ThresholdController *ThresholdController::instance() { return g_instance; }

void ThresholdController::setInstance(ThresholdController *controller) {
  g_instance = controller;
}

ThresholdController *ThresholdController::create(QQmlEngine *, QJSEngine *) {
  Q_ASSERT(g_instance);
  QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
  return g_instance;
}

void ThresholdController::setError(const QString &message) {
  if (m_lastError == message) {
    return;
  }
  m_lastError = message;
  emit lastErrorChanged();
}

void ThresholdController::refresh() {
  m_loggerItems.clear();

  qint64 firstId = 0;
  if (m_db && m_db->isOpen()) {
    Data::LoggerRepository repo(m_db->connection());
    const auto rows = repo.findAll();
    for (const auto &info : rows) {
      QVariantMap item;
      item.insert(QLatin1String(CentralLogger::Ui::kRoleId),
                  QVariant::fromValue(info.id));
      item.insert(QLatin1String(CentralLogger::Ui::kRoleName), info.name);
      item.insert(QLatin1String(CentralLogger::Ui::kRoleStationCode),
                  info.stationCode);
      m_loggerItems.append(item);
      if (firstId == 0) {
        firstId = info.id;
      }
    }
  }
  emit loggerItemsChanged();

  bool stillExists = false;
  for (const auto &item : m_loggerItems) {
    if (item.toMap().value(QLatin1String(CentralLogger::Ui::kRoleId))
            .toLongLong() == m_selectedLoggerId) {
      stillExists = true;
      break;
    }
  }
  if (!stillExists) {
    m_selectedLoggerId = firstId;
  }
  updateSelectedLoggerName();
  emit selectedLoggerChanged();

  loadRows();
}

void ThresholdController::setSelectedLoggerId(qint64 id) {
  if (m_selectedLoggerId == id) {
    return;
  }
  m_selectedLoggerId = id;
  updateSelectedLoggerName();
  emit selectedLoggerChanged();
  loadRows();
}

void ThresholdController::updateSelectedLoggerName() {
  m_selectedLoggerName.clear();
  for (const auto &item : m_loggerItems) {
    const auto map = item.toMap();
    if (map.value(QLatin1String(CentralLogger::Ui::kRoleId)).toLongLong()
        == m_selectedLoggerId) {
      m_selectedLoggerName =
          map.value(QLatin1String(CentralLogger::Ui::kRoleName)).toString();
      break;
    }
  }
}

void ThresholdController::setFilterText(const QString &text) {
  m_table.setFilterText(text);
}

void ThresholdController::loadRows() {
  QVector<ThresholdRow> rows;

  if (m_db && m_db->isOpen() && m_selectedLoggerId > 0) {
    Data::SensorCatalogRepository repo(m_db->connection());
    QString error;
    const auto sensors = repo.listByLoggerId(m_selectedLoggerId, &error);
    if (!error.isEmpty()) {
      qWarning() << "ThresholdController: catalog read failed:" << error;
      setError(error);
    } else {
      for (const auto &sensor : sensors) {
        if (sensor.sensorType
                != QLatin1String(CentralLogger::Sensor::kTypeAnalog)
            || !sensor.active) {
          continue;
        }
        ThresholdRow row;
        row.sensorId = sensor.id;
        row.edgeSensorId = sensor.edgeSensorId;
        row.name = sensor.name.isEmpty()
                       ? QString(QLatin1String(CentralLogger::Sensor::kFallbackNameFmt))
                             .arg(sensor.edgeSensorId)
                       : sensor.name;
        row.unit = sensor.unit;
        row.minText = formatThreshold(sensor.minThreshold);
        row.maxText = formatThreshold(sensor.maxThreshold);
        row.originalMinText = row.minText;
        row.originalMaxText = row.maxText;
        rows.append(row);
      }
      setError({});
    }
  }

  m_table.setRows(rows);
}

void ThresholdController::discardChanges() {
  loadRows();
}

void ThresholdController::save() {
  if (m_saving) {
    return;
  }
  if (!m_db || !m_db->isOpen()) {
    setError(QLatin1String(CentralLogger::Format::kErrDatabaseNotOpen));
    emit saveFinished(false, m_lastError);
    return;
  }
  if (m_selectedLoggerId <= 0) {
    setError(QStringLiteral("Select a logger first."));
    emit saveFinished(false, m_lastError);
    return;
  }

  struct Change {
    ThresholdRow          row;
    std::optional<double> min;
    std::optional<double> max;
  };
  QVector<Change> changes;

  const auto rows = m_table.rows();
  for (const auto &row : rows) {
    if (row.minText == row.originalMinText
        && row.maxText == row.originalMaxText) {
      continue;
    }
    Change change;
    change.row = row;
    QString parseError;
    if (!parseThreshold(row.minText, &change.min, &parseError)
        || !parseThreshold(row.maxText, &change.max, &parseError)) {
      setError(QStringLiteral("%1 (%2)").arg(row.name, parseError));
      emit saveFinished(false, m_lastError);
      return;
    }
    if (change.min.has_value() && change.max.has_value()
        && *change.min >= *change.max) {
      setError(QStringLiteral("%1: Min threshold must be lower than Max.")
                   .arg(row.name));
      emit saveFinished(false, m_lastError);
      return;
    }
    changes.append(change);
  }

  if (changes.isEmpty()) {
    emit saveFinished(true, QString{});
    return;
  }

  m_saving = true;
  emit savingChanged();
  setError({});

  QSqlDatabase conn = m_db->connection();
  if (!conn.transaction()) {
    m_saving = false;
    emit savingChanged();
    setError(QStringLiteral("Could not start database transaction."));
    emit saveFinished(false, m_lastError);
    return;
  }

  Data::SensorCatalogRepository repo(conn);
  for (const auto &change : changes) {
    QString error;
    auto sensor = repo.findByLoggerAndEdgeId(
        m_selectedLoggerId, change.row.edgeSensorId,
        QLatin1String(CentralLogger::Sensor::kTypeAnalog), &error);
    if (!sensor.has_value()) {
      conn.rollback();
      m_saving = false;
      emit savingChanged();
      setError(error.isEmpty()
                   ? QStringLiteral("Sensor not found: %1").arg(change.row.name)
                   : error);
      emit saveFinished(false, m_lastError);
      return;
    }
    sensor->minThreshold = change.min;
    sensor->maxThreshold = change.max;
    if (!repo.upsert(*sensor, &error)) {
      conn.rollback();
      m_saving = false;
      emit savingChanged();
      setError(error);
      emit saveFinished(false, m_lastError);
      return;
    }
  }

  if (!conn.commit()) {
    conn.rollback();
    m_saving = false;
    emit savingChanged();
    setError(QStringLiteral("Could not commit database transaction."));
    emit saveFinished(false, m_lastError);
    return;
  }

  m_table.markSaved();
  m_saving = false;
  emit savingChanged();

  if (m_dashboard) {
    // Re-read the catalog so the summary charts pick up the new thresholds
    // immediately, and drop the bridge cache so the next poll re-evaluates
    // ALARM state with the new limits.
    m_dashboard->refreshLoggerCatalog(m_selectedLoggerId);
    m_dashboard->logEvent(
        m_selectedLoggerId, CentralLogger::Sensor::kEventTypeInfo,
        QStringLiteral("Thresholds updated: %1").arg(m_selectedLoggerName));
  }

  emit saveFinished(true, QString{});
}

} // namespace CentralLogger::Core
