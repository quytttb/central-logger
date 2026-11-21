#include "AdapterProcessManager.h"

#include "utils/AdapterConstants.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSet>
#include <QUrl>
#include <QtDebug>

namespace CentralLogger::Core {
namespace {
struct Definition {
    QString name;
    QStringList arguments;
};
}

AdapterProcessManager::AdapterProcessManager(QObject *parent) : QObject(parent) {}
AdapterProcessManager::~AdapterProcessManager() { stop(); }

bool AdapterProcessManager::startFromFile(const QString &path, const QString &defaultProgram,
                                          QString *error)
{
    auto fail = [error](const QString &message) {
        if (error) *error = message;
        return false;
    };
    if (!m_processes.isEmpty())
        return fail(QStringLiteral("Adapters were already started."));
    QFile file(path);
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly) || file.size() > AdapterDefaults::kMaxSecretFileBytes)
        return fail(QStringLiteral("Cannot read adapter configuration."));
    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject() || !doc.object().value(QStringLiteral("instances")).isArray())
        return fail(QStringLiteral("Adapter configuration must contain an instances array."));
    const auto root = doc.object();
    const QString program = root.value(QStringLiteral("executable")).toString(defaultProgram);
    const QFileInfo programInfo(program);
    if (!programInfo.isAbsolute() || !programInfo.isFile() || !programInfo.isExecutable())
        return fail(QStringLiteral("Adapter executable is missing or not executable."));

    const auto instances = root.value(QStringLiteral("instances")).toArray();
    if (instances.size() > AdapterDefaults::kMaxManagedAdapters)
        return fail(QStringLiteral("Too many adapter instances."));
    QList<Definition> definitions;
    QSet<QString> stations;
    QSet<int> ports;
    for (const auto &value : instances) {
        if (!value.isObject())
            return fail(QStringLiteral("Invalid adapter instance."));
        const auto obj = value.toObject();
        if (!obj.value(QStringLiteral("enabled")).isUndefined()
            && !obj.value(QStringLiteral("enabled")).isBool())
            return fail(QStringLiteral("Adapter enabled must be a boolean."));
        if (obj.value(QStringLiteral("enabled")).isBool()
            && !obj.value(QStringLiteral("enabled")).toBool())
            continue;
        const QString name = obj.value(QStringLiteral("station_code")).toString().trimmed();
        const QString displayName = obj.value(QStringLiteral("station_name")).toString().trimmed();
        const QUrl url(obj.value(QStringLiteral("osg_url")).toString(), QUrl::StrictMode);
        const QString credentials = obj.value(QStringLiteral("credentials_file")).toString();
        const QString token = obj.value(QStringLiteral("adapter_token_file")).toString();
        const QString policy = obj.value(QStringLiteral("quality_policy")).toString();
        const QJsonValue modbus = obj.value(QStringLiteral("modbus_port"));
        const QJsonValue rest = obj.value(QStringLiteral("rest_port"));
        const int modbusPort = modbus.toInt(-1);
        const int restPort = rest.toInt(-1);
        if (name.isEmpty() || displayName.isEmpty() || stations.contains(name)
            || !url.isValid() || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))
            || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment()
            || (!url.path().isEmpty() && url.path() != QLatin1String("/"))
            || !QFileInfo(credentials).isAbsolute() || !QFileInfo(token).isAbsolute()
            || !QFileInfo(policy).isAbsolute()
            || !modbus.isDouble() || !rest.isDouble()
            || modbus.toDouble() != modbusPort || rest.toDouble() != restPort
            || modbusPort < 1024 || modbusPort > 65535 || restPort < 1024 || restPort > 65535
            || modbusPort == restPort || ports.contains(modbusPort) || ports.contains(restPort))
            return fail(QStringLiteral("Invalid or duplicate adapter station, URL, file path or port."));
        stations.insert(name);
        ports.insert(modbusPort);
        ports.insert(restPort);
        Definition def{name, {QStringLiteral("--osg-url"), url.toString(),
                              QStringLiteral("--credentials-file"), credentials,
                              QStringLiteral("--adapter-token-file"), token,
                              QStringLiteral("--quality-policy"), policy,
                              QStringLiteral("--station-code"), name,
                              QStringLiteral("--station-name"), displayName,
                              QStringLiteral("--modbus-port"), QString::number(modbusPort),
                              QStringLiteral("--rest-port"), QString::number(restPort)}};
        const auto analogIds = obj.value(QStringLiteral("analog_ids"));
        if (!analogIds.isUndefined()) {
            if (!analogIds.isString() || analogIds.toString().isEmpty())
                return fail(QStringLiteral("Invalid analog_ids."));
            def.arguments << QStringLiteral("--analog-ids") << analogIds.toString();
        }
        definitions.append(std::move(def));
    }
    m_stopping = false;
    for (const auto &def : definitions) {
        auto *process = new QProcess(this);
        process->setProcessChannelMode(QProcess::ForwardedChannels);
        connect(process, &QProcess::errorOccurred, this, [name = def.name](QProcess::ProcessError) {
            qWarning() << "OSG adapter process error for station" << name;
        });
        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                this, [this, name = def.name](int exitCode, QProcess::ExitStatus) {
            if (!m_stopping)
                qWarning() << "OSG adapter exited for station" << name << "code" << exitCode;
        });
        m_processes.append(process);
        process->start(program, def.arguments);
    }
    return true;
}

void AdapterProcessManager::stop()
{
    m_stopping = true;
    for (auto *process : std::as_const(m_processes)) {
        if (process->state() != QProcess::NotRunning)
            process->terminate();
    }
    for (auto *process : std::as_const(m_processes)) {
        if (process->state() != QProcess::NotRunning
            && !process->waitForFinished(AdapterDefaults::kManagedAdapterStopTimeoutMs)) {
            process->kill();
            process->waitForFinished(AdapterDefaults::kManagedAdapterStopTimeoutMs);
        }
        delete process;
    }
    m_processes.clear();
}
} // namespace CentralLogger::Core
