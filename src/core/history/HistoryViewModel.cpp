#include "HistoryViewModel.h"

#include "data/db/Database.h"
#include "data/repositories/LoggerRepository.h"
#include "data/repositories/SensorReadingRepository.h"
#include "network/workers/HistoryWriterWorker.h"

#include <QDateTime>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QTextStream>
#include <QThread>
#include <QtConcurrent>
#include <QVariantMap>

namespace CentralLogger::Core {

namespace {

constexpr int kHistorySearchLimit = 5000;

QVariantMap filterItem(qint64 id, const QString &name)
{
    QVariantMap item;
    item[QStringLiteral("id")]   = id;
    item[QStringLiteral("name")] = name;
    return item;
}

QString csvEscape(const QString &s)
{
    if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"')) ||
        s.contains(QLatin1Char('\n'))) {
        return QStringLiteral("\"%1\"").arg(
            QString(s).replace(QStringLiteral("\""), QStringLiteral("\"\"")));
    }
    return s;
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

    const QString connName = QStringLiteral("history_search_%1").arg(
        reinterpret_cast<quintptr>(QThread::currentThreadId()));

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
    db.setDatabaseName(params.dbPath);
    if (!db.open()) {
        result.error = db.lastError().text();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(connName);
        return result;
    }

    {
        Data::SensorReadingRepository repo(db);
        result.totalCount = repo.countHistory(params.loggerId,
                                              params.fromUtc,
                                              params.toUtc,
                                              params.sensorId,
                                              &result.error);
        if (!result.error.isEmpty()) {
            db.close();
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(connName);
            return result;
        }
        result.rows = repo.searchHistory(params.loggerId,
                                         params.fromUtc,
                                         params.toUtc,
                                         params.sensorId,
                                         params.limit,
                                         &result.error);
    }

    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connName);
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
{
    connect(m_searchWatcher, &QFutureWatcher<HistorySearchResult>::finished,
            this, &HistoryViewModel::onSearchCompleted);
    reloadFilters();
}

HistoryViewModel::~HistoryViewModel()
{
    abortWorker();
    if (m_searchWatcher->isRunning()) {
        m_searchWatcher->waitForFinished();
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

    const QDateTime fromDt = QDateTime::fromString(fromDate, QStringLiteral("dd/MM/yyyy"))
                                  .toUTC();
    const QDateTime toDt   = QDateTime::fromString(toDate,   QStringLiteral("dd/MM/yyyy"))
                                  .addDays(1).addSecs(-1).toUTC();

    if (!fromDt.isValid() || !toDt.isValid()) {
        setError(tr("Invalid date format. Use dd/MM/yyyy."));
        return;
    }
    if (fromDt > toDt) {
        setError(tr("Start date must be before end date."));
        return;
    }

    const bool wasShowLogger = showLoggerColumn();
    m_searchLoggerId = m_loggerId;
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
    if (!sameQueryAsLast(m_loggerId, sensorId, fromDt, toDt))
        m_model.clear();
    rememberQuery(m_loggerId, sensorId, fromDt, toDt);

    HistorySearchParams params;
    params.dbPath   = g_db->connection().databaseName();
    params.loggerId = m_loggerId;
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
    if (g_historyWriter) {
        g_historyWriter->flushPending();
    }
    search(fromDate, toDate, sensorId);
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

    m_model.setRows(result.rows);
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

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        emit exportFinished(false, tr("Cannot open file: %1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    out << "\xEF\xBB\xBF";
    out << "Time,Logger,Sensor,Unit,Value,Status\n";

    for (const auto &r : rows) {
        out << csvEscape(r.recordedAt.toLocalTime()
                              .toString(QStringLiteral("dd/MM/yyyy HH:mm:ss"))) << ','
            << csvEscape(r.loggerName) << ','
            << csvEscape(r.sensorName) << ','
            << csvEscape(r.unit) << ','
            << QString::number(r.value, 'f', qBound(0, r.decimals, 6)) << ','
            << HistoryTableModel::statusText(r) << '\n';
    }

    file.close();
    emit exportFinished(true, path);
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
