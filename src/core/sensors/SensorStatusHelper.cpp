#include "core/sensors/SensorStatusHelper.h"

namespace CentralLogger::Core {

QString SensorStatusHelper::normalizeDiCode(const QString &code)
{
    const QString t = code.trimmed();
    return t.isEmpty() ? QStringLiteral("00") : t;
}

QString SensorStatusHelper::labelForDiCode(const QString &code)
{
    const QString c = normalizeDiCode(code);
    if (c == QStringLiteral("00")) {
        return QStringLiteral("Monitoring");
    }
    if (c == QStringLiteral("01")) {
        return QStringLiteral("Calibrating");
    }
    if (c == QStringLiteral("02")) {
        return QStringLiteral("Error");
    }
    if (c == QStringLiteral("03")) {
        return QStringLiteral("Maintenance");
    }
    return c;
}

bool SensorStatusHelper::isActiveDiCode(const QString &code)
{
    return normalizeDiCode(code) != QStringLiteral("00");
}

} // namespace CentralLogger::Core
