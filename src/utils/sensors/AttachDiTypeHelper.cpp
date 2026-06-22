#include "utils/sensors/AttachDiTypeHelper.h"

namespace CentralLogger::Utils {

namespace {

bool isStandardCode(const QString &code) {
  const QString c = AttachDiTypeHelper::normalizeCode(code);
  return c == QStringLiteral("00") || c == QStringLiteral("01") ||
         c == QStringLiteral("02") || c == QStringLiteral("03");
}

QString standardLabel(const QString &code) {
  const QString c = AttachDiTypeHelper::normalizeCode(code);
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

} // namespace

QString AttachDiTypeHelper::normalizeCode(const QString &code) {
  const QString t = code.trimmed();
  return t.isEmpty() ? QStringLiteral("00") : t;
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
  return normalizeCode(code) != QStringLiteral("00");
}

int AttachDiTypeHelper::sortRank(const QString &code) {
  const QString c = normalizeCode(code);
  if (c == QStringLiteral("02")) {
    return 0;
  }
  if (c == QStringLiteral("03")) {
    return 1;
  }
  if (c == QStringLiteral("01")) {
    return 2;
  }
  if (c == QStringLiteral("00")) {
    return 3;
  }
  return 4;
}

} // namespace CentralLogger::Utils
