#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace CentralLogger::Data {
class Database;
} // namespace CentralLogger::Data

namespace CentralLogger::Network {

/// Read-only client for the per-logger REST API.
/// One instance lives on the main thread (single QNetworkAccessManager).
/// Endpoints handled here:
///   GET  /api/v1/config           -> configFetched
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

    /// Issues `GET /readings` for the debug dialog. NEVER call from polling
    /// loops, timers, or ModbusBridge — UI buttons only.
    void fetchReadingsDebug(qint64 loggerId);

    /// One-shot probe: GET /config using raw connection params (no DB lookup).
    /// Used by Add Logger "Connect & Load Config". Result is RAM-only
    /// until the user saves the new logger locally.
    void probeConfig(const QString &host, int apiPort, const QString &token);

    /// Aborts any in-flight probeConfig() request and clears its guard.
    void cancelProbe();

signals:
    /// All telemetry/config signals share the same shape:
    ///   ok            : transport + HTTP 2xx
    ///   httpStatus    : HTTP status code, or 0 for transport errors
    ///   rawJson       : pretty-printed response body (or the transport error text)
    ///   errorMessage  : human-friendly message when ok is false
    void configFetched(qint64 loggerId, bool ok, int httpStatus, QString rawJson, QString errorMessage);
    void readingsDebugFetched(qint64 loggerId, bool ok, int httpStatus, QString rawJson, QString errorMessage);

    /// Task 22: probe result — not tied to a loggerId.
    void probeConfigFetched(bool ok, int httpStatus, QString rawJson, QString errorMessage);

private:
    enum class Endpoint { Config, Readings };

    static int bitFor(Endpoint endpoint);

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
    QHash<qint64, int>                       m_inflight;
    bool                                     m_probeInFlight = false;
    QPointer<QNetworkReply>                  m_probeReply;
};

} // namespace CentralLogger::Network
