#pragma once

#include <QAbstractNativeEventFilter>

class WindowsFramelessHelper : public QAbstractNativeEventFilter
{
public:
    WindowsFramelessHelper();
    void setTopBarHeight(int height);

protected:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    int m_topBarHeight = 56;
};
