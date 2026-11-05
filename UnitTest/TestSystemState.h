#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TestSystemState
//
//  The desktop state a unit test reads in place of this machine's: every key
//  up and none toggled, the pointer far from every window, the Windows
//  defaults for the interaction settings and the menu metrics, and one
//  monitor at 96 DPI with no window on it. Install puts it in place, once
//  for the module, and again after a test that set another reader.
//
////////////////////////////////////////////////////////////////////////////////

class TestSystemState
{
public:
    static constexpr POINT  kPointerPx = { -30000, -30000 };
    static constexpr RECT   kMonitorPx = { 0, 0, 1920, 1080 };
    static constexpr RECT   kWorkPx    = { 0, 0, 1920, 1040 };
    static constexpr UINT   kDpi       = USER_DEFAULT_SCREEN_DPI;

    static void  Install ();

private:
    //  The one monitor, whose handle is a value and no monitor of the
    //  system's.
    class Display : public IDxuiDisplay
    {
    public:
        HMONITOR  MonitorFromPoint           (POINT pointPx, DWORD flags) override;
        HMONITOR  MonitorFromRect            (LPCRECT rectPx, DWORD flags) override;
        HMONITOR  MonitorFromWindow          (HWND, DWORD) override                                  { return GetMonitor(); }
        BOOL      GetMonitorInfoW            (HMONITOR monitor, LPMONITORINFO info) override;
        BOOL      EnumDisplayMonitors        (HDC, LPCRECT, MONITORENUMPROC proc, LPARAM param) override;
        UINT      GetDpiForSystem            () override                                             { return kDpi; }
        UINT      GetDpiForWindow            (HWND) override                                         { return kDpi; }
        HWND      WindowFromPoint            (POINT) override                                        { return nullptr; }
        BOOL      SystemParametersInfoForDpi (UINT, UINT, PVOID, UINT, UINT) override                { return FALSE; }
        int       GetSystemMetricsForDpi     (int, UINT) override                                    { return 0; }

        static HMONITOR  GetMonitor ()  { return reinterpret_cast<HMONITOR> (static_cast<uintptr_t> (0x1)); }
    };

    static inline Display  s_display;

    static BOOL WINAPI  ReadNoParameter (UINT action, UINT param, PVOID pvParam, UINT winIni);
    static int  WINAPI  ReadNoMetric    (int index);
    static UINT WINAPI  ReadNoTime      ();
};
