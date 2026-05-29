#pragma once

#include <QString>

namespace CentralLogger::Core {

/// Validates logger `host` values per docs/thiet_ke_db.md (IPv4 or hostname).
class HostValidator
{
public:
    /// True when @p host is a valid IPv4 address or RFC 1123-style hostname.
    static bool isValidHost(const QString &host);

    static bool isValidIpv4(const QString &host);
    static bool isValidHostname(const QString &host);

private:
    static bool looksLikeIpv4Literal(const QString &host);
};

} // namespace CentralLogger::Core
