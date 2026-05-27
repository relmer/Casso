#include "Pch.h"

#include "WindowManager.h"

#include "../Config/WindowPlacementProfile.h"
#include "../Config/Win32RegistrySettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    Win32RegistrySettings  s_win32Reg;
    WindowPlacementProfile s_profile (s_win32Reg);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildPlacementSubkeyForMonitor
//
//  Thin compatibility shim. Delegates to WindowPlacementProfile so the
//  topology-hashing logic has exactly one home.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WindowManager::BuildPlacementSubkeyForMonitor (HMONITOR activeMonitor)
{
    return WindowPlacementProfile::BuildTopologySubkey (activeMonitor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryLoadSavedWindowPlacement
//
////////////////////////////////////////////////////////////////////////////////

bool WindowManager::TryLoadSavedWindowPlacement (
    HMONITOR  activeMonitor,
    LONG    & outX,
    LONG    & outY,
    int     & outW,
    int     & outH) const
{
    std::wstring                     subkey;
    WindowPlacementProfile::Bounds   bounds;
    RECT                             wr     = {};
    HMONITOR                         hMon   = nullptr;



    subkey = WindowPlacementProfile::BuildTopologySubkey (activeMonitor);

    if (!s_profile.TryLoad (subkey, bounds))
    {
        return false;
    }

    wr.left   = bounds.x;
    wr.top    = bounds.y;
    wr.right  = bounds.x + bounds.w;
    wr.bottom = bounds.y + bounds.h;

    hMon = MonitorFromRect (&wr, MONITOR_DEFAULTTONULL);
    if (hMon == nullptr)
    {
        return false;
    }

    outX = bounds.x;
    outY = bounds.y;
    outW = bounds.w;
    outH = bounds.h;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveWindowPlacement
//
////////////////////////////////////////////////////////////////////////////////

void WindowManager::SaveWindowPlacement (HWND hwnd, bool fullscreen)
{
    HMONITOR                         hMon    = nullptr;
    std::wstring                     subkey;
    RECT                             wr      = {};
    int                              width   = 0;
    int                              height  = 0;
    HRESULT                          hr      = S_OK;
    WindowPlacementProfile::Bounds   bounds;



    if (hwnd == nullptr)
    {
        return;
    }

    if (IsIconic (hwnd) || IsZoomed (hwnd) || fullscreen)
    {
        return;
    }

    if (!GetWindowRect (hwnd, &wr))
    {
        return;
    }

    width  = wr.right - wr.left;
    height = wr.bottom - wr.top;

    if (width <= 0 || height <= 0)
    {
        return;
    }

    hMon = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);
    if (hMon == nullptr)
    {
        return;
    }

    subkey   = WindowPlacementProfile::BuildTopologySubkey (hMon);
    bounds.x = wr.left;
    bounds.y = wr.top;
    bounds.w = width;
    bounds.h = height;

    hr = s_profile.Save (subkey, bounds);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
