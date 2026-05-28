#pragma once

#include <QString>

namespace CentralLogger::Core {

/// Maps edge attach-DI status codes to human-readable chip labels.
class SensorStatusHelper
{
public:
    /// Normalize di_type / status code (trim; empty → "00").
    static QString normalizeDiCode(const QString &code);

    /// Edge SensorBasicTab: 00–03 labels; custom codes returned as-is.
    static QString labelForDiCode(const QString &code);

    /// True when code represents an active attach-DI state (not monitoring/default).
    static bool isActiveDiCode(const QString &code);
};

} // namespace CentralLogger::Core
