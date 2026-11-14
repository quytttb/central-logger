#include "OsgSource.h"

#include "utils/AdapterConstants.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

namespace CentralLogger::Adapters::Osg {

namespace {
using namespace CentralLogger::AdapterDefaults;
bool containsExact(const QJsonArray &values, const QJsonValue &value)
{
    if (value.isUndefined())
        return false;
    for (const auto &candidate : values) {
        if (candidate == value)
            return true;
    }
    return false;
}

qint64 timestampMs(const QJsonValue &value)
{
    bool ok = false;
    qint64 result = 0;
    if (value.isDouble()) {
        const double raw = value.toDouble();
        if (std::isfinite(raw) && raw > 0 && raw < kMaxSourceTimestampMs && std::floor(raw) == raw) {
            result = static_cast<qint64>(raw);
            ok = true;
        }
    } else if (value.isString()) {
        result = value.toString().toLongLong(&ok);
    }
    return ok && result > 0 && result < kMaxSourceTimestampMs ? result : 0;
}

QJsonObject safeFields(const QJsonObject &object)
{
    QJsonObject safe;
    for (const auto *key : {"id", "sensor_id", "name", "unit", "value", "status",
                            "time", "enabled", "alert", "syncing"}) {
        const auto value = object.value(QLatin1String(key));
        if (!value.isUndefined() && !value.isObject() && !value.isArray())
            safe.insert(QLatin1String(key), value);
    }
    return safe;
}

bool numberFromValue(const QJsonValue &raw, float *out)
{
    if (!raw.isString() || raw.toString().trimmed().isEmpty())
        return false;
    bool ok = false;
    const double value = raw.toString().trimmed().toDouble(&ok);
    if (!ok || !std::isfinite(value)
        || std::abs(value) > std::numeric_limits<float>::max())
        return false;
    *out = static_cast<float>(value);
    return std::isfinite(*out);
}

QString stringField(const QJsonObject &object, const char *field)
{
    return object.value(QLatin1String(field)).toString();
}
} // namespace

OsgSource::OsgSource(Settings settings, QObject *parent)
    : QObject(parent), m_settings(std::move(settings))
{
    // Do not accidentally send the user's OSG credentials to a system proxy.
    m_network.setProxy(QNetworkProxy::NoProxy);
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &OsgSource::startCycle);
}

void OsgSource::start()
{
    if (m_running)
        return;
    m_running = true;
    schedule(0);
}

void OsgSource::stop()
{
    m_running = false;
    m_online = false;
    m_timer.stop();
    if (m_reply)
        m_reply->abort();
    m_token.clear();
}

bool OsgSource::parseArrayResponse(const QByteArray &body, QJsonArray *out)
{
    const auto doc = QJsonDocument::fromJson(body);
    if (!doc.isObject() || !doc.object().value(QStringLiteral("data")).isArray())
        return false;
    *out = doc.object().value(QStringLiteral("data")).toArray();
    return true;
}

bool OsgSource::parseId(const QJsonObject &object, quint16 *id)
{
    auto value = object.value(QStringLiteral("id"));
    if (value.isUndefined())
        value = object.value(QStringLiteral("sensor_id"));
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < 0 || number > 65535
        || std::floor(number) != number)
        return false;
    *id = static_cast<quint16>(number);
    return true;
}

bool OsgSource::parseNumericValue(const QJsonValue &value, float *out)
{
    return numberFromValue(value, out);
}

qint64 OsgSource::parseTimestampMs(const QJsonValue &value)
{
    return timestampMs(value);
}

void OsgSource::schedule(int delayMs)
{
    if (m_running)
        m_timer.start(delayMs);
}

void OsgSource::startCycle()
{
    if (!m_running || m_reply)
        return;
    m_reauthenticated = false;
    if (m_token.isEmpty())
        login();
    else if (m_catalog.isEmpty()
             || QDateTime::currentMSecsSinceEpoch() - m_catalogFetchedAtMs >= kCatalogRefreshMs)
        fetchCatalog();
    else {
        m_pendingCatalog = m_catalog;
        m_pendingCatalogRefreshed = false;
        fetchRealtime();
    }
}

void OsgSource::login() { issue(Step::Login); }
void OsgSource::fetchCatalog() { issue(Step::Catalog); }
void OsgSource::fetchRealtime() { issue(Step::Realtime); }

void OsgSource::issue(Step step)
{
    if (!m_running || m_reply)
        return;
    QUrl url = m_settings.url;
    switch (step) {
    case Step::Login:    url.setPath(QStringLiteral("/api/auth/login")); break;
    case Step::Catalog:  url.setPath(QStringLiteral("/api/sensors")); break;
    case Step::Realtime: url.setPath(QStringLiteral("/api/data/realtime")); break;
    }
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(m_settings.timeoutMs);
    if (step == Step::Login) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        const QJsonObject payload{{QStringLiteral("username"), m_settings.credentials.username},
                                  {QStringLiteral("password"), m_settings.credentials.password}};
        m_reply = m_network.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    } else {
        request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
        m_reply = m_network.get(request);
    }

    QNetworkReply *reply = m_reply;
    reply->setReadBufferSize(kMaxUpstreamBodyBytes + 1);
    auto bytes = std::make_shared<QByteArray>();
    auto overflow = std::make_shared<bool>(false);
    auto *deadline = new QTimer(reply);
    deadline->setSingleShot(true);
    connect(deadline, &QTimer::timeout, reply, &QNetworkReply::abort);
    deadline->start(m_settings.timeoutMs);
    connect(reply, &QNetworkReply::readyRead, this, [reply, bytes, overflow]() {
        bytes->append(reply->readAll());
        if (bytes->size() > kMaxUpstreamBodyBytes) {
            *overflow = true;
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, step, bytes, overflow]() {
        bytes->append(reply->readAll());
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool ok = reply->error() == QNetworkReply::NoError
                        && !*overflow && bytes->size() <= kMaxUpstreamBodyBytes;
        m_reply = nullptr;
        reply->deleteLater();
        if (m_running)
            complete(step, status, *bytes, ok);
    });
}

void OsgSource::complete(Step step, int httpStatus, const QByteArray &body, bool transportOk)
{
    if (step != Step::Login && httpStatus == 401 && !m_reauthenticated) {
        m_reauthenticated = true;
        m_token.clear();
        login();
        return;
    }
    if (!transportOk || httpStatus < 200 || httpStatus >= 300) {
        if (httpStatus == 401) {
            m_token.clear();
            fail("OSG authentication rejected");
        } else if (httpStatus == 0) {
            fail("OSG network request failed or timed out");
        } else {
            fail("OSG API request failed");
        }
        return;
    }

    if (step == Step::Login) {
        const auto doc = QJsonDocument::fromJson(body);
        const auto data = doc.object().value(QStringLiteral("data")).toObject();
        const auto token = data.value(QStringLiteral("accessToken"));
        if (!doc.isObject() || !token.isString() || token.toString().isEmpty()) {
            fail("OSG login response invalid");
            return;
        }
        m_token = token.toString();
        fetchCatalog();
        return;
    }

    QJsonArray array;
    if (!parseArrayResponse(body, &array)) {
        fail("OSG response schema not recognized");
        return;
    }
    QSet<quint16> ids;
    for (const auto &value : array) {
        quint16 id = 0;
        if (!value.isObject() || !parseId(value.toObject(), &id) || ids.contains(id)) {
            fail("OSG sensor identifiers invalid or duplicated");
            return;
        }
        ids.insert(id);
    }
    if (step == Step::Catalog) {
        m_pendingCatalog = array;
        m_pendingCatalogRefreshed = true;
        fetchRealtime();
        return;
    }

    applyData(m_pendingCatalog, array);
    m_online = true;
    m_lastSuccessMs = QDateTime::currentMSecsSinceEpoch();
    m_failures = 0;
    emit sourceStateChanged();
    schedule(m_settings.pollIntervalMs);
}

void OsgSource::fail(const char *reason)
{
    m_online = false;
    qWarning("%s; retrying with backoff", reason);
    emit sourceStateChanged();
    const int shift = qMin(m_failures, 6);
    m_failures = qMin(m_failures + 1, 6);
    const int base = qMin(kMaximumBackoffMs, kInitialBackoffMs * (1 << shift));
    const int jitter = QRandomGenerator::global()->bounded(qMax(1, base / 4));
    schedule(qMin(kMaximumBackoffMs, base + jitter));
}

void OsgSource::applyData(const QJsonArray &catalog, const QJsonArray &readings)
{
    QHash<quint16, QJsonObject> values;
    for (const auto &value : readings) {
        quint16 id = 0;
        const auto object = value.toObject();
        if (parseId(object, &id))
            values.insert(id, safeFields(object));
    }
    m_catalog = catalog;
    if (m_pendingCatalogRefreshed)
        m_catalogFetchedAtMs = QDateTime::currentMSecsSinceEpoch();
    m_pendingCatalogRefreshed = false;
    m_hasCatalog = true;
    m_readings = std::move(values);
}

EdgeSnapshot OsgSource::snapshot() const
{
    EdgeSnapshot result;
    result.polling = m_running;
    result.rtuConnected = false; // HTTP reachability does not prove physical RTU state.
    result.upstreamConnected = m_online;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool upstreamStale = !m_online || m_lastSuccessMs <= 0
        || now - m_lastSuccessMs > m_settings.staleAfterMs;

    for (const auto &value : m_catalog) {
        const auto meta = value.toObject();
        quint16 id = 0;
        if (!parseId(meta, &id) || !m_settings.analogIds.contains(id))
            continue;
        const auto reading = m_readings.value(id);
        AnalogPoint point;
        point.id = id;
        point.name = stringField(meta, "name");
        point.unit = stringField(meta, "unit");
        const auto enabled = reading.contains(QStringLiteral("enabled"))
                                 ? reading.value(QStringLiteral("enabled"))
                                 : meta.value(QStringLiteral("enabled"));
        point.active = !enabled.isBool() || enabled.toBool();
        const auto measurementTime = reading.value(QStringLiteral("time"));
        point.timestampMs = timestampMs(measurementTime);
        point.stale = upstreamStale || point.timestampMs <= 0
            || now - point.timestampMs > m_settings.staleAfterMs
            || point.timestampMs - now > kFutureClockToleranceMs;
        const auto raw = reading.value(QStringLiteral("value"));
        float parsed = 0.0f;
        const bool numeric = numberFromValue(raw, &parsed);
        const auto rawStatus = reading.value(QStringLiteral("status"));
        const auto rawSyncing = reading.value(QStringLiteral("syncing"));
        const auto rawAlert = reading.value(QStringLiteral("alert"));
        const bool noAlarm = containsExact(m_settings.quality.noAlarmValues, rawAlert);
        const bool alarm = containsExact(m_settings.quality.alarmValues, rawAlert);
        const QString readingUnit = stringField(reading, "unit");
        const auto expectedUnit = m_settings.quality.expectedUnits.value(QString::number(id));
        const bool matchingUnit = expectedUnit.isString() && !point.unit.isEmpty()
            && expectedUnit.toString() == point.unit
            && readingUnit == point.unit;
        point.valid = numeric && enabled.isBool() && enabled.toBool()
            && matchingUnit && point.timestampMs > 0
            && containsExact(m_settings.quality.validStatuses, rawStatus)
            && containsExact(m_settings.quality.acceptableSyncing, rawSyncing)
            && (noAlarm || alarm);
        point.alarm = point.valid && alarm;
        point.value = numeric ? parsed : 0.0f;
        result.analogs.append(point);
    }
    std::sort(result.analogs.begin(), result.analogs.end(),
              [](const AnalogPoint &a, const AnalogPoint &b) { return a.id < b.id; });
    result.catalogAvailable = m_hasCatalog && !result.analogs.isEmpty();
    return result;
}

QJsonObject OsgSource::debugReadings() const
{
    QJsonArray values;
    const auto current = snapshot();
    int ignoredSensors = 0;
    for (const auto &value : m_catalog) {
        quint16 id = 0;
        if (parseId(value.toObject(), &id) && !m_settings.analogIds.contains(id))
            ++ignoredSensors;
    }
    for (const auto &point : current.analogs) {
        QJsonObject item = m_readings.value(point.id);
        item.insert(QStringLiteral("sensor_id"), point.id);
        item.insert(QStringLiteral("adapter_valid"), point.valid);
        item.insert(QStringLiteral("adapter_stale"), point.stale);
        values.append(item);
    }
    return {{QStringLiteral("upstream_connected"), current.upstreamConnected},
            {QStringLiteral("polling"), current.polling},
            {QStringLiteral("ignored_unmapped_sensors"), ignoredSensors},
            {QStringLiteral("sensors"), values}};
}

} // namespace CentralLogger::Adapters::Osg
