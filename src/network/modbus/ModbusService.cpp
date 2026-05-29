#include "ModbusService.h"

#include "ModbusMapParser.h"

#include <QModbusDataUnit>
#include <QModbusReply>
#include <QModbusTcpClient>
#include <QTimer>
#include <QUrl>
#include <QDateTime>
#include <QtDebug>

namespace CentralLogger::Network {

namespace {

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
    if (!state) {
        state = new LoggerState;
        state->config = config;
        m_states.insert(config.loggerId, state);
    } else {
        const bool reconnect = state->config.host != config.host
                            || state->config.modbusPort != config.modbusPort
                            || state->config.unitId != config.unitId
                            || state->config.timeoutMs != config.timeoutMs;
        state->config = config;
        if (reconnect && state->client) {
            state->client->disconnectDevice();
            state->client->deleteLater();
            state->client = nullptr;
            // C-7 fix: reset in-flight state so the next onPollTimer fires a
            // fresh cycle rather than returning immediately (pollInFlight guard).
            state->pollInFlight = false;
            state->analogAccum.clear();
            state->analogPlan.clear();
        }
    }

    if (!state->timer) {
        state->timer = new QTimer(this);
        state->timer->setSingleShot(false);
        connect(state->timer, &QTimer::timeout, this, &ModbusService::onPollTimer);
        state->timer->setProperty("loggerId", QVariant::fromValue(config.loggerId));
    }
    state->timer->setInterval(config.pollIntervalMs > 0 ? config.pollIntervalMs : 2000);

    if (config.enabled && !m_maintenance) {
        if (!state->timer->isActive()) {
            state->timer->start();
        }
        if (created) {
            // First-time register: poll immediately so the UI doesn't wait
            // a full interval to see status.
            QTimer::singleShot(0, this, [this, id = config.loggerId]() {
                if (auto *s = stateFor(id)) {
                    if (s->config.enabled && !m_maintenance) {
                        startPollCycle(*s);
                    }
                }
            });
        }
    } else {
        state->timer->stop();
    }
}

void ModbusService::unregisterLogger(qint64 loggerId)
{
    destroyState(loggerId);
}

void ModbusService::setMaintenanceMode(bool maintenance)
{
    if (m_maintenance == maintenance) return;
    m_maintenance = maintenance;
    for (auto *state : m_states) {
        if (!state->timer) continue;
        if (m_maintenance) {
            state->timer->stop();
        } else if (state->config.enabled && !state->timer->isActive()) {
            state->timer->start();
        }
    }
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
    state.client->setConnectionParameter(QModbusDevice::NetworkAddressParameter,
                                         state.config.host);
    state.client->setConnectionParameter(QModbusDevice::NetworkPortParameter,
                                         state.config.modbusPort);
    state.client->setTimeout(state.config.timeoutMs);
    state.client->setNumberOfRetries(0);
}

void ModbusService::destroyState(qint64 loggerId)
{
    LoggerState *state = m_states.take(loggerId);
    if (!state) return;

    // M-9: disconnect the transient stateChanged lambda before touching the
    // client. Without this, if the state is torn down while the client is still
    // in ConnectingState the lambda fires after `state` is deleted — reading a
    // dangling pointer via stateFor(id) (now removed from m_states, so null)
    // and leaking the heap-allocated holder.
    if (state->connectHolder) {
        disconnect(*state->connectHolder);
        delete state->connectHolder;
        state->connectHolder = nullptr;
    }

    if (state->timer) {
        state->timer->stop();
        state->timer->deleteLater();
    }
    if (state->client) {
        // Disconnect all remaining signals from this client to ModbusService
        // before scheduling deletion, so no queued signals can reach a freed
        // LoggerState after we `delete state` below.
        state->client->disconnect(this);
        state->client->disconnectDevice();
        state->client->deleteLater();
    }
    delete state;
}

void ModbusService::onPollTimer()
{
    auto *timer = qobject_cast<QTimer *>(sender());
    if (!timer) return;
    const qint64 loggerId = timer->property("loggerId").toLongLong();
    if (auto *state = stateFor(loggerId)) {
        if (!state->config.enabled || m_maintenance) return;
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
        // connectDevice is async; defer the actual read until the state
        // transitions to ConnectedState.
        QMetaObject::Connection *holder = new QMetaObject::Connection;
        // M-9: store the holder in LoggerState so destroyState can clean it up
        // if the state is removed while the connection attempt is still in-flight.
        state.connectHolder = holder;
        *holder = connect(state.client, &QModbusDevice::stateChanged,
                          this, [this, id = state.config.loggerId, holder](QModbusDevice::State newState) {
            if (newState == QModbusDevice::ConnectedState) {
                disconnect(*holder);
                delete holder;
                if (auto *s = stateFor(id)) {
                    s->connectHolder = nullptr;
                    readHeader(*s);
                }
            } else if (newState == QModbusDevice::UnconnectedState) {
                disconnect(*holder);
                delete holder;
                if (auto *s = stateFor(id)) {
                    s->connectHolder = nullptr;
                    finishCycle(*s, false, QStringLiteral("connect failed"));
                }
            }
        });

        // C-2 fix: guard against TCP stalling in ConnectingState indefinitely.
        // After 2× timeoutMs we abort the connection attempt and finish the cycle.
        const int connectTimeoutMs = state.config.timeoutMs > 0
                                     ? state.config.timeoutMs * 2
                                     : 4000;
        QTimer::singleShot(connectTimeoutMs, this, [this, id = state.config.loggerId]() {
            auto *s = stateFor(id);
            if (!s || !s->pollInFlight) return;
            if (s->client
                && s->client->state() == QModbusDevice::ConnectingState) {
                qWarning() << "ModbusService: connect timeout for logger" << id
                           << "— aborting cycle";
                s->client->disconnectDevice();
                finishCycle(*s, false, QStringLiteral("connect timeout"));
            }
        });

        if (!state.client->connectDevice()) {
            disconnect(*holder);
            delete holder;
            state.connectHolder = nullptr;
            finishCycle(state, false, state.client->errorString());
        }
        return;
    }

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
    connect(reply, &QModbusReply::finished, this, [this, id = state.config.loggerId, reply]() {
        reply->deleteLater();
        auto *s = stateFor(id);
        if (!s) return;
        if (reply->error() != QModbusDevice::NoError) {
            finishCycle(*s, false, reply->errorString());
            return;
        }
        const auto regs = regsFromUnit(reply->result());
        s->currentSnapshot.header = ModbusMapParser::parseHeader(regs);
        if (!s->currentSnapshot.header.isValid()) {
            finishCycle(*s, false, QStringLiteral("invalid header (HR0 != 1)"));
            return;
        }
        s->lastHeader = s->currentSnapshot.header;
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
            [this, id = state.config.loggerId, reply, chunkIdx]() {
        reply->deleteLater();
        auto *s = stateFor(id);
        if (!s || !s->pollInFlight) return;
        if (reply->error() != QModbusDevice::NoError) {
            finishCycle(*s, false, reply->errorString());
            return;
        }
        const auto regs = regsFromUnit(reply->result());
        s->analogAccum.append(ModbusMapParser::parseAnalogChunk(regs.constData(), regs.size()));
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
    connect(reply, &QModbusReply::finished, this, [this, id = state.config.loggerId, reply]() {
        reply->deleteLater();
        auto *s = stateFor(id);
        if (!s) return;
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
    connect(reply, &QModbusReply::finished, this, [this, id = state.config.loggerId, reply]() {
        reply->deleteLater();
        auto *s = stateFor(id);
        if (!s) return;
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
    state.pollInFlight = false;
    state.currentSnapshot.success = success;
    state.currentSnapshot.errorMessage = errorMessage;
    if (!success) {
        state.analogAccum.clear();
        state.analogPlan.clear();
    }
    emit pollFinished(state.currentSnapshot);
}

} // namespace CentralLogger::Network
