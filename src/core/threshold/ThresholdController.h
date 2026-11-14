#pragma once

#include "core/threshold/ThresholdTableModel.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQmlIntegration/qqmlintegration.h>

class QJSEngine;
class QQmlEngine;

namespace CentralLogger::Data {
class Database;
} // namespace CentralLogger::Data

namespace CentralLogger::Core {

class DashboardController;

/// QML facade for the Thresholds screen (Set Threshold → full page).
///
/// Owns the logger picker list + the editable ThresholdTableModel and
/// persists edits to `logger_sensor.min_threshold/max_threshold`. Thresholds
/// are applied centrally: SensorMerger raises ALARM when a live value is
/// <= min or >= max, and the Dashboard summary tints each sensor chart.
///
/// Registered as a QML singleton (`CentralLogger.Core.ThresholdController`).
class ThresholdController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(CentralLogger::Core::ThresholdTableModel *table READ table CONSTANT)
    Q_PROPERTY(QVariantList loggerItems READ loggerItems NOTIFY loggerItemsChanged)
    Q_PROPERTY(qint64 selectedLoggerId READ selectedLoggerId
                   WRITE setSelectedLoggerId NOTIFY selectedLoggerChanged)
    Q_PROPERTY(QString selectedLoggerName READ selectedLoggerName
                   NOTIFY selectedLoggerChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool saving READ saving NOTIFY savingChanged)

public:
    /// Parent required so QML cannot default-construct a second singleton.
    explicit ThresholdController(QObject *parent);

    ThresholdTableModel *table() { return &m_table; }
    QVariantList loggerItems() const { return m_loggerItems; }
    qint64       selectedLoggerId() const { return m_selectedLoggerId; }
    QString      selectedLoggerName() const { return m_selectedLoggerName; }
    QString      lastError() const { return m_lastError; }
    bool         saving() const { return m_saving; }

    void setDatabase(Data::Database *db) { m_db = db; }
    void setDashboardController(DashboardController *dashboard) { m_dashboard = dashboard; }

    static ThresholdController *instance();
    static void setInstance(ThresholdController *controller);
    static ThresholdController *create(QQmlEngine *, QJSEngine *);

public slots:
    /// Reloads the logger picker + rows for the selected logger.
    void refresh();

    void setSelectedLoggerId(qint64 id);
    void setFilterText(const QString &text);

    /// Restores every edited cell to the DB value.
    void discardChanges();

    /// Validates + writes changed rows in one transaction.
    void save();

signals:
    void loggerItemsChanged();
    void selectedLoggerChanged();
    void lastErrorChanged();
    void savingChanged();
    void saveFinished(bool ok, QString message);

private:
    void setError(const QString &message);
    void loadRows();
    void updateSelectedLoggerName();

    Data::Database       *m_db = nullptr;
    DashboardController  *m_dashboard = nullptr;
    ThresholdTableModel   m_table;
    QVariantList          m_loggerItems;
    qint64                m_selectedLoggerId = 0;
    QString               m_selectedLoggerName;
    QString               m_lastError;
    bool                  m_saving = false;
};

} // namespace CentralLogger::Core
