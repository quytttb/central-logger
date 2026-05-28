#include "RestConfigService.h"

#include "RestConfigParser.h"
#include "data/db/Database.h"
#include "data/repositories/LoggerRepository.h"

#include <QFile>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QUrl>
#include <QtDebug>

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
    ep.baseUrl = QStringLiteral("http://%1:%2/api/v1").arg(info->host).arg(info->apiPort);
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
    req.setTransferTimeout(10000); // M-6: prevent indefinite hang on slow device
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
    envelope.insert(QStringLiteral("expected_revision"), expectedRevision);
    envelope.insert(QStringLiteral("config"), configPatch);
    const QByteArray payload = QJsonDocument(envelope).toJson(QJsonDocument::Compact);

    QNetworkRequest req{ QUrl(ep.baseUrl + QStringLiteral("/config")) };
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(10000); // M-6: prevent indefinite hang on slow device
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
            const QString formatted = RestConfigParser::formatRestError(status, body, reply->errorString());
            qWarning() << "[RestConfig] Apply config failed. Status:" << status << "Error:" << formatted << "Body:" << body;
            emit configApplied(loggerId, false, status, pretty, formatted);
        }
    });
}

void RestConfigService::fetchReadingsDebug(qint64 loggerId)
{
    if (!startGuard(loggerId, Endpoint::Readings)) {
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
            QStringLiteral("Device REST token empty — Scan QR on logger"));
        return;
    }

    QNetworkRequest req{ QUrl(ep.baseUrl + QStringLiteral("/readings")) };
    req.setRawHeader("Authorization", "Bearer " + ep.token.toUtf8());
    req.setTransferTimeout(10000); // M-6: prevent indefinite hang on slow device

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
            ? QStringLiteral("Could not reach the logger. Check host, API port, and network.")
            : QStringLiteral("Could not reach the logger: %1").arg(transportError);
    }
    // Delegate to the shared error formatter for HTTP-level errors.
    return RestConfigParser::formatRestError(httpStatus, body, transportError);
}

} // namespace

void RestConfigService::probeConfig(const QString &host, int apiPort, const QString &token)
{
    if (m_probeInFlight) return;  // one at a time

    if (host.trimmed().isEmpty() || apiPort <= 0) {
        emit probeConfigFetched(false, 0, QString{},
            QStringLiteral("Host and API port are required."));
        return;
    }

    m_probeInFlight = true;

    const QString baseUrl = QStringLiteral("http://%1:%2/api/v1").arg(host.trimmed()).arg(apiPort);

    QNetworkRequest req{ QUrl(baseUrl + QStringLiteral("/config")) };
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(8000); // 8s timeout for probe
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
            QStringLiteral("Device REST token empty — Scan QR on logger"));
        return;
    }

    m_reportInFlight.insert(loggerId);

    QNetworkRequest req{ QUrl(ep.baseUrl + QStringLiteral("/reports/latest")) };
    req.setRawHeader("Authorization", "Bearer " + ep.token.toUtf8());
    req.setTransferTimeout(30000); // M-6: reports may be large; allow 30s transfer

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
                RestConfigParser::formatRestError(status, body, reply->errorString()));
            return;
        }

        // M-8: guard against unbounded readAll() for potentially large files.
        // Check Content-Length first; if absent, cap at read time.
        constexpr qint64 kMaxBytes = 50LL * 1024 * 1024; // 50 MB
        const qint64 contentLength =
            reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
        if (contentLength > kMaxBytes) {
            emit reportDownloaded(loggerId, false, QString{},
                QStringLiteral("Report too large (%1 MB, limit 50 MB)")
                    .arg(contentLength / (1024 * 1024)));
            return;
        }

        const QByteArray data = reply->readAll();
        if (data.size() > kMaxBytes) {
            emit reportDownloaded(loggerId, false, QString{},
                QStringLiteral("Report data exceeds 50 MB limit"));
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
