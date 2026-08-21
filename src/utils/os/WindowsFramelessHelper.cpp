#include "WindowsFramelessHelper.h"
#include <QByteArray>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

WindowsFramelessHelper::WindowsFramelessHelper()
{
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

            // When the window is maximized the OS adds a hidden resize border
            // on every side so a maximized window can be dragged back; ignore it
            // so our hit zones stay inside the actual screen rect.
            if (IsZoomed(msg->hwnd)) {
                return false;
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
        case WM_GETMINMAXINFO: {
            // Without this hook, a frameless window that responds to
            // WM_NCCALCSIZE by collapsing the non-client area would, when
            // maximized, cover the taskbar / spill off-screen by the hidden
            // border width. Constrain the maximized tracking size and position
            // to the monitor's work area so the window snaps to it.
            MINMAXINFO *mmi = reinterpret_cast<MINMAXINFO *>(msg->lParam);
            HMONITOR mon = MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST);
            if (mon) {
                MONITORINFO mi;
                mi.cbSize = sizeof(MONITORINFO);
                if (GetMonitorInfoW(mon, &mi)) {
                    mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
                    mmi->ptMaxPosition.y = mi.rcWork.top  - mi.rcMonitor.top;
                    mmi->ptMaxSize.x     = mi.rcWork.right  - mi.rcWork.left;
                    mmi->ptMaxSize.y     = mi.rcWork.bottom - mi.rcWork.top;
                    mmi->ptMaxTrackSize.x = mmi->ptMaxSize.x;
                    mmi->ptMaxTrackSize.y = mmi->ptMaxSize.y;
                }
            }
            *result = 0;
            return true;
        }
        }
    }
#endif
    return false;
}
