#include "utils/sensors/AttachDiTypeHelper.h"

#include "utils/SensorConstants.h"

#include <QLatin1String>

namespace CentralLogger::Utils {

namespace {

using CentralLogger::Sensor::kAttachCodeCalibrating;
using CentralLogger::Sensor::kAttachCodeError;
using CentralLogger::Sensor::kAttachCodeMaintenance;
using CentralLogger::Sensor::kAttachCodeMonitoring;

bool isStandardCode(const QString &code) {
  const QString c = AttachDiTypeHelper::normalizeCode(code);
  return c == QLatin1String(kAttachCodeMonitoring)
      || c == QLatin1String(kAttachCodeCalibrating)
      || c == QLatin1String(kAttachCodeError)
      || c == QLatin1String(kAttachCodeMaintenance);
}

QString standardLabel(const QString &code) {
  const QString c = AttachDiTypeHelper::normalizeCode(code);
  if (c == QLatin1String(kAttachCodeMonitoring)) {
    return QStringLiteral("Monitoring");
  }
  if (c == QLatin1String(kAttachCodeCalibrating)) {
    return QStringLiteral("Calibrating");
  }
  if (c == QLatin1String(kAttachCodeError)) {
    return QStringLiteral("Error");
  }
  if (c == QLatin1String(kAttachCodeMaintenance)) {
    return QStringLiteral("Maintenance");
  }
  return c;
}

} // namespace

QString AttachDiTypeHelper::normalizeCode(const QString &code) {
  const QString t = code.trimmed();
  return t.isEmpty() ? QString::fromUtf8(kAttachCodeMonitoring) : t;
}

QString AttachDiTypeHelper::displayLabel(const QString &code,
                                         const QString &catalogDiName) {
  if (isStandardCode(code)) {
    return standardLabel(code);
  }
  const QString name = catalogDiName.trimmed();
  if (!name.isEmpty()) {
    return name;
  }
  return normalizeCode(code);
}

bool AttachDiTypeHelper::isAttachActiveCode(const QString &code) {
  return normalizeCode(code) != QString::fromUtf8(kAttachCodeMonitoring);
}

int AttachDiTypeHelper::sortRank(const QString &code) {
  const QString c = normalizeCode(code);
  if (c == QLatin1String(kAttachCodeError)) {
    return 0;
  }
  if (c == QLatin1String(kAttachCodeMaintenance)) {
    return 1;
  }
  if (c == QLatin1String(kAttachCodeCalibrating)) {
    return 2;
  }
  if (c == QLatin1String(kAttachCodeMonitoring)) {
    return 3;
  }
  return 4;
}

} // namespace CentralLogger::Utils
