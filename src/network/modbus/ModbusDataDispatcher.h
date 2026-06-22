#pragma once

#include "ModbusTypes.h"

#include <QObject>

namespace CentralLogger::Network {

class HistoryWriterWorker;

/// Receives a single Modbus poll snapshot and fans it out to the live
/// pipeline (main thread / RAM) and the history pipeline (background batch
/// writer). Eliminates duplicate consumers wired directly to ModbusService.
class ModbusDataDispatcher : public QObject
{
    Q_OBJECT

public:
    explicit ModbusDataDispatcher(QObject *parent = nullptr);

    void setHistoryWriter(HistoryWriterWorker *worker) { m_historyWorker = worker; }

public slots:
    void onPollFinished(const CentralLogger::Network::PollSnapshot &snapshot);

signals:
    /// Main-thread live path: status, catalog sync, UI cache refresh.
    void liveSnapshotReady(const CentralLogger::Network::PollSnapshot &snapshot);

private:
    HistoryWriterWorker *m_historyWorker = nullptr;
};

} // namespace CentralLogger::Network
