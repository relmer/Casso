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
//  Recovers the window position saved for THIS monitor arrangement, declining
//  if it no longer makes sense.
//
//  Placements are keyed by monitor TOPOLOGY, not stored as a single rect, so
//  docking and undocking a laptop restores the position that belonged to each
//  arrangement instead of the last one used.
//
//  The recovered rect is validated with MONITOR_DEFAULTTONULL, deliberately
//  not DEFAULTTONEAREST. A window saved on a screen that has since been
//  removed must NOT be dragged onto a surviving one -- it should fall back to
//  the default placement, which is where a user expects a window to appear
//  when its old home is gone.
//
//  Failing to load and loading something off-screen are both reported the same
//  way, since the caller does the same thing in either case.
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
//  Records the window rect -- but only when that rect is actually the user's
//  windowed placement.
//
//  Most of this function is the test for that, and every clause guards against
//  persisting something the user never chose:
//
//    minimized / maximized  the rect is not where they want the window
//    fullscreen             covers the steady borderless state
//    no WS_CAPTION          covers the TRANSITIONS in and out of it. A
//                           caption-less window is the borderless popup
//                           mid-flight, and a synchronous WM_SIZE during that
//                           transition once persisted the full-monitor rect
//                           and permanently stomped the user's window size
//    zero extent            a degenerate rect from a window being torn down
//
//  The fullscreen flag and the caption test overlap on purpose: the flag
//  describes intent, the style describes the window right now, and the failure
//  that motivated this happened in the gap between them.
//
//  It is saved against the current monitor topology, matching how
//  TryLoadSavedWindowPlacement looks it up.
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
