#pragma once

#include "core/threshold/ThresholdTableModel.h"

#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

class QJSEngine;
class QQmlEngine;

namespace CentralLogger::Data {
class Database;
} // namespace CentralLogger::Data

namespace CentralLogger::Core {

/// Displays the selected logger's mirrored thresholds from SQLite. The
/// Settings form owns the GET that refreshes the mirror; this controller
/// does not contact the logger or modify the catalog.
class ThresholdController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(CentralLogger::Core::ThresholdTableModel *table READ table CONSTANT)
    Q_PROPERTY(qint64 selectedLoggerId READ selectedLoggerId
                   WRITE setSelectedLoggerId NOTIFY selectedLoggerChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit ThresholdController(QObject *parent);

    ThresholdTableModel *table() { return &m_table; }
    qint64 selectedLoggerId() const { return m_selectedLoggerId; }
    QString lastError() const { return m_lastError; }

    void setDatabase(Data::Database *db) { m_db = db; }
    void setSelectedLoggerId(qint64 id);
    void refresh();

    Q_INVOKABLE void setFilterText(const QString &text);

    static void setInstance(ThresholdController *controller);
    static ThresholdController *create(QQmlEngine *, QJSEngine *);

signals:
    void selectedLoggerChanged();
    void lastErrorChanged();

private:
    Data::Database      *m_db = nullptr;
    ThresholdTableModel  m_table;
    qint64               m_selectedLoggerId = 0;
    QString              m_lastError;
};

} // namespace CentralLogger::Core
