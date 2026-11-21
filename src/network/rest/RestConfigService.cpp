#include "RestConfigService.h"

#include "RestConfigParser.h"
#include "data/db/Database.h"
#include "data/repositories/LoggerRepository.h"
#include "utils/AppConstants.h"
#include "utils/FormatConstants.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace CentralLogger::Network {

namespace {

constexpr int kFetchBit    = 1 << 0;
constexpr int kReadingsBit = 1 << 1;

QString formatBaseUrl(const QString &host, int port)
{
    const QString trimmed = host.trimmed();
    // RFC 3986: IPv6 literals in a URL must be wrapped in [...].
    // Check for ':' rather than parsing with QHostAddress.
    const QString hostForUrl = trimmed.contains(QLatin1Char(':'))
        ? QStringLiteral("[%1]").arg(trimmed)
        : trimmed;
    return QStringLiteral("http://%1:%2/api/v1").arg(hostForUrl).arg(port);
}

} // namespace

int RestConfigService::bitFor(Endpoint endpoint)
{
    switch (endpoint) {
    case Endpoint::Readings: return kReadingsBit;
    default:                return kFetchBit;
    }
}

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
        if (errorOut) *errorOut = QLatin1String(CentralLogger::Format::kErrDatabaseNotOpen);
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
    if (info->host.trimmed().isEmpty() || info->apiPort <= 0) {
        if (errorOut) *errorOut = QStringLiteral("Logger has no host / API port");
        return ep;
    }
    ep.baseUrl = formatBaseUrl(info->host, info->apiPort);
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

void RestConfigService::cancelProbe()
{
    m_probeInFlight = false;
    auto reply = m_probeReply;
    m_probeReply = nullptr;
    if (reply) {
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
}

void RestConfigService::fetchConfig(qint64 loggerId)
{
    if (!startGuard(loggerId, Endpoint::Config)) {
        // Coalesce concurrent GET /config calls for the same logger: all
        // subscribers receive the in-flight reply's configFetched signal.
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
            qInfo().noquote() << "[readings-debug] loggerId=" << loggerId
                              << "status=" << status << "\n" << pretty;
            emit readingsDebugFetched(loggerId, true, status, pretty, QString{});
        } else {
            const QString errMsg = RestConfigParser::formatRestError(status, body, reply->errorString());
            qWarning().noquote() << "[readings-debug] loggerId=" << loggerId
                                 << "failed status=" << status << "—" << errMsg;
            emit readingsDebugFetched(loggerId, false, status, pretty, errMsg);
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

    const QString baseUrl = formatBaseUrl(host, apiPort);

    QNetworkRequest req{ QUrl(baseUrl + QStringLiteral("/config")) };
    // Like fetchConfig(), send a bodyless GET without Content-Type.
    req.setTransferTimeout(CentralLogger::Defaults::kRestProbeTimeoutMs); // 8s timeout for probe
    if (!token.trimmed().isEmpty()) {
        req.setRawHeader("Authorization", "Bearer " + token.trimmed().toUtf8());
    }

    QNetworkReply *reply = m_nam->get(req);
    m_probeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_probeReply == reply) {
            m_probeReply = nullptr;
        }
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

} // namespace CentralLogger::Network
