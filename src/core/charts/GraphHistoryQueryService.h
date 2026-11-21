#pragma once

#include "Downsampler.h"

#include <QString>
#include <atomic>

namespace CentralLogger::Core {

struct GraphHistoryResult {
    GraphSeries series;
    QString error;
};

/// Synchronous worker function: caller runs it off the GUI thread and discards
/// superseded results. Only active ANALOG readings for this logger are streamed.
class GraphHistoryQueryService {
public:
    static GraphHistoryResult query(const QString &databasePath, qint64 loggerId,
                                    qint64 fromMs, qint64 toMs, const std::atomic_bool &cancelled);
};

} // namespace CentralLogger::Core
