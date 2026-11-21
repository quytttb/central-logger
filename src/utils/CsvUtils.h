#pragma once

#include <QFile>
#include <QLatin1Char>
#include <QString>
#include <QStringConverter>
#include <QTextStream>

#include <concepts>

namespace CentralLogger::Utils {

/// Escape a CSV field using minimal RFC 4180 quoting.
inline QString csvEscape(const QString &s)
{
    if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"'))
        || s.contains(QLatin1Char('\n')) || s.contains(QLatin1Char('\r'))) {
        return QStringLiteral("\"%1\"")
            .arg(QString(s).replace(QLatin1Char('"'), QStringLiteral("\"\"")));
    }
    return s;
}

/// Write a UTF-8 file prefixed with the UTF-8 BOM via a custom stream callback.
template <typename WriterFn>
    requires std::invocable<WriterFn &, QTextStream &>
inline bool writeUtf8BomFile(const QString &path, WriterFn &&writeBody,
                             QString *errorOut = nullptr)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Cannot open file: %1").arg(file.errorString());
        }
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true);
    writeBody(out);
    out.flush();
    file.close();
    if (file.error() != QFileDevice::NoError) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to write file: %1").arg(file.errorString());
        }
        return false;
    }
    return true;
}

/// Write pre-built UTF-8 content prefixed with the UTF-8 BOM.
inline bool writeUtf8BomFile(const QString &path, const QString &content,
                             QString *errorOut = nullptr)
{
    return writeUtf8BomFile(
        path,
        [&content](QTextStream &out) { out << content; },
        errorOut);
}

} // namespace CentralLogger::Utils
