#include "WindowsFramelessHelper.h"
#include <QByteArray>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

WindowsFramelessHelper::WindowsFramelessHelper()
{
}

void WindowsFramelessHelper::setTopBarHeight(int height)
{
    m_topBarHeight = height;
}

bool WindowsFramelessHelper::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (eventType == QByteArray("windows_generic_MSG") || eventType == QByteArray("windows_dispatcher_MSG")) {
        MSG *msg = static_cast<MSG *>(message);
        if (!msg->hwnd) return false;

        switch (msg->message) {
        case WM_NCCALCSIZE: {
            if (msg->wParam == TRUE) {
                // By returning 0, we tell DWM that the client area covers the whole window,
                // removing the title bar. DWM still draws the shadow.
                *result = 0;
                return true;
            }
            break;
        }
        case WM_NCHITTEST: {
            POINT pt;
            pt.x = GET_X_LPARAM(msg->lParam);
            pt.y = GET_Y_LPARAM(msg->lParam);

            RECT rw;
            GetWindowRect(msg->hwnd, &rw);

            // Dynamically load DPI functions to avoid missing symbols on older MinGW
            typedef UINT(WINAPI *GetDpiForWindow_t)(HWND);
            typedef int(WINAPI *GetSystemMetricsForDpi_t)(int, UINT);
            static GetDpiForWindow_t getDpi = (GetDpiForWindow_t)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
            static GetSystemMetricsForDpi_t getMetrics = (GetSystemMetricsForDpi_t)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetSystemMetricsForDpi");

            int frameX = 8;
            int frameY = 8;
            int dpi = 96;

            if (getDpi && getMetrics) {
                dpi = getDpi(msg->hwnd);
                frameX = getMetrics(SM_CXFRAME, dpi) + getMetrics(SM_CXPADDEDBORDER, dpi);
                frameY = getMetrics(SM_CYFRAME, dpi) + getMetrics(SM_CXPADDEDBORDER, dpi);
            } else {
                frameX = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                frameY = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
            }

            bool isLeft = (pt.x >= rw.left && pt.x < rw.left + frameX);
            bool isRight = (pt.x < rw.right && pt.x >= rw.right - frameX);
            bool isTop = (pt.y >= rw.top && pt.y < rw.top + frameY);
            bool isBottom = (pt.y < rw.bottom && pt.y >= rw.bottom - frameY);

            if (isTop && isLeft) { *result = HTTOPLEFT; return true; }
            if (isTop && isRight) { *result = HTTOPRIGHT; return true; }
            if (isBottom && isLeft) { *result = HTBOTTOMLEFT; return true; }
            if (isBottom && isRight) { *result = HTBOTTOMRIGHT; return true; }
            if (isLeft) { *result = HTLEFT; return true; }
            if (isRight) { *result = HTRIGHT; return true; }
            if (isBottom) { *result = HTBOTTOM; return true; }
            if (isTop) { *result = HTTOP; return true; }

            // Normal client area
            *result = HTCLIENT;
            return true;
        }
        }
    }
#endif
    return false;
}
