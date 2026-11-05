#include "Pch.h"

#include "DxuiDisplay.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDisplay::SystemDisplay
//
//  The desktop's, each method its Win32 call.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDisplay::SystemDisplay : public IDxuiDisplay
{
public:
    HMONITOR  MonitorFromPoint           (POINT pointPx, DWORD flags) override                          { return ::MonitorFromPoint (pointPx, flags); }
    HMONITOR  MonitorFromRect            (LPCRECT rectPx, DWORD flags) override                         { return ::MonitorFromRect (rectPx, flags); }
    HMONITOR  MonitorFromWindow          (HWND hwnd, DWORD flags) override                              { return ::MonitorFromWindow (hwnd, flags); }
    BOOL      GetMonitorInfoW            (HMONITOR monitor, LPMONITORINFO info) override                { return ::GetMonitorInfoW (monitor, info); }
    BOOL      EnumDisplayMonitors        (HDC dc, LPCRECT clip, MONITORENUMPROC proc, LPARAM param) override { return ::EnumDisplayMonitors (dc, clip, proc, param); }
    UINT      GetDpiForSystem            () override                                                    { return ::GetDpiForSystem(); }
    UINT      GetDpiForWindow            (HWND hwnd) override                                           { return ::GetDpiForWindow (hwnd); }
    HWND      WindowFromPoint            (POINT pointPx) override                                       { return ::WindowFromPoint (pointPx); }
    BOOL      SystemParametersInfoForDpi (UINT action, UINT param, PVOID pvParam, UINT winIni, UINT dpi) override { return ::SystemParametersInfoForDpi (action, param, pvParam, winIni, dpi); }
    int       GetSystemMetricsForDpi     (int index, UINT dpi) override                                 { return ::GetSystemMetricsForDpi (index, dpi); }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDisplay::Get
//
////////////////////////////////////////////////////////////////////////////////

IDxuiDisplay & DxuiDisplay::Get()
{
    static SystemDisplay  s_system;



    return (s_display != nullptr) ? *s_display : s_system;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDisplay::Set
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDisplay::Set (IDxuiDisplay * display)
{
    s_display = display;
}
