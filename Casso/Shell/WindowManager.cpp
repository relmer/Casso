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

    // Two ways to decline: no saved placement for this monitor topology, or
    // one that no longer lands on any monitor (DEFAULTTONULL, not
    // DEFAULTTONEAREST -- a window saved on a since-removed screen must NOT be
    // dragged onto a surviving one, it should fall back to the default place).
    if (m_profile.TryLoad (topologyKey, bounds))
    {
        wr.left   = bounds.x;
        wr.top    = bounds.y;
        wr.right  = bounds.x + bounds.w;
        wr.bottom = bounds.y + bounds.h;

        hMon = MonitorFromRect (&wr, MONITOR_DEFAULTTONULL);
    }

    if (hMon != nullptr)
    {
        outX = bounds.x;
        outY = bounds.y;
        outW = bounds.w;
        outH = bounds.h;
    }

    return hMon != nullptr;
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



    // Only a normal, captioned, on-screen window's rect is the user's real
    // windowed placement. Minimized / maximized / fullscreen rects are not,
    // and a CAPTION-LESS window is the borderless-fullscreen popup or some
    // other transitional state -- the fullscreen flag covers the steady state,
    // this covers transitions, where a synchronous WM_SIZE once persisted the
    // full-monitor rect and permanently stomped the user's window size.
    bool  savable = hwnd != nullptr
                    && !IsIconic (hwnd)
                    && !IsZoomed (hwnd)
                    && !fullscreen
                    && (GetWindowLong (hwnd, GWL_STYLE) & WS_CAPTION) == WS_CAPTION
                    && GetWindowRect (hwnd, &wr);

    if (savable)
    {
        width   = wr.right - wr.left;
        height  = wr.bottom - wr.top;
        hMon    = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);
        savable = width > 0 && height > 0 && hMon != nullptr;
    }

    if (savable)
    {
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
}
