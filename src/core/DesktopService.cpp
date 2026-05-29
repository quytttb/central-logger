#include "DesktopService.h"

#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJSEngine>
#include <QQmlEngine>

namespace CentralLogger::Core {

DesktopService *DesktopService::create(QQmlEngine *engine, QJSEngine *)
{
    static DesktopService *instance = new DesktopService();
    engine->setObjectOwnership(instance, QQmlEngine::CppOwnership);
    return instance;
}

QString DesktopService::reportSavedMessagePrefix()
{
    return QStringLiteral("Report saved: ");
}

QString DesktopService::fileBaseName(const QString &path)
{
    return QFileInfo(path).fileName();
}

bool DesktopService::copyToClipboard(const QString &text)
{
    if (text.isEmpty())
        return false;
    if (QClipboard *cb = QGuiApplication::clipboard())
        cb->setText(text);
    return true;
}

} // namespace CentralLogger::Core
