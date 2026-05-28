#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace CentralLogger::Data {
class Database;
} // namespace CentralLogger::Data

namespace CentralLogger::Network {

/// Talks to the per-logger REST API per docs/contracts/rest-config-contract-v1.md.
/// One instance lives on the main thread (single QNetworkAccessManager).
/// Endpoints handled here:
///   GET  /api/v1/config           -> configFetched
///   POST /api/v1/config           -> configApplied
///   GET  /api/v1/readings (debug) -> readingsDebugFetched (one-shot, never auto)
class RestConfigService : public QObject
{
    Q_OBJECT

public:
    explicit RestConfigService(QObject *parent = nullptr);
    ~RestConfigService() override;

    void setDatabase(Data::Database *db) { m_db = db; }

public slots:
    /// Issues a `GET /config`. Emits `configFetched` with the raw body once
    /// the reply finishes. Concurrent calls for the same loggerId are
    /// coalesced — only the first wins until the reply lands.
    void fetchConfig(qint64 loggerId);

    /// Issues `POST /config` with `{ "expected_revision": N, "config": ... }`.
    void applyConfig(qint64 loggerId, int expectedRevision, const QJsonObject &configPatch);

    /// Issues `GET /readings` for the debug dialog. NEVER call from polling
    /// loops, timers, or ModbusBridge — UI buttons only.
    void fetchReadingsDebug(qint64 loggerId);

    /// One-shot probe: GET /config using raw connection params (no DB lookup).
    /// Used by LoggerFormDialog "Connect & Load Config" before saving.
    /// Result is RAM-only until the user clicks Save.  Task 22 (FE-022).
    void probeConfig(const QString &host, int apiPort, const QString &token);

    /// Task 20 (FE-021): GET /reports/latest — downloads the report file
    /// (binary/text) from the edge and saves it to `savePath`.
    /// Emits `reportDownloaded` when done. On-demand only (user button).
    void downloadLatestReport(qint64 loggerId, const QString &savePath);

signals:
    /// All three signals share the same shape:
    ///   ok            : transport + HTTP 2xx
    ///   httpStatus    : HTTP status code, or 0 for transport errors
    ///   rawJson       : pretty-printed response body (or the transport error text)
    ///   errorMessage  : human-friendly message when ok is false
    void configFetched(qint64 loggerId, bool ok, int httpStatus, QString rawJson, QString errorMessage);
    void configApplied(qint64 loggerId, bool ok, int httpStatus, QString rawJson, QString errorMessage);
    void readingsDebugFetched(qint64 loggerId, bool ok, int httpStatus, QString rawJson, QString errorMessage);

    /// Task 22: probe result — not tied to a loggerId.
    void probeConfigFetched(bool ok, int httpStatus, QString rawJson, QString errorMessage);

    /// Task 20: report download result.
    ///   ok         : transport + HTTP 2xx + file written successfully
    ///   savePath   : the path the file was saved to (empty on error)
    ///   errorMessage : human-friendly message when ok is false
    void reportDownloaded(qint64 loggerId, bool ok, QString savePath, QString errorMessage);

public:
    /// M-10: Fetch and Apply use separate guard bits so they can run
    /// concurrently for the same logger without blocking each other.
    enum class Endpoint { Config, Apply, Readings, Probe };

private:
    struct LoggerEndpoint
    {
        QString baseUrl;  // http://host:port/api/v1
        QString token;
        bool    valid = false;
    };

    LoggerEndpoint resolveEndpoint(qint64 loggerId, QString *errorOut) const;
    bool           startGuard(qint64 loggerId, Endpoint endpoint);
    void           releaseGuard(qint64 loggerId, Endpoint endpoint);

    static int httpStatusOf(QNetworkReply *reply);

    QNetworkAccessManager *m_nam = nullptr;
    Data::Database        *m_db  = nullptr;

    // (loggerId, endpoint) -> in-flight; prevents double-clicks stacking.
    QHash<qint64, int> m_inflight;
    bool        m_probeInFlight = false;
    QSet<qint64> m_reportInFlight; // per-logger; allows concurrent downloads to different loggers
};

} // namespace CentralLogger::Network
