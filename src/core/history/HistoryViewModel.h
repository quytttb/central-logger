#pragma once

#include "core/history/HistoryTableModel.h"
#include "data/models/HistoryRow.h"

#include <QDateTime>
#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVector>
#include <QtQmlIntegration/qqmlintegration.h>

namespace CentralLogger::Data {
class Database;
} // namespace CentralLogger::Data

namespace CentralLogger::Network {
class HistoryWriterWorker;
} // namespace CentralLogger::Network

namespace CentralLogger::Core {

/// Result of a background history SQL query (QThreadPool / QtConcurrent).
struct HistorySearchResult
{
    QVector<Data::HistoryRow> rows;
    int                       totalCount = 0;
    QString                   error;
};

/// ViewModel for the global History page (sidebar navigation).
class HistoryViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(qint64  loggerId          READ loggerId          WRITE setLoggerId          NOTIFY loggerIdChanged)
    Q_PROPERTY(bool    loading           READ loading                                       NOTIFY loadingChanged)
    Q_PROPERTY(QString lastError         READ lastError                                     NOTIFY lastErrorChanged)
    Q_PROPERTY(int     recordCount       READ recordCount                                   NOTIFY recordCountChanged)
    Q_PROPERTY(int     displayedCount    READ displayedCount                                NOTIFY displayedCountChanged)
    Q_PROPERTY(QVariantList loggerItems  READ loggerItems                                  NOTIFY loggerItemsChanged)
    Q_PROPERTY(QVariantList sensorItems   READ sensorItems                                   NOTIFY sensorItemsChanged)
    Q_PROPERTY(bool    showLoggerColumn  READ showLoggerColumn                              NOTIFY showLoggerColumnChanged)
    Q_PROPERTY(bool    searchedOnce       READ searchedOnce                                  NOTIFY searchedOnceChanged)
    Q_PROPERTY(CentralLogger::Core::HistoryTableModel *tableModel
               READ tableModel CONSTANT)

public:
    explicit HistoryViewModel(QObject *parent = nullptr);
    ~HistoryViewModel() override;

    qint64  loggerId()         const { return m_loggerId; }
    bool    loading()          const { return m_loading; }
    QString lastError()        const { return m_lastError; }
    int     recordCount()      const { return m_recordCount; }
    int     displayedCount()   const { return m_displayedCount; }
    QVariantList loggerItems() const { return m_loggerItems; }
    QVariantList sensorItems() const { return m_sensorItems; }
    bool    showLoggerColumn() const { return m_searchLoggerId == 0; }
    bool    searchedOnce()     const { return m_searchedOnce; }

    HistoryTableModel *tableModel() { return &m_model; }

    static void registerDatabase(Data::Database *db);
    static void registerHistoryWriter(Network::HistoryWriterWorker *worker);

public slots:
    void setLoggerId(qint64 id);

    /// Reload logger and sensor dropdowns from DB (call on view open).
    Q_INVOKABLE void reloadFilters();

    Q_INVOKABLE void search(const QString &fromDate, const QString &toDate,
                            qint64 sensorId = 0);

    /// Flushes pending history writes, then re-runs @p search with the same filters.
    Q_INVOKABLE void refresh(const QString &fromDate, const QString &toDate,
                             qint64 sensorId = 0);

    Q_INVOKABLE void exportCsv(const QUrl &fileUrl);

signals:
    void loggerIdChanged();
    void loadingChanged();
    void lastErrorChanged();
    void recordCountChanged();
    void displayedCountChanged();
    void loggerItemsChanged();
    void sensorItemsChanged();
    void showLoggerColumnChanged();
    void searchedOnceChanged();

    void exportFinished(bool ok, QString message);

private slots:
    void onSearchCompleted();

private:
    void loadLoggerItems();
    void loadSensorItems();
    void abortWorker();
    void setLoading(bool v);
    void setError(const QString &msg);
    bool sameQueryAsLast(qint64 loggerId, qint64 sensorId,
                         const QDateTime &fromUtc, const QDateTime &toUtc) const;
    void rememberQuery(qint64 loggerId, qint64 sensorId,
                       const QDateTime &fromUtc, const QDateTime &toUtc);

    HistoryTableModel m_model;

    qint64       m_loggerId        = 0; // filter dropdown only
    qint64       m_searchLoggerId  = 0; // last Search — drives table Logger column
    bool         m_searchedOnce    = false;
    bool         m_loading     = false;
    QString      m_lastError;
    int          m_recordCount = 0;
    int          m_displayedCount = 0;
    QVariantList m_loggerItems;
    QVariantList m_sensorItems;

    QFutureWatcher<HistorySearchResult> *m_searchWatcher = nullptr;
    quint64                              m_searchGeneration = 0;
    quint64                              m_activeSearchGeneration = 0;

    bool      m_hasLastQuery   = false;
    qint64    m_lastQueryLoggerId = 0;
    qint64    m_lastQuerySensorId = 0;
    QDateTime m_lastQueryFromUtc;
    QDateTime m_lastQueryToUtc;
};

} // namespace CentralLogger::Core
