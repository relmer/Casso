#include "Pch.h"

#include "TestSystemState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TestSystemState::Install
//
////////////////////////////////////////////////////////////////////////////////

void TestSystemState::Install()
{
    DxuiInputState::SetKeyReader      ([] (int) { return SHORT (0); });
    DxuiInputState::SetAsyncKeyReader ([] (int) { return SHORT (0); });
    DxuiInputState::SetCursorReader   ([] (POINT * outScreenPx) { *outScreenPx = kPointerPx; return TRUE; });

    DxuiSystemSettings::Instance().SetParameterReader (ReadNoParameter);
    DxuiSystemSettings::Instance().SetMetricReaders   (ReadNoMetric, ReadNoTime, ReadNoTime);
    DxuiSystemSettings::Instance().Refresh();

    DxuiDisplay::Set (&s_display);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TestSystemState::ReadNoParameter
//
//  Every query fails, so each setting is the Windows default.
//
////////////////////////////////////////////////////////////////////////////////

BOOL WINAPI TestSystemState::ReadNoParameter (UINT action, UINT param, PVOID pvParam, UINT winIni)
{
    (void) action;
    (void) param;
    (void) pvParam;
    (void) winIni;

    return FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TestSystemState::ReadNoMetric
//
//  Zero, so each metric is the Windows default.
//
////////////////////////////////////////////////////////////////////////////////

int WINAPI TestSystemState::ReadNoMetric (int index)
{
    (void) index;

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TestSystemState::ReadNoTime
//
//  Zero, so each time is the Windows default.
//
////////////////////////////////////////////////////////////////////////////////

UINT WINAPI TestSystemState::ReadNoTime()
{
    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TestSystemState::Display::MonitorFromPoint
//
//  The monitor, or for a point off it, the nearest, which is the same one.
//
////////////////////////////////////////////////////////////////////////////////

HMONITOR TestSystemState::Display::MonitorFromPoint (POINT pointPx, DWORD flags)
{
    bool  isOn = PtInRect (&kMonitorPx, pointPx) != FALSE;



    return (isOn || flags != MONITOR_DEFAULTTONULL) ? GetMonitor() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TestSystemState::Display::MonitorFromRect
//
////////////////////////////////////////////////////////////////////////////////

HMONITOR TestSystemState::Display::MonitorFromRect (LPCRECT rectPx, DWORD flags)
{
    RECT  overlap = {};
    bool  isOn    = rectPx != nullptr && IntersectRect (&overlap, rectPx, &kMonitorPx) != FALSE;



    return (isOn || flags != MONITOR_DEFAULTTONULL) ? GetMonitor() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TestSystemState::Display::GetMonitorInfoW
//
//  The monitor's rect, work area and device name. For another handle the
//  call fails.
//
////////////////////////////////////////////////////////////////////////////////

BOOL TestSystemState::Display::GetMonitorInfoW (HMONITOR monitor, LPMONITORINFO info)
{
    if (monitor != GetMonitor() || info == nullptr)
    {
        return FALSE;
    }

    info->rcMonitor = kMonitorPx;
    info->rcWork    = kWorkPx;
    info->dwFlags   = MONITORINFOF_PRIMARY;

    if (info->cbSize >= sizeof (MONITORINFOEXW))
    {
        (void) wcscpy_s (reinterpret_cast<MONITORINFOEXW *> (info)->szDevice, L"\\\\.\\TESTDISPLAY1");
    }

    return TRUE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TestSystemState::Display::EnumDisplayMonitors
//
////////////////////////////////////////////////////////////////////////////////

BOOL TestSystemState::Display::EnumDisplayMonitors (HDC, LPCRECT, MONITORENUMPROC proc, LPARAM param)
{
    RECT  monitor = kMonitorPx;



    if (proc != nullptr)
    {
        (void) proc (GetMonitor(), nullptr, &monitor, param);
    }

    return TRUE;
}
