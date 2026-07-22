#include "Pch.h"

#include "WindowManager.h"

#include "../Config/WindowPlacementProfile.h"
#include "../Config/GlobalUserPrefs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WindowManager::WindowManager
//
////////////////////////////////////////////////////////////////////////////////

WindowManager::WindowManager (GlobalUserPrefs & prefs, SavePrefsFn savePrefs)
    : m_profile   (prefs)
    , m_savePrefs (std::move (savePrefs))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildPlacementKeyForMonitor
//
//  Thin compatibility shim. Delegates to WindowPlacementProfile so the
//  topology-hashing logic has exactly one home.
//
////////////////////////////////////////////////////////////////////////////////

std::string WindowManager::BuildPlacementKeyForMonitor (HMONITOR activeMonitor)
{
    return WindowPlacementProfile::BuildTopologyKey (activeMonitor);
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
    std::string                      topologyKey;
    WindowPlacementProfile::Bounds   bounds;
    RECT                             wr     = {};
    HMONITOR                         hMon   = nullptr;



    topologyKey = WindowPlacementProfile::BuildTopologyKey (activeMonitor);

    if (!m_profile.TryLoad (topologyKey, bounds))
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
    std::string                      topologyKey;
    RECT                             wr      = {};
    int                              width   = 0;
    int                              height  = 0;
    WindowPlacementProfile::Bounds   bounds;



    if (hwnd == nullptr)
    {
        return;
    }

    if (IsIconic (hwnd) || IsZoomed (hwnd) || fullscreen)
    {
        return;
    }

    // A caption-less window is the borderless-fullscreen popup (or some other
    // transitional state), never the user's real windowed placement. The
    // fullscreen flag above covers the steady state; this covers transitions,
    // where a synchronous WM_SIZE once persisted the full-monitor rect as the
    // windowed placement and permanently stomped the user's window size.
    if ((GetWindowLong (hwnd, GWL_STYLE) & WS_CAPTION) != WS_CAPTION)
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

    topologyKey = WindowPlacementProfile::BuildTopologyKey (hMon);
    bounds.x = wr.left;
    bounds.y = wr.top;
    bounds.w = width;
    bounds.h = height;

    m_profile.Save (topologyKey, bounds);

    if (m_savePrefs)
    {
        m_savePrefs();
    }
}
