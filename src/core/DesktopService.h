#pragma once

#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

class QJSEngine;
class QQmlEngine;

namespace CentralLogger::Core {

/// Clipboard helper exposed to QML (CentralLogger.Core.DesktopService).
class DesktopService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    static DesktopService *create(QQmlEngine *, QJSEngine *);

    /// Copies @p text to the system clipboard. Returns false when @p text is empty.
    Q_INVOKABLE static bool copyToClipboard(const QString &text);

    /// Prefix for `system_event.message` rows that store a report file path after this text.
    Q_INVOKABLE static QString reportSavedMessagePrefix();

    Q_INVOKABLE static QString fileBaseName(const QString &path);

private:
    explicit DesktopService(QObject *parent = nullptr) : QObject(parent) {}
};

} // namespace CentralLogger::Core
