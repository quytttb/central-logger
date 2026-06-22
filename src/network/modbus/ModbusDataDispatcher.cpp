#include "ModbusDataDispatcher.h"

#include "network/workers/HistoryWriterWorker.h"

namespace CentralLogger::Network {

ModbusDataDispatcher::ModbusDataDispatcher(QObject *parent)
    : QObject(parent)
{
}

void ModbusDataDispatcher::onPollFinished(const PollSnapshot &snapshot)
{
    emit liveSnapshotReady(snapshot);

    if (m_historyWorker) {
        m_historyWorker->enqueue(snapshot);
    }
}

} // namespace CentralLogger::Network
