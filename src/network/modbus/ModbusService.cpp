#include "ModbusService.h"

#include "ModbusMapParser.h"
#include "utils/AppConstants.h"

#include <QModbusDataUnit>
#include <QModbusReply>
#include <QModbusTcpClient>
#include <QTimer>
#include <QDateTime>
#include <QtDebug>
#include <utility>

namespace CentralLogger::Network {

namespace {

using CentralLogger::Defaults::kConnectTimeoutFallbackMs;
using CentralLogger::Defaults::kConnectTimeoutMultiplier;

// kConnectTimeoutMultiplier / kConnectTimeoutFallbackMs live in
// CentralLogger::Defaults (utils/AppConstants.h). See C-2 fix.

QVector<quint16> regsFromUnit(const QModbusDataUnit &unit)
{
    QVector<quint16> out;
    out.reserve(static_cast<int>(unit.valueCount()));
    for (uint i = 0; i < unit.valueCount(); ++i) {
        out.append(unit.value(i));
    }
    return out;
}

} // namespace

ModbusService::ModbusService(QObject *parent)
    : QObject(parent)
{
}

ModbusService::~ModbusService()
{
    shutdown();
}

void ModbusService::syncLoggers(const QVector<LoggerRuntimeConfig> &configs)
{
    QHash<qint64, LoggerRuntimeConfig> next;
    for (const auto &c : configs) {
        next.insert(c.loggerId, c);
    }

    // Drop loggers that disappeared from the new set.
    const auto existingIds = m_states.keys();
    for (qint64 id : existingIds) {
        if (!next.contains(id)) {
            destroyState(id);
        }
    }

    // Add or refresh the rest.
    for (auto it = next.constBegin(); it != next.constEnd(); ++it) {
        registerLogger(it.value());
    }
}

void ModbusService::registerLogger(const LoggerRuntimeConfig &config)
{
    LoggerState *state = m_states.value(config.loggerId, nullptr);
    const bool created = (state == nullptr);
    const bool wasEnabled = state && state->config.enabled;
    if (created) {
        state = new LoggerState;
        m_states.insert(config.loggerId, state);
    }
    const bool endpointChanged = !created && (state->config.host != config.host
        || state->config.modbusPort != config.modbusPort || state->config.unitId != config.unitId
        || state->config.timeoutMs != config.timeoutMs);
    state->config = config;

    if (created) {
        state->timer = new QTimer(this);
        connect(state->timer, &QTimer::timeout, this, &ModbusService::onPollTimer);
        state->timer->setProperty("loggerId", QVariant::fromValue(config.loggerId));

        state->watchdog = new QTimer(this);
        state->watchdog->setSingleShot(true);
        state->watchdog->setTimerType(Qt::PreciseTimer);
        connect(state->watchdog, &QTimer::timeout, this, [this, id = config.loggerId,
                                                         timer = state->watchdog]() {
            auto *s = stateFor(id);
            if (s && s->watchdog == timer && s->pollInFlight) {
                finishCycle(*s, false, s->awaitingConnection ? QStringLiteral("connect timeout")
                                                            : QStringLiteral("poll cycle timeout"));
            }
        });
    }

    if (endpointChanged || !config.enabled) {
        forceReconnect(*state);
        state->consecutiveFailures = 0;
    }
    const int interval = config.pollIntervalMs > 0 ? config.pollIntervalMs
                                                   : Defaults::kDefaultPollIntervalMs;
    if (state->timer->interval() != interval)
        state->timer->setInterval(interval);

    if (config.enabled) {
        if (!state->timer->isActive()) {
            state->timer->start();
        }
        if (!wasEnabled) {
            // First-time register: poll soon so the UI doesn't wait a full
            // interval to see status. Audit M-4: stagger the first poll per
            // logger (100 ms each) so startup doesn't open every TCP
            // connection at once.
            const int delayMs = created ? m_staggerCounter++ * Defaults::kModbusStartupStaggerMs : 0;
            QTimer::singleShot(delayMs, state->timer, [this, id = config.loggerId, timer = state->timer]() {
                if (auto *s = stateFor(id)) {
                    if (s->timer == timer && s->config.enabled && !s->pollInFlight) {
                        startPollCycle(*s);
                    }
                }
            });
        }
    } else {
        state->timer->stop();
    }
}

void ModbusService::reconnectLogger(qint64 loggerId)
{
    auto *state = stateFor(loggerId);
    if (!state || !state->config.enabled)
        return;
    forceReconnect(*state);
    state->timer->start();
    startPollCycle(*state);
}

void ModbusService::shutdown()
{
    const auto ids = m_states.keys();
    for (qint64 id : ids) {
        destroyState(id);
    }
}

ModbusService::LoggerState *ModbusService::stateFor(qint64 loggerId)
{
    return m_states.value(loggerId, nullptr);
}

void ModbusService::ensureClient(LoggerState &state)
{
    if (state.client) return;
    state.client = new QModbusTcpClient(this);
    state.clientEpoch = ++m_nextClientEpoch;
    state.client->setConnectionParameter(QModbusDevice::NetworkAddressParameter,
                                         state.config.host);
    state.client->setConnectionParameter(QModbusDevice::NetworkPortParameter,
                                         state.config.modbusPort);
    state.client->setTimeout(state.config.timeoutMs > 0 ? state.config.timeoutMs
                                                        : Defaults::kDefaultTimeoutMs);
    state.client->setNumberOfRetries(0);
    const auto id = state.config.loggerId;
    const auto epoch = state.clientEpoch;
    // Queue device signals: connectDevice()/sendReadRequest() can emit an
    // error synchronously, before their caller has handled the return value.
    connect(state.client, &QModbusDevice::stateChanged, this, [this, id, epoch](QModbusDevice::State next) {
        auto *s = stateFor(id);
        if (!s || isStaleReply(*s, epoch))
            return;
        if (next == QModbusDevice::ConnectedState && s->awaitingConnection
            && s->client->state() == QModbusDevice::ConnectedState) {
            s->awaitingConnection = false;
            s->watchdog->start(s->client->timeout());
            readHeader(*s);
        } else if (next == QModbusDevice::UnconnectedState) {
            finishCycle(*s, false, s->client->errorString().isEmpty()
                                     ? QStringLiteral("connection closed") : s->client->errorString());
        }
    }, Qt::QueuedConnection);
    connect(state.client, &QModbusDevice::errorOccurred, this, [this, id, epoch](QModbusDevice::Error error) {
        if (error != QModbusDevice::ConnectionError && error != QModbusDevice::ReadError
            && error != QModbusDevice::WriteError)
            return;
        auto *s = stateFor(id);
        if (s && !isStaleReply(*s, epoch))
            finishCycle(*s, false, s->client->errorString());
    }, Qt::QueuedConnection);
}

void ModbusService::forceReconnect(LoggerState &state)
{
    state.watchdog->stop();
    state.pollInFlight = false;
    state.awaitingConnection = false;
    state.analogAccum.clear();
    state.analogPlan.clear();

    if (auto *client = std::exchange(state.client, nullptr)) {
        client->disconnect(this);
        // Don't close a socket inside its reply/readyRead callback. From this
        // point its replies are ignored and the next poll gets a new client.
        QMetaObject::invokeMethod(client, [client]() { client->disconnectDevice(); },
                                  Qt::QueuedConnection);
        client->deleteLater();
    }
}

bool ModbusService::isStaleReply(const LoggerState &state, quint64 replyEpoch) const
{
    // C-A fix: stale if the state's client has been replaced (epoch advanced)
    // or no cycle is currently in flight (state reset/destroyed mid-request).
    return !state.client || replyEpoch != state.clientEpoch || !state.pollInFlight;
}

void ModbusService::destroyState(qint64 loggerId)
{
    LoggerState *state = m_states.take(loggerId);
    if (!state) return;

    forceReconnect(*state);
    state->timer->stop();
    state->timer->deleteLater();
    state->watchdog->deleteLater();
    delete state;
}

void ModbusService::onPollTimer()
{
    auto *timer = qobject_cast<QTimer *>(sender());
    if (!timer) return;
    const qint64 loggerId = timer->property("loggerId").toLongLong();
    if (auto *state = stateFor(loggerId)) {
        if (state->timer != timer || !state->config.enabled) return;
        if (state->pollInFlight) return;
        startPollCycle(*state);
    }
}

void ModbusService::startPollCycle(LoggerState &state)
{
    ensureClient(state);
    if (!state.client) return;

    state.pollInFlight = true;
    state.currentSnapshot = PollSnapshot{};
    state.currentSnapshot.loggerId   = state.config.loggerId;
    state.currentSnapshot.measuredAt = QDateTime::currentDateTimeUtc();
    state.analogAccum.clear();
    state.analogPlan.clear();

    if (state.client->state() == QModbusDevice::UnconnectedState) {
        state.awaitingConnection = true;
        const int connectTimeoutMs = state.config.timeoutMs > 0
                                      ? state.config.timeoutMs * kConnectTimeoutMultiplier
                                      : kConnectTimeoutFallbackMs;
        state.watchdog->start(connectTimeoutMs);
        if (!state.client->connectDevice())
            finishCycle(state, false, state.client->errorString());
        return;
    }
    if (state.client->state() != QModbusDevice::ConnectedState) {
        finishCycle(state, false, QStringLiteral("connection not ready"));
        return;
    }
    // The read deadline covers all FC03/FC02/FC01 PDUs, not each one separately.
    state.watchdog->start(state.client->timeout());
    readHeader(state);
}

void ModbusService::readHeader(LoggerState &state)
{
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, 0, 10);
    auto *reply = state.client->sendReadRequest(unit, state.config.unitId);
    if (!reply) {
        finishCycle(state, false, state.client->errorString());
        return;
    }
    if (reply->isFinished()) {
        reply->deleteLater();
        finishCycle(state, false, QStringLiteral("header reply finished immediately"));
        return;
    }
    connect(reply, &QModbusReply::finished, this, [this, id = state.config.loggerId,
                                                   epoch = state.clientEpoch, reply]() {
        reply->deleteLater();
        auto *s = stateFor(id);
        if (!s || isStaleReply(*s, epoch)) return;
        if (reply->error() != QModbusDevice::NoError) {
            finishCycle(*s, false, reply->errorString());
            return;
        }
        const auto regs = regsFromUnit(reply->result());
        s->currentSnapshot.header = ModbusMapParser::parseHeader(regs);
        if (!s->currentSnapshot.header.isValid()) {
            finishCycle(*s, false, QStringLiteral("unsupported Modbus map version"));
            return;
        }
        if (s->currentSnapshot.header.upstreamUnavailable()) {
            finishCycle(*s, false, QStringLiteral("adapter upstream unavailable"));
            return;
        }
        readAnalogChunks(*s);
    });
}

void ModbusService::readAnalogChunks(LoggerState &state)
{
    const auto plan = planPollReads(state.currentSnapshot.header.na, 0, 0);
    // plan[0] is the header (already read); collect the analog chunks only.
    state.analogPlan.clear();
    state.analogAccum.clear();
    for (int i = 1; i < plan.size(); ++i) {
        state.analogPlan.append(plan[i]);
    }
    readNextAnalogChunk(state, 0);
}

void ModbusService::readNextAnalogChunk(LoggerState &state, int chunkIdx)
{
    if (chunkIdx >= state.analogPlan.size()) {
        state.currentSnapshot.analogs = state.analogAccum;
        readDiscreteInputs(state);
        return;
    }
    const auto &pdu = state.analogPlan[chunkIdx];
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, pdu.start, pdu.quantity);
    auto *reply = state.client->sendReadRequest(unit, state.config.unitId);
    if (!reply) {
        finishCycle(state, false, state.client->errorString());
        return;
    }
    if (reply->isFinished()) {
        reply->deleteLater();
        finishCycle(state, false, QStringLiteral("analog reply finished immediately"));
        return;
    }
    connect(reply, &QModbusReply::finished, this,
            [this, id = state.config.loggerId, epoch = state.clientEpoch, reply, chunkIdx]() {
        reply->deleteLater();
        auto *s = stateFor(id);
        if (!s || isStaleReply(*s, epoch)) return;
        if (reply->error() != QModbusDevice::NoError) {
            finishCycle(*s, false, reply->errorString());
            return;
        }
        const auto regs = regsFromUnit(reply->result());
        s->analogAccum.append(ModbusMapParser::parseAnalogChunk(
            regs.constData(), regs.size(), s->currentSnapshot.header.mapVersion));
        readNextAnalogChunk(*s, chunkIdx + 1);
    });
}

void ModbusService::readDiscreteInputs(LoggerState &state)
{
    const quint16 ndi = state.currentSnapshot.header.ndi;
    if (ndi == 0) {
        readCoils(state);
        return;
    }
    QModbusDataUnit unit(QModbusDataUnit::DiscreteInputs, 0, ndi);
    auto *reply = state.client->sendReadRequest(unit, state.config.unitId);
    if (!reply) {
        finishCycle(state, false, state.client->errorString());
        return;
    }
    if (reply->isFinished()) {
        reply->deleteLater();
        finishCycle(state, false, QStringLiteral("DI reply finished immediately"));
        return;
    }
    connect(reply, &QModbusReply::finished, this, [this, id = state.config.loggerId,
                                                   epoch = state.clientEpoch, reply]() {
        reply->deleteLater();
        auto *s = stateFor(id);
        if (!s || isStaleReply(*s, epoch)) return;
        if (reply->error() != QModbusDevice::NoError) {
            finishCycle(*s, false, reply->errorString());
            return;
        }
        const auto diRegs = regsFromUnit(reply->result());
        const int bitCount = static_cast<int>(s->currentSnapshot.header.ndi);
        s->currentSnapshot.diBits = ModbusMapParser::unpackDiscrete(diRegs, bitCount);
        readCoils(*s);
    });
}

void ModbusService::readCoils(LoggerState &state)
{
    const quint16 ndo = state.currentSnapshot.header.ndo;
    if (ndo == 0) {
        finishCycle(state, true);
        return;
    }
    QModbusDataUnit unit(QModbusDataUnit::Coils, 0, ndo);
    auto *reply = state.client->sendReadRequest(unit, state.config.unitId);
    if (!reply) {
        finishCycle(state, false, state.client->errorString());
        return;
    }
    if (reply->isFinished()) {
        reply->deleteLater();
        finishCycle(state, false, QStringLiteral("DO reply finished immediately"));
        return;
    }
    connect(reply, &QModbusReply::finished, this, [this, id = state.config.loggerId,
                                                   epoch = state.clientEpoch, reply]() {
        reply->deleteLater();
        auto *s = stateFor(id);
        if (!s || isStaleReply(*s, epoch)) return;
        if (reply->error() != QModbusDevice::NoError) {
            finishCycle(*s, false, reply->errorString());
            return;
        }
        const auto doRegs = regsFromUnit(reply->result());
        const int bitCount = static_cast<int>(s->currentSnapshot.header.ndo);
        s->currentSnapshot.doBits = ModbusMapParser::unpackDiscrete(doRegs, bitCount);
        finishCycle(*s, true);
    });
}

void ModbusService::finishCycle(LoggerState &state, bool success, const QString &errorMessage)
{
    if (!state.pollInFlight)
        return;
    state.currentSnapshot.success = success;
    state.currentSnapshot.errorMessage = errorMessage;
    if (!success) {
        if (++state.consecutiveFailures == 1) {
            qWarning() << "Modbus poll failed for logger" << state.config.loggerId
                       << "client state" << state.client->state() << "epoch" << state.clientEpoch
                       << errorMessage << "— reopening TCP on the next poll";
        }
        forceReconnect(state);
    } else {
        state.watchdog->stop();
        state.pollInFlight = false;
        if (state.consecutiveFailures > 0) {
            qInfo() << "Modbus recovered for logger" << state.config.loggerId
                    << "after" << state.consecutiveFailures << "failed polls";
            state.consecutiveFailures = 0;
        }
    }
    emit pollFinished(state.currentSnapshot);
}

} // namespace CentralLogger::Network
