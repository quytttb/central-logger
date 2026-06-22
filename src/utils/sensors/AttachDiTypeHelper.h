#pragma once

#include <QString>

namespace CentralLogger::Utils {

/// Attach-DI `di_type` codes (00–03 + custom): normalize, labels, sort for
/// merger/UI.
class AttachDiTypeHelper {
public:
  static QString normalizeCode(const QString &code);
  static bool isAttachActiveCode(const QString &code);
  /// Standard 00–03 labels; custom types use catalog DI `name`, else raw code.
  static QString displayLabel(const QString &code,
                              const QString &catalogDiName = {});
  static int sortRank(const QString &code);
};

} // namespace CentralLogger::Utils
