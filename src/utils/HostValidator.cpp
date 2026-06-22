#include "utils/HostValidator.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QRegularExpression>

namespace CentralLogger::Utils {

namespace {

bool isValidHostnameLabel(const QString &label) {
  if (label.isEmpty() || label.size() > 63)
    return false;

  static const QRegularExpression labelRe(
      QStringLiteral("^[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?$"));
  return labelRe.match(label).hasMatch();
}

} // namespace

bool HostValidator::looksLikeIpv4Literal(const QString &host) {
  static const QRegularExpression dottedDigits(QStringLiteral("^[0-9.]+$"));
  return dottedDigits.match(host).hasMatch();
}

bool HostValidator::isValidIpv4(const QString &host) {
  const QString s = host.trimmed();
  if (s.isEmpty())
    return false;

  // Require exactly four dotted decimal octets — QHostAddress accepts
  // shortened forms (e.g. "192.168.1") which we reject for user input.
  static const QRegularExpression ipv4Re(
      QStringLiteral("^(25[0-5]|2[0-4]\\d|1\\d{2}|[1-9]?\\d)"
                     "(\\.(25[0-5]|2[0-4]\\d|1\\d{2}|[1-9]?\\d)){3}$"));
  if (!ipv4Re.match(s).hasMatch())
    return false;

  QHostAddress addr;
  return addr.setAddress(s) &&
         addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isNull();
}

bool HostValidator::isValidHostname(const QString &host) {
  const QString s = host.trimmed();
  if (s.isEmpty() || s.size() > 253)
    return false;

  if (s.contains(QLatin1Char(' ')) || s.contains(QLatin1Char('/')) ||
      s.contains(QLatin1Char(':')) || s.contains(QLatin1Char('@')) ||
      s.startsWith(QLatin1Char('.')) || s.endsWith(QLatin1Char('.')) ||
      s.contains(QStringLiteral(".."))) {
    return false;
  }

  const auto labels = s.split(QLatin1Char('.'), Qt::SkipEmptyParts);
  if (labels.isEmpty())
    return false;

  for (const QString &label : labels) {
    if (!isValidHostnameLabel(label))
      return false;
  }
  return true;
}

bool HostValidator::isValidHost(const QString &host) {
  const QString s = host.trimmed();
  if (s.isEmpty())
    return false;

  // Strings like "192.168.1" are not valid IPv4 but would pass hostname
  // label rules — require strict IPv4 when only digits and dots.
  if (looksLikeIpv4Literal(s))
    return isValidIpv4(s);

  return isValidHostname(s);
}

} // namespace CentralLogger::Utils
