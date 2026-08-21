#include "RestConfigService.h"

#include "RestConfigParser.h"
#include "data/db/Database.h"
#include "data/repositories/LoggerRepository.h"
#include "utils/AppConstants.h"
#include "utils/FormatConstants.h"
#include "utils/Version.h"

#include <QFile>
#include <QJsonDocument>
#include <QUuid>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QUrl>

namespace CentralLogger::Network {

namespace {

// M-10: separate bits so Fetch and Apply can be guarded independently.
constexpr int kFetchBit    = 1 << 0;
constexpr int kApplyBit    = 1 << 1;
constexpr int kReadingsBit = 1 << 2;

int bitFor(RestConfigService::Endpoint endpoint)
{
    switch (endpoint) {
    case RestConfigService::Endpoint::Apply:    return kApplyBit;
    case RestConfigService::Endpoint::Readings: return kReadingsBit;
    default:                                    return kFetchBit;
    }
}

} // namespace

RestConfigService::RestConfigService(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

RestConfigService::~RestConfigService() = default;

int RestConfigService::httpStatusOf(QNetworkReply *reply)
{
    return reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
}

RestConfigService::LoggerEndpoint
RestConfigService::resolveEndpoint(qint64 loggerId, QString *errorOut) const
{
    LoggerEndpoint ep;
    if (!m_db || !m_db->isOpen()) {
        if (errorOut) *errorOut = QStringLiteral("Database not open");
        return ep;
    }
    Data::LoggerRepository repo(m_db->connection());
    QString err;
    const auto info = repo.findById(loggerId, &err);
    if (!info) {
        if (errorOut) *errorOut = err.isEmpty()
            ? QStringLiteral("Logger %1 not found").arg(loggerId)
            : err;
        return ep;
    }
    if (info->host.isEmpty() || info->apiPort <= 0) {
        if (errorOut) *errorOut = QStringLiteral("Logger has no host / API port");
        return ep;
    }
    // RFC 3986: IPv6 literals in a URL must be wrapped in [...].
    // QHostAddress returns IPv6Protocol for any address with ':' (including
    // IPv4-mapped), so detect with a quick check rather than parsing.
    const bool needsBracket = info->host.contains(QLatin1Char(':'));
    const QString hostForUrl = needsBracket
        ? QStringLiteral("[%1]").arg(info->host)
        : info->host;
    ep.baseUrl = QStringLiteral("http://%1:%2/api/v1").arg(hostForUrl).arg(info->apiPort);
    ep.token   = info->apiToken;
    ep.valid   = true;
    return ep;
}

bool RestConfigService::startGuard(qint64 loggerId, Endpoint endpoint)
{
    const int bit  = bitFor(endpoint);
    const int cur  = m_inflight.value(loggerId, 0);
    if (cur & bit) return false;
    m_inflight.insert(loggerId, cur | bit);
    return true;
}

void RestConfigService::releaseGuard(qint64 loggerId, Endpoint endpoint)
{
    const int cur  = m_inflight.value(loggerId, 0);
    const int next = cur & ~bitFor(endpoint);
    if (next == 0) {
        m_inflight.remove(loggerId);
    } else {
        m_inflight.insert(loggerId, next);
    }
}

void RestConfigService::fetchConfig(qint64 loggerId)
{
    if (!startGuard(loggerId, Endpoint::Config)) {
        emit configFetched(loggerId, false, 0, QString{},
                           QStringLiteral("Config fetch already in progress for this logger"));
        return;
    }

    QString resolveErr;
    const auto ep = resolveEndpoint(loggerId, &resolveErr);
    if (!ep.valid) {
        releaseGuard(loggerId, Endpoint::Config);
        emit configFetched(loggerId, false, 0, QString{}, resolveErr);
        return;
    }

    // M-7: GET requests carry no body — do not send Content-Type.
    QNetworkRequest req{ QUrl(ep.baseUrl + QStringLiteral("/config")) };
    req.setTransferTimeout(CentralLogger::Defaults::kRestTransferTimeoutMs); // M-6: prevent indefinite hang on slow device
    if (!ep.token.isEmpty()) {
        req.setRawHeader("Authorization", "Bearer " + ep.token.toUtf8());
    }

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, loggerId]() {
        reply->deleteLater();
        releaseGuard(loggerId, Endpoint::Config);
        const int     status = httpStatusOf(reply);
        const QByteArray body = reply->readAll();
        const bool ok = (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300);
        const QString pretty = RestConfigParser::prettyJson(body);
        if (ok) {
            emit configFetched(loggerId, true, status, pretty, QString{});
        } else {
            emit configFetched(loggerId, false, status, pretty,
                               RestConfigParser::formatRestError(status, body, reply->errorString()));
        }
    });
}

void RestConfigService::applyConfig(qint64 loggerId,
                                    int expectedRevision,
                                    const QJsonObject &configPatch)
{
    // M-10: Endpoint::Apply has its own bit — a concurrent fetchConfig does
    // not block apply (and vice-versa). When busy, emit an error instead of
    // silently dropping the request.
    if (!startGuard(loggerId, Endpoint::Apply)) {
        emit configApplied(loggerId, false, 0, QString{},
                           QStringLiteral("Config apply already in progress for this logger"));
        return;
    }

    QString resolveErr;
    const auto ep = resolveEndpoint(loggerId, &resolveErr);
    if (!ep.valid) {
        releaseGuard(loggerId, Endpoint::Apply);
        emit configApplied(loggerId, false, 0, QString{}, resolveErr);
        return;
    }

    QJsonObject envelope;
    envelope.insert(QStringLiteral("api_version"), 1);
    envelope.insert(QStringLiteral("request_id"),
                    QStringLiteral("central-%1")
                        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    envelope.insert(QStringLiteral("expected_revision"), expectedRevision);
    envelope.insert(QStringLiteral("config"), configPatch);
    const QByteArray payload = QJsonDocument(envelope).toJson(QJsonDocument::Compact);

    QNetworkRequest req{ QUrl(ep.baseUrl + QStringLiteral("/config")) };
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(CentralLogger::Defaults::kRestTransferTimeoutMs); // M-6: prevent indefinite hang on slow device
    if (!ep.token.isEmpty()) {
        req.setRawHeader("Authorization", "Bearer " + ep.token.toUtf8());
    }

    QNetworkReply *reply = m_nam->post(req, payload);
    connect(reply, &QNetworkReply::finished, this, [this, reply, loggerId]() {
        reply->deleteLater();
        releaseGuard(loggerId, Endpoint::Apply);
        const int     status = httpStatusOf(reply);
        const QByteArray body = reply->readAll();
        const bool ok = (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300);
        const QString pretty = RestConfigParser::prettyJson(body);
        if (ok) {
            emit configApplied(loggerId, true, status, pretty, QString{});
        } else {
            emit configApplied(loggerId, false, status, pretty,
                               RestConfigParser::formatRestError(status, body, reply->errorString()));
        }
    });
}

void RestConfigService::fetchReadingsDebug(qint64 loggerId)
{
    if (!startGuard(loggerId, Endpoint::Readings)) {
        // Guard is busy — emit so the VM can clear its busy state. Without
        // this the UI's busy flag is stuck forever (re-entrant request for
        // the same logger).
        emit readingsDebugFetched(loggerId, false, 0, QString{},
            QStringLiteral("Readings request already in progress."));
        return;
    }

    QString resolveErr;
    const auto ep = resolveEndpoint(loggerId, &resolveErr);
    if (!ep.valid) {
        releaseGuard(loggerId, Endpoint::Readings);
        emit readingsDebugFetched(loggerId, false, 0, QString{}, resolveErr);
        return;
    }
    if (ep.token.isEmpty()) {
        releaseGuard(loggerId, Endpoint::Readings);
        emit readingsDebugFetched(loggerId, false, 0, QString{},
            CentralLogger::Format::kErrRestTokenEmpty);
        return;
    }

    QNetworkRequest req{ QUrl(ep.baseUrl + QStringLiteral("/readings")) };
    req.setRawHeader("Authorization", "Bearer " + ep.token.toUtf8());
    req.setTransferTimeout(CentralLogger::Defaults::kRestTransferTimeoutMs); // M-6: prevent indefinite hang on slow device

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, loggerId]() {
        reply->deleteLater();
        releaseGuard(loggerId, Endpoint::Readings);
        const int     status = httpStatusOf(reply);
        const QByteArray body = reply->readAll();
        const bool ok = (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300);
        const QString pretty = RestConfigParser::prettyJson(body);
        if (ok) {
            emit readingsDebugFetched(loggerId, true, status, pretty, QString{});
        } else {
            emit readingsDebugFetched(loggerId, false, status, pretty,
                                      RestConfigParser::formatRestError(status, body, reply->errorString()));
        }
    });
}

// ---------------------------------------------------------------------------
// Task 22: probeConfig — one-shot GET /config from raw connection params
// ---------------------------------------------------------------------------

namespace {

/// Maps probe errors to user-friendly messages that help the user fix
/// the form fields before saving. Contract: rest-config-contract-v1.md §UX.
QString humanizeProbeError(int httpStatus,
                           const QByteArray &body,
                           const QString &transportError)
{
    if (httpStatus == 0) {
        if (transportError.contains(QLatin1String("Connection refused"), Qt::CaseInsensitive))
            return QStringLiteral("Connection refused. Check host and API port.");
        if (transportError.contains(QLatin1String("timed out"), Qt::CaseInsensitive)
         || transportError.contains(QLatin1String("timeout"), Qt::CaseInsensitive))
            return QStringLiteral("Connection timed out. Check host, API port, and network.");
        if (transportError.contains(QLatin1String("Host not found"), Qt::CaseInsensitive)
         || transportError.contains(QLatin1String("not found"), Qt::CaseInsensitive))
            return QStringLiteral("Host not found. Check hostname or IP address.");
        return transportError.isEmpty()
            ? QLatin1String(CentralLogger::Format::kErrLoggerUnreachable)
            : QString(QLatin1String(CentralLogger::Format::kErrLoggerUnreachableFmt)).arg(transportError);
    }
    // Delegate to the shared error formatter for HTTP-level errors.
    return RestConfigParser::formatRestError(httpStatus, body, transportError);
}

} // namespace

void RestConfigService::probeConfig(const QString &host, int apiPort, const QString &token)
{
    if (m_probeInFlight) {
        // Same as fetchReadingsDebug: emit so the form's busy state can
        // recover instead of waiting forever for a probeConfigResult that
        // would never arrive from the in-flight request.
        emit probeConfigFetched(false, 0, QString{},
            QStringLiteral("Probe already in progress."));
        return;
    }

    if (host.trimmed().isEmpty() || apiPort <= 0) {
        emit probeConfigFetched(false, 0, QString{},
            QStringLiteral("Host and API port are required."));
        return;
    }

    m_probeInFlight = true;

    const QString baseUrl = QStringLiteral("http://%1:%2/api/v1").arg(host.trimmed()).arg(apiPort);

    QNetworkRequest req{ QUrl(baseUrl + QStringLiteral("/config")) };
    // M-7: GET /config carries no body — don't send Content-Type. (Apply
    // below does the same, so probe and fetch stay consistent.)
    req.setTransferTimeout(CentralLogger::Defaults::kRestProbeTimeoutMs); // 8s timeout for probe
    if (!token.trimmed().isEmpty()) {
        req.setRawHeader("Authorization", "Bearer " + token.trimmed().toUtf8());
    }

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_probeInFlight = false;
        const int        status = httpStatusOf(reply);
        const QByteArray body   = reply->readAll();
        const bool ok = (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300);
        const QString pretty = RestConfigParser::prettyJson(body);
        if (ok) {
            emit probeConfigFetched(true, status, pretty, QString{});
        } else {
            emit probeConfigFetched(false, status, pretty,
                                    humanizeProbeError(status, body, reply->errorString()));
        }
    });
}

// ---------------------------------------------------------------------------
// Task 20: downloadLatestReport — GET /reports/latest → save to file
// ---------------------------------------------------------------------------

void RestConfigService::downloadLatestReport(qint64 loggerId, const QString &savePath)
{
    if (m_reportInFlight.contains(loggerId)) {
        emit reportDownloaded(loggerId, false, QString{},
            QStringLiteral("Report download already in progress for this logger"));
        return;
    }

    if (savePath.isEmpty()) {
        emit reportDownloaded(loggerId, false, QString{},
            QStringLiteral("No save path specified"));
        return;
    }

    QString resolveErr;
    const auto ep = resolveEndpoint(loggerId, &resolveErr);
    if (!ep.valid) {
        emit reportDownloaded(loggerId, false, QString{}, resolveErr);
        return;
    }
    if (ep.token.isEmpty()) {
        emit reportDownloaded(loggerId, false, QString{},
            CentralLogger::Format::kErrRestTokenEmpty);
        return;
    }

    m_reportInFlight.insert(loggerId);

    QNetworkRequest req{ QUrl(ep.baseUrl + QStringLiteral("/reports/latest")) };
    req.setRawHeader("Authorization", "Bearer " + ep.token.toUtf8());
    req.setTransferTimeout(CentralLogger::Defaults::kRestReportTimeoutMs); // M-6: reports may be large; allow 30s transfer

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, loggerId, savePath]() {
        reply->deleteLater();
        m_reportInFlight.remove(loggerId);

        const int status = httpStatusOf(reply);
        const bool httpOk = (reply->error() == QNetworkReply::NoError
                             && status >= 200 && status < 300);

        if (!httpOk) {
            const QByteArray body = reply->readAll();
            emit reportDownloaded(loggerId, false, QString{},
                RestConfigParser::formatReportDownloadError(status, body, reply->errorString()));
            return;
        }

        // M-8: guard against unbounded readAll() for potentially large files.
        // Check Content-Length first; if absent, cap at read time.
        constexpr qint64 kMaxBytes = CentralLogger::Defaults::kRestReportMaxBytes; // 50 MB
        const qint64 contentLength =
            reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
        if (contentLength > kMaxBytes) {
            emit reportDownloaded(loggerId, false, QString{},
                QString(QLatin1String(CentralLogger::Format::kErrReportTooLarge))
                    .arg(contentLength / (1024 * 1024)));
            return;
        }

        const QByteArray data = reply->readAll();
        if (data.size() > kMaxBytes) {
            emit reportDownloaded(loggerId, false, QString{},
                CentralLogger::Format::kErrReportDataTooLarge);
            return;
        }
        QFile file(savePath);
        if (!file.open(QIODevice::WriteOnly)) {
            emit reportDownloaded(loggerId, false, QString{},
                QStringLiteral("Cannot write to file: %1").arg(file.errorString()));
            return;
        }
        file.write(data);
        file.close();

        emit reportDownloaded(loggerId, true, savePath, QString{});
    });
}

} // namespace CentralLogger::Network
