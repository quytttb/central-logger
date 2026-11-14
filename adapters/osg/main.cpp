#include "OsgSource.h"

#include "common/ModbusV1Server.h"
#include "common/RestV1Server.h"
#include "utils/AdapterConstants.h"
#include "utils/AppConstants.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonDocument>
#include <QModbusDataUnit>
#include <QModbusDevice>
#include <QTextStream>

using namespace CentralLogger::Adapters;
using namespace CentralLogger::Adapters::Osg;
using namespace CentralLogger::AdapterDefaults;

namespace {
bool readPrivateFile(const QString &path, QByteArray *out)
{
    const QFileInfo info(path);
    if (!info.isFile() || info.size() <= 0 || info.size() > kMaxSecretFileBytes)
        return false;
#ifdef Q_OS_UNIX
    constexpr auto others = QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
                          | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;
    if (info.permissions() & others)
        return false;
#endif
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    *out = file.read(kMaxSecretFileBytes + 1);
    return out->size() <= kMaxSecretFileBytes;
}

bool parsePositive(const QString &text, int min, int max, int *out)
{
    bool ok = false;
    const int value = text.toInt(&ok);
    if (!ok || value < min || value > max)
        return false;
    *out = value;
    return true;
}

bool readPolicy(const QString &path, VerifiedQualityPolicy *policy)
{
    if (path.isEmpty())
        return true;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > kMaxSecretFileBytes)
        return false;
    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return false;
    const auto object = doc.object();
    auto readList = [&object](const char *name, QJsonArray *out) {
        const auto value = object.value(QLatin1String(name));
        if (!value.isArray())
            return false;
        *out = value.toArray();
        for (const auto &item : *out) {
            if (item.isArray() || item.isObject() || item.isUndefined())
                return false;
        }
        return true;
    };
    if (!readList("valid_statuses", &policy->validStatuses)
        || !readList("acceptable_syncing", &policy->acceptableSyncing)
        || !readList("no_alarm_values", &policy->noAlarmValues)
        || !readList("alarm_values", &policy->alarmValues)
        || !object.value(QStringLiteral("expected_units")).isObject())
        return false;
    policy->expectedUnits = object.value(QStringLiteral("expected_units")).toObject();
    for (auto it = policy->expectedUnits.begin(); it != policy->expectedUnits.end(); ++it) {
        bool ok = false;
        const uint id = it.key().toUInt(&ok);
        if (!ok || id > 65535 || QString::number(id) != it.key()
            || !it.value().isString() || it.value().toString().isEmpty())
            return false;
    }
    for (const auto &a : policy->alarmValues) {
        if (policy->noAlarmValues.contains(a))
            return false;
    }
    return true;
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("osg_adapter"));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "Read-only OSG REST to Central Logger v1 adapter. Runs only when launched."));
    parser.addHelpOption();
    parser.addOptions({
        {QStringLiteral("osg-url"), QStringLiteral("OSG base URL (no credentials or path)."), QStringLiteral("url")},
        {QStringLiteral("credentials-file"), QStringLiteral("Private JSON file with username and password."), QStringLiteral("path")},
        {QStringLiteral("adapter-token-file"), QStringLiteral("Private file containing the Central-side bearer token."), QStringLiteral("path")},
        {QStringLiteral("quality-policy"), QStringLiteral("Verified OSG quality mappings JSON file (optional)."), QStringLiteral("path")},
        {QStringLiteral("station-code"), QStringLiteral("Unique virtual station code."), QStringLiteral("code")},
        {QStringLiteral("station-name"), QStringLiteral("Virtual station name."), QStringLiteral("name")},
        {QStringLiteral("bind"), QStringLiteral("Bind IP address (loopback by default)."), QStringLiteral("ip"), QStringLiteral("127.0.0.1")},
        {QStringLiteral("allow-remote"), QStringLiteral("Explicitly allow non-loopback binding; LAN only, no TLS.")},
        {QStringLiteral("modbus-port"), QStringLiteral("Adapter Modbus TCP port."), QStringLiteral("port"), QString::number(kModbusPort)},
        {QStringLiteral("rest-port"), QStringLiteral("Adapter REST port."), QStringLiteral("port"), QString::number(kRestPort)},
        {QStringLiteral("unit-id"), QStringLiteral("Adapter Modbus unit ID."), QStringLiteral("id"), QString::number(kUnitId)},
        {QStringLiteral("analog-ids"), QStringLiteral("Comma-separated OSG IDs verified as analog."), QStringLiteral("ids"), QString::fromLatin1(kObservedOsgAnalogIds)},
        {QStringLiteral("poll-ms"), QStringLiteral("Upstream poll interval in milliseconds."), QStringLiteral("ms"), QString::number(kPollIntervalMs)},
        {QStringLiteral("timeout-ms"), QStringLiteral("Upstream request deadline in milliseconds."), QStringLiteral("ms"), QString::number(kRequestTimeoutMs)},
        {QStringLiteral("stale-ms"), QStringLiteral("Maximum source data age in milliseconds."), QStringLiteral("ms"), QString::number(kStaleAfterMs)}
    });
    parser.process(app);
    QTextStream err(stderr);

    const QUrl url(parser.value(QStringLiteral("osg-url")), QUrl::StrictMode);
    if (!url.isValid() || (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https"))
        || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment()
        || (!url.path().isEmpty() && url.path() != QStringLiteral("/"))) {
        err << "Provide --osg-url as an http(s) base URL without user info, path or query.\n";
        return 2;
    }
    const QHostAddress bind(parser.value(QStringLiteral("bind")));
    if (bind.isNull() || (!bind.isLoopback() && !parser.isSet(QStringLiteral("allow-remote")))) {
        err << "Non-loopback bind requires an IP address and explicit --allow-remote.\n";
        return 2;
    }
    if (!bind.isLoopback())
        err << "Warning: LAN clients can access Modbus without authentication; never expose it to the Internet.\n";
    if (url.scheme() == QStringLiteral("http"))
        err << "Warning: OSG credentials, token and data travel over unencrypted HTTP.\n";

    int modbusPort, restPort, unitId, pollMs, timeoutMs, staleMs;
    QSet<quint16> analogIds;
    for (const auto &part : parser.value(QStringLiteral("analog-ids")).split(QLatin1Char(','))) {
        int id = 0;
        if (!parsePositive(part.trimmed(), 0, 65535, &id)
            || analogIds.contains(static_cast<quint16>(id))) {
            err << "Analog IDs must be distinct integers in the uint16 range.\n";
            return 2;
        }
        analogIds.insert(static_cast<quint16>(id));
    }
    if (!parsePositive(parser.value(QStringLiteral("modbus-port")), 1024, 65535, &modbusPort)
        || !parsePositive(parser.value(QStringLiteral("rest-port")), 1024, 65535, &restPort)
        || modbusPort == restPort
        || !parsePositive(parser.value(QStringLiteral("unit-id")), 1, 247, &unitId)
        || !parsePositive(parser.value(QStringLiteral("poll-ms")), kMinPollMs, kMaxPollMs, &pollMs)
        || !parsePositive(parser.value(QStringLiteral("timeout-ms")), kMinTimeoutMs, kMaxTimeoutMs, &timeoutMs)
        || !parsePositive(parser.value(QStringLiteral("stale-ms")), kMinStaleMs, kMaxStaleMs, &staleMs)
        || parser.value(QStringLiteral("station-code")).trimmed().isEmpty()
        || parser.value(QStringLiteral("station-name")).trimmed().isEmpty()) {
        err << "Invalid ports, intervals, unit ID, station code or station name.\n";
        return 2;
    }

    QByteArray credentialBytes, adapterToken;
    if (!readPrivateFile(parser.value(QStringLiteral("credentials-file")), &credentialBytes)
        || !readPrivateFile(parser.value(QStringLiteral("adapter-token-file")), &adapterToken)) {
        err << "Credential and adapter token files must exist and be private (mode 0600 on Unix).\n";
        return 2;
    }
    const auto credentialsDoc = QJsonDocument::fromJson(credentialBytes);
    credentialBytes.fill('\0');
    const auto credentialObject = credentialsDoc.object();
    Credentials credentials{credentialObject.value(QStringLiteral("username")).toString(),
                            credentialObject.value(QStringLiteral("password")).toString()};
    adapterToken = adapterToken.trimmed();
    if (!credentialsDoc.isObject() || credentials.username.isEmpty() || credentials.password.isEmpty()
        || adapterToken.size() < 16 || adapterToken.contains(' ') || adapterToken.contains('\n')
        || adapterToken.contains('\r')) {
        err << "Credentials JSON or adapter token is invalid (token needs at least 16 non-space characters).\n";
        return 2;
    }
    VerifiedQualityPolicy quality;
    if (!readPolicy(parser.value(QStringLiteral("quality-policy")), &quality)) {
        err << "Invalid quality policy; use only independently verified status, syncing and alert mappings.\n";
        return 2;
    }
    if (quality.validStatuses.isEmpty() || quality.acceptableSyncing.isEmpty()
        || (quality.noAlarmValues.isEmpty() && quality.alarmValues.isEmpty())
        || quality.expectedUnits.isEmpty())
        err << "Warning: OSG quality meanings are unverified; values will remain invalid until verified mappings are supplied.\n";

    Settings settings{url, pollMs, timeoutMs, staleMs, std::move(credentials), quality, analogIds};
    OsgSource source(std::move(settings));
    ModbusV1Server modbus(source, kSnapshotLeaseMs);
    modbus.setServerAddress(unitId);
    modbus.setConnectionParameter(QModbusDevice::NetworkAddressParameter, bind.toString());
    modbus.setConnectionParameter(QModbusDevice::NetworkPortParameter, modbusPort);
    QModbusDataUnitMap map;
    map.insert(QModbusDataUnit::HoldingRegisters,
               QModbusDataUnit(QModbusDataUnit::HoldingRegisters, 0, kHeaderRegisters));
    if (!modbus.setMap(map) || !modbus.connectDevice()) {
        err << "Cannot start Modbus server: " << modbus.errorString() << '\n';
        return 1;
    }

    VirtualStation station{parser.value(QStringLiteral("station-code")).trimmed(),
                           parser.value(QStringLiteral("station-name")).trimmed(),
                           qMax(1, pollMs / CentralLogger::Defaults::kMsPerSecond), bind, unitId};
    RestV1Server rest(source, std::move(station), adapterToken);
    adapterToken.fill('\0');
    if (!rest.listen(bind, static_cast<quint16>(restPort))) {
        err << "Cannot start REST server: " << rest.errorString() << '\n';
        return 1;
    }
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &source, &OsgSource::stop);
    source.start();
    err << "OSG adapter started; Modbus port " << modbusPort << ", REST port " << restPort << ".\n";
    return app.exec();
}
