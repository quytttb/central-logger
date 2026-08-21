#pragma once

#include <QAbstractNativeEventFilter>

class WindowsFramelessHelper : public QAbstractNativeEventFilter
{
public:
    WindowsFramelessHelper();

protected:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
};
