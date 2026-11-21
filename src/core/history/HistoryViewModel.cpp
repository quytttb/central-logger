#include "HistoryViewModel.h"

#include "core/SettingsController.h"
#include "core/history/AverageExportService.h"
#include "data/db/Database.h"
#include "data/repositories/LoggerRepository.h"
#include "data/repositories/SensorReadingRepository.h"
#include "network/workers/HistoryWriterWorker.h"
#include "utils/AppConstants.h"
#include "utils/CsvUtils.h"
#include "utils/DbConstants.h"
#include "utils/FormatConstants.h"
#include "utils/UiConstants.h"

#include <QDateTime>
#include <QTimeZone>
#include <QTimer>
#include <QtConcurrent>
#include <QVariantMap>

#include <memory>

namespace CentralLogger::Core {

namespace {

using CentralLogger::Defaults::kHistorySearchLimit;

QVariantMap filterItem(qint64 id, const QString &name)
{
    QVariantMap item;
    item[QStringLiteral("id")]   = id;
    item[QStringLiteral("name")] = name;
    return item;
}

struct HistorySearchParams
{
    QString   dbPath;
    qint64    loggerId   = 0;
    QDateTime fromUtc;
    QDateTime toUtc;
    qint64    sensorId   = 0;
    int       limit      = kHistorySearchLimit;
};

HistorySearchResult executeHistorySearch(HistorySearchParams params)
{
    HistorySearchResult result;
    Data::Database::withScopedConnection(
        QStringLiteral("history_search"),
        params.dbPath,
        [&](QSqlDatabase &db) {
            Data::SensorReadingRepository repo(db);
            result.totalCount = repo.countHistory(params.loggerId,
                                                  params.fromUtc,
                                                  params.toUtc,
                                                  params.sensorId,
                                                  &result.error);
            if (!result.error.isEmpty()) {
                return;
            }
            result.rows = repo.searchHistory(params.loggerId,
                                             params.fromUtc,
                                             params.toUtc,
                                             params.sensorId,
                                             params.limit,
                                             &result.error);
        },
        &result.error);
    return result;
}

/// Worker: build the average CSV on a throw-away connection, then write it.
AverageExportJobResult executeAverageExport(const QString &dbPath,
                                            AverageExportParams params,
                                            const QString &savePath)
{
    AverageExportJobResult result;
    Core::AverageExportResult built;
    QString openErr;
    if (!Data::Database::withScopedConnection(
            QStringLiteral("avg_export"),
            dbPath,
            [&](QSqlDatabase &db) {
                Core::AverageExportService service(db);
                built = service.build(params);
            },
            &openErr)) {
        result.error = openErr;
        return result;
    }

    if (!built.error.isEmpty()) {
        result.error = built.error;
        return result;
    }

    if (!CentralLogger::Utils::writeUtf8BomFile(savePath, built.csv, &result.error)) {
        return result;
    }

    result.ok   = true;
    result.path = savePath;
    return result;
}

} // namespace

namespace {
Data::Database *g_db = nullptr;
Network::HistoryWriterWorker *g_historyWriter = nullptr;
} // namespace

void HistoryViewModel::registerDatabase(Data::Database *db)
{
    g_db = db;
}

void HistoryViewModel::registerHistoryWriter(Network::HistoryWriterWorker *worker)
{
    g_historyWriter = worker;
}

HistoryViewModel::HistoryViewModel(QObject *parent)
    : QObject(parent)
    , m_searchWatcher(new QFutureWatcher<HistorySearchResult>(this))
    , m_averageExportWatcher(new QFutureWatcher<AverageExportJobResult>(this))
{
    connect(m_searchWatcher, &QFutureWatcher<HistorySearchResult>::finished,
            this, &HistoryViewModel::onSearchCompleted);
    connect(m_averageExportWatcher, &QFutureWatcher<AverageExportJobResult>::finished,
            this, &HistoryViewModel::onAverageExportCompleted);
    reloadFilters();
}

HistoryViewModel::~HistoryViewModel()
{
    abortWorker();
    if (m_searchWatcher->isRunning()) {
        m_searchWatcher->waitForFinished();
    }
    if (m_averageExportWatcher->isRunning()) {
        m_averageExportWatcher->waitForFinished();
    }
}

void HistoryViewModel::setLoggerId(qint64 id)
{
    if (m_loggerId == id)
        return;
    m_loggerId = id;
    loadSensorItems();
    emit loggerIdChanged();
}

void HistoryViewModel::reloadFilters()
{
    loadLoggerItems();
    loadSensorItems();
}

void HistoryViewModel::loadLoggerItems()
{
    m_loggerItems = { filterItem(0, tr("(All loggers)")) };

    if (g_db && g_db->isOpen()) {
        Data::LoggerRepository repo(g_db->connection());
        for (const auto &lg : repo.findAll()) {
            m_loggerItems.append(filterItem(lg.id, lg.name));
        }
    }
    emit loggerItemsChanged();
}

void HistoryViewModel::loadSensorItems()
{
    m_sensorItems = { filterItem(0, tr("(All sensors)")) };

    if (g_db && g_db->isOpen()) {
        Data::SensorReadingRepository repo(g_db->connection());
        for (const auto &p : repo.sensorsWithReadings(m_loggerId)) {
            m_sensorItems.append(filterItem(p.first, p.second));
        }
    }
    emit sensorItemsChanged();
}

void HistoryViewModel::search(const QString &fromDate, const QString &toDate, qint64 sensorId)
{
    if (m_loading)
        return;
    if (!g_db || !g_db->isOpen()) {
        setError(tr("Database not available."));
        return;
    }

    // M-7 fix: date boundaries must use the configured system_timezone so
    // History search and the Dashboard chart agree around midnight.
    const QTimeZone tz = SettingsController::resolveTimeZone();

    const QDate fromDate2 = QDate::fromString(fromDate, QLatin1String(CentralLogger::Format::kDateDdMmYyyy));
    const QDate toDate2   = QDate::fromString(toDate,   QLatin1String(CentralLogger::Format::kDateDdMmYyyy));
    const QDateTime fromDt = fromDate2.isValid()
        ? QDateTime(fromDate2, QTime(0, 0), tz).toUTC()
        : QDateTime();
    const QDateTime toDt = toDate2.isValid()
        ? QDateTime(toDate2.addDays(1), QTime(0, 0), tz).addMSecs(-1).toUTC()
        : QDateTime();

    if (!fromDt.isValid() || !toDt.isValid()) {
        setError(tr("Invalid date format. Use dd/MM/yyyy."));
        return;
    }
    if (fromDt > toDt) {
        setError(tr("Start date must be before end date."));
        return;
    }

    startSearch(m_loggerId, sensorId, fromDt, toDt);
}

void HistoryViewModel::startSearch(qint64 loggerId, qint64 sensorId,
                                   const QDateTime &fromDt, const QDateTime &toDt)
{
    const bool wasShowLogger = showLoggerColumn();
    m_searchLoggerId = loggerId;
    if (!m_searchedOnce) {
        m_searchedOnce = true;
        emit searchedOnceChanged();
    }
    if (wasShowLogger != showLoggerColumn())
        emit showLoggerColumnChanged();

    ++m_searchGeneration;
    m_activeSearchGeneration = m_searchGeneration;

    setError(QString());
    setLoading(true);
    if (!sameQueryAsLast(loggerId, sensorId, fromDt, toDt))
        m_model.clear();
    rememberQuery(loggerId, sensorId, fromDt, toDt);

    HistorySearchParams params;
    params.dbPath   = g_db->connection().databaseName();
    params.loggerId = loggerId;
    params.fromUtc  = fromDt;
    params.toUtc    = toDt;
    params.sensorId = sensorId;
    params.limit    = kHistorySearchLimit;

    m_searchWatcher->setFuture(QtConcurrent::run([params]() {
        return executeHistorySearch(params);
    }));
}

void HistoryViewModel::refresh(const QString &fromDate, const QString &toDate, qint64 sensorId)
{
    if (!g_historyWriter) {
        search(fromDate, toDate, sensorId);
        return;
    }

    // H-D: flushPending() is now non-blocking. Re-run the search only after
    // the writer acknowledges the drain via flushFinished(); a short fallback
    // timer guards against the signal never arriving (e.g. writer not started).
    auto done = std::make_shared<bool>(false);
    auto conn = std::make_shared<QMetaObject::Connection>();
    // Disconnect the flushFinished connection regardless of which path
    // (signal or fallback timer) ran the search, so we don't leak a
    // QMetaObject::Connection registration per Refresh click.
    auto runSearch = [this, fromDate, toDate, sensorId, done, conn]() {
        if (*done)
            return;
        *done = true;
        search(fromDate, toDate, sensorId);
    };
    auto disconnectConn = [conn]() {
        if (*conn)
            QObject::disconnect(*conn);
    };

    *conn = QObject::connect(
        g_historyWriter, &Network::HistoryWriterWorker::flushFinished,
        this, [runSearch, disconnectConn]() {
            disconnectConn();
            runSearch();
        });
    // Fallback: if flushFinished did not fire within 2 s, search anyway.
    QTimer::singleShot(2000, this, [runSearch, disconnectConn]() {
        disconnectConn();
        runSearch();
    });

    g_historyWriter->flushPending();
}

void HistoryViewModel::refreshCurrent()
{
    if (!m_loading && m_hasLastQuery && g_db && g_db->isOpen())
        startSearch(m_lastQueryLoggerId, m_lastQuerySensorId, m_lastQueryFromUtc, m_lastQueryToUtc);
}

void HistoryViewModel::onSearchCompleted()
{
    if (m_activeSearchGeneration != m_searchGeneration) {
        return;
    }

    const HistorySearchResult result = m_searchWatcher->result();
    if (!result.error.isEmpty()) {
        setLoading(false);
        setError(result.error);
        return;
    }

    m_model.mergeLatest(result.rows);
    const int displayed = result.rows.size();
    if (m_recordCount != result.totalCount) {
        m_recordCount = result.totalCount;
        emit recordCountChanged();
    }
    if (m_displayedCount != displayed) {
        m_displayedCount = displayed;
        emit displayedCountChanged();
    }
    setLoading(false);
}

void HistoryViewModel::exportCsv(const QUrl &fileUrl)
{
    const QString path = fileUrl.toLocalFile();
    const auto &rows   = m_model.rows();

    if (rows.isEmpty()) {
        emit exportFinished(false, tr("No data to export."));
        return;
    }

    const QTimeZone tz = SettingsController::resolveTimeZone();
    QString writeErr;
    if (!CentralLogger::Utils::writeUtf8BomFile(
            path,
            [&rows, &tz](QTextStream &out) {
                out << "Time,Logger,Sensor,Unit,Value,Status\n";
                for (const auto &r : rows) {
                    out << CentralLogger::Utils::csvEscape(
                               r.recordedAt.toTimeZone(tz).toString(
                                   QLatin1String(CentralLogger::Format::kDateTimeDdMmYyyyHms)))
                        << ',' << CentralLogger::Utils::csvEscape(r.loggerName)
                        << ',' << CentralLogger::Utils::csvEscape(r.sensorName)
                        << ',' << CentralLogger::Utils::csvEscape(r.unit)
                        << ',' << CentralLogger::Utils::csvEscape(HistoryTableModel::valueText(r))
                        << ',' << HistoryTableModel::statusText(r) << '\n';
                }
            },
            &writeErr)) {
        emit exportFinished(false, writeErr);
        return;
    }
    emit exportFinished(true, path);
}

void HistoryViewModel::exportAverageCsv(const QUrl &fileUrl,
                                        const QString &fromDate,
                                        const QString &toDate)
{
    if (m_averageExportRunning) {
        emit exportFinished(false, tr("An average export is already running."));
        return;
    }
    if (!g_db || !g_db->isOpen()) {
        emit exportFinished(false, tr("Database not available."));
        return;
    }
    if (m_loggerId <= 0) {
        emit exportFinished(false,
                            tr("Select a single logger before exporting averages."));
        return;
    }

    const QString path = fileUrl.toLocalFile();
    if (path.isEmpty()) {
        emit exportFinished(false, tr("No save path specified."));
        return;
    }

    const QTimeZone tz = SettingsController::resolveTimeZone();
    const QDate fromDay = QDate::fromString(fromDate, QLatin1String(CentralLogger::Format::kDateDdMmYyyy));
    const QDate toDay   = QDate::fromString(toDate,   QLatin1String(CentralLogger::Format::kDateDdMmYyyy));
    if (!fromDay.isValid() || !toDay.isValid()) {
        emit exportFinished(false, tr("Invalid date format. Use dd/MM/yyyy."));
        return;
    }
    if (fromDay > toDay) {
        emit exportFinished(false, tr("Start date must be before end date."));
        return;
    }

    AverageExportParams params;
    params.loggerId      = m_loggerId;
    params.timeZone      = tz;
    params.fromUtc = QDateTime(fromDay, QTime(0, 0), tz).toUTC();
    params.toUtc   = QDateTime(toDay.addDays(1), QTime(0, 0), tz)
                         .addMSecs(-1)
                         .toUTC();

    m_averageExportRunning = true;
    const QString dbPath = g_db->connection().databaseName();
    m_averageExportWatcher->setFuture(QtConcurrent::run(
        [dbPath, params, path]() {
            return executeAverageExport(dbPath, params, path);
        }));
}

void HistoryViewModel::onAverageExportCompleted()
{
    m_averageExportRunning = false;
    const AverageExportJobResult result = m_averageExportWatcher->result();
    if (!result.ok) {
        emit exportFinished(false, result.error.isEmpty()
                                       ? tr("Average export failed.")
                                       : result.error);
        return;
    }
    emit exportFinished(true, result.path);
}

void HistoryViewModel::abortWorker()
{
    ++m_searchGeneration;
    if (m_loading) {
        m_loading = false;
        emit loadingChanged();
    }
}

void HistoryViewModel::setLoading(bool v)
{
    if (m_loading == v)
        return;
    m_loading = v;
    emit loadingChanged();
}

void HistoryViewModel::setError(const QString &msg)
{
    if (m_lastError == msg)
        return;
    m_lastError = msg;
    emit lastErrorChanged();
}

bool HistoryViewModel::sameQueryAsLast(qint64 loggerId, qint64 sensorId,
                                       const QDateTime &fromUtc,
                                       const QDateTime &toUtc) const
{
    if (!m_hasLastQuery)
        return false;
    return loggerId == m_lastQueryLoggerId
        && sensorId == m_lastQuerySensorId
        && fromUtc == m_lastQueryFromUtc
        && toUtc == m_lastQueryToUtc;
}

void HistoryViewModel::rememberQuery(qint64 loggerId, qint64 sensorId,
                                     const QDateTime &fromUtc, const QDateTime &toUtc)
{
    m_hasLastQuery       = true;
    m_lastQueryLoggerId  = loggerId;
    m_lastQuerySensorId  = sensorId;
    m_lastQueryFromUtc   = fromUtc;
    m_lastQueryToUtc     = toUtc;
}

} // namespace CentralLogger::Core
