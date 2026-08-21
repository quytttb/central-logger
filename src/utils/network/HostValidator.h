#pragma once

#include <QString>

namespace CentralLogger::Utils {

/// Validates logger `host` values per docs/thiet_ke_db.md.
/// Accepts:
///   - IPv4 literals (e.g. "192.168.1.10")
///   - IPv6 literals without zone (e.g. "::1", "2001:db8::1") — bracketed
///     notation is rejected, callers must pass the bare address.
///   - RFC 1123 hostnames (labels 1–63 chars, total ≤ 253 chars).
class HostValidator {
public:
  /// True when @p host is a valid IPv4/IPv6 address or RFC 1123 hostname.
  static bool isValidHost(const QString &host);

  static bool isValidIpv4(const QString &host);
  static bool isValidHostname(const QString &host);

private:
  static bool looksLikeIpv4Literal(const QString &host);
};

} // namespace CentralLogger::Utils
