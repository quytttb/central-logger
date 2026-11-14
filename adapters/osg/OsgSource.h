#pragma once

#include "common/EdgeSource.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QUrl>

namespace CentralLogger::Adapters::Osg {

// All lists default to empty. An operator may populate them only after OSG has
// confirmed the precise meanings for this device/firmware.
struct VerifiedQualityPolicy
{
    QJsonArray validStatuses;
    QJsonArray acceptableSyncing;
    QJsonArray noAlarmValues;
    QJsonArray alarmValues;
    QJsonObject expectedUnits;
};

struct Credentials
{
    QString username;
    QString password;
};

struct Settings
{
    QUrl url;
    int pollIntervalMs = 0;
    int timeoutMs = 0;
    int staleAfterMs = 0;
    Credentials credentials;
    VerifiedQualityPolicy quality;
    QSet<quint16> analogIds;
};

class OsgSource final : public QObject, public EdgeSource
{
    Q_OBJECT

public:
    explicit OsgSource(Settings settings, QObject *parent = nullptr);
    void start();
    void stop();

    EdgeSnapshot snapshot() const override;
    QJsonObject debugReadings() const override;

    // Parsers are also usable by deterministic local tests; no upstream
    // request is made by these functions.
    static bool parseArrayResponse(const QByteArray &body, QJsonArray *out);
    static bool parseId(const QJsonObject &object, quint16 *id);
    static bool parseNumericValue(const QJsonValue &value, float *out);
    static qint64 parseTimestampMs(const QJsonValue &value);

signals:
    void sourceStateChanged();

private:
    enum class Step { Login, Catalog, Realtime };
    void startCycle();
    void login();
    void fetchCatalog();
    void fetchRealtime();
    void issue(Step step);
    void complete(Step step, int httpStatus, const QByteArray &body, bool transportOk);
    void fail(const char *reason);
    void schedule(int delayMs);
    void applyData(const QJsonArray &catalog, const QJsonArray &readings);

    Settings m_settings;
    QNetworkAccessManager m_network;
    QTimer m_timer;
    QNetworkReply *m_reply = nullptr;
    QString m_token;
    bool m_running = false;
    bool m_online = false;
    bool m_hasCatalog = false;
    bool m_reauthenticated = false;
    bool m_pendingCatalogRefreshed = false;
    int m_failures = 0;
    QJsonArray m_pendingCatalog;
    QJsonArray m_catalog;
    QHash<quint16, QJsonObject> m_readings;
    qint64 m_catalogFetchedAtMs = 0;
    qint64 m_lastSuccessMs = 0;
};

} // namespace CentralLogger::Adapters::Osg
