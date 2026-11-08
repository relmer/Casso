#include "Pch.h"

#include "WindowManager.h"

#include "Config/WindowPlacementProfile.h"
#include "Config/GlobalUserPrefs.h"





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
//  BuildPlacementKey
//
//  Thin compatibility shim. Delegates to WindowPlacementProfile so the
//  topology-hashing logic has exactly one home.
//
////////////////////////////////////////////////////////////////////////////////

std::string WindowManager::BuildPlacementKey()
{
    return WindowPlacementProfile::BuildTopologyKey();
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
    LONG    & outX,
    LONG    & outY,
    int     & outW,
    int     & outH,
    bool    & outMaximized) const
{
    std::string                      topologyKey;
    WindowPlacementProfile::Bounds   bounds;
    RECT                             wr     = {};
    HMONITOR                         hMon   = nullptr;



    topologyKey = WindowPlacementProfile::BuildTopologyKey();

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
        outX         = bounds.x;
        outY         = bounds.y;
        outW         = bounds.w;
        outH         = bounds.h;
        outMaximized = bounds.maximized;
    }

    WindowTrace::LogRect ((hMon != nullptr) ? "restore.hit" : "restore.miss", "main",
                          bounds.x, bounds.y, bounds.w, bounds.h, "key=" + topologyKey);

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
    // windowed placement. Minimized / fullscreen rects are not, and a
    // CAPTION-LESS window is the borderless-fullscreen popup or some other
    // transitional state -- the fullscreen flag covers the steady state,
    // this covers transitions, where a synchronous WM_SIZE once persisted
    // the full-monitor rect and permanently stomped the user's window size.
    //
    // MAXIMIZED IS A STATE, NOT A RECT. A zoomed window saves the visible
    // rect it had when last restored, with the flag set, and restore
    // re-maximizes from there. Its normal WINDOW rect cannot stand in: the
    // border measured on a maximized window is wider than the restored one,
    // and subtracting it shrank the window by the difference every cycle.
    bool  zoomed  = hwnd != nullptr && IsZoomed (hwnd);
    //  THE VISIBLE RECT IS WHAT IS STORED. A window rect takes in the
    //  invisible resize border, and the same stored numbers then mean a
    //  filled screen half on one window and a margin on another.
    bool  savable = hwnd != nullptr
                    && !IsIconic (hwnd)
                    && !fullscreen
                    && (GetWindowLong (hwnd, GWL_STYLE) & WS_CAPTION) == WS_CAPTION
                    && GetWindowRect (hwnd, &wr);

    if (savable)
    {
        UpdateNormalPlacement (hwnd);

        wr      = DxuiWindowFrame::GetVisibleRect (hwnd);
        savable = m_normalPlacement.TryGetNormalRect (!zoomed, wr, wr);
    }

    if (savable)
    {
        width   = wr.right - wr.left;
        height  = wr.bottom - wr.top;
        hMon    = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);
        savable = WindowPlacementProfile::IsPlaceableRect (wr) && hMon != nullptr;
    }

    if (savable)
    {
        topologyKey      = WindowPlacementProfile::BuildTopologyKey();
        bounds.x         = wr.left;
        bounds.y         = wr.top;
        bounds.w         = width;
        bounds.h         = height;
        bounds.maximized = zoomed;

        WindowTrace::LogRect ("save", "main", wr, "key=" + topologyKey +
                              (zoomed ? " maximized" : ""));
        m_profile.Save (topologyKey, bounds);

        if (m_savePrefs)
        {
            m_savePrefs();
        }
    }
    else
    {
        WindowTrace::LogWindow ("save.refused", "main", hwnd,
                                fullscreen ? "fullscreen" : "not a windowed rect");
    }
}






////////////////////////////////////////////////////////////////////////////////
//
//  SetExpectedVisibleRect
//
//  The saved visible rect the window was created at. It is what a save
//  reports until the window has been shown restored, and it is applied again
//  then, when the restored border can be measured for certain.
//
////////////////////////////////////////////////////////////////////////////////

void WindowManager::SetExpectedVisibleRect (const RECT & visible)
{
    m_normalPlacement.SetExpectedRect (visible);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateNormalPlacement
//
//  Puts the visible frame exactly on the saved rect the first time the window
//  is shown restored. A window that starts maximized gets there on its first
//  restore instead. A restored window's rect is then recorded, so a window
//  that opened at the default place and is maximized before it ever moves
//  still has a restored rect to save.
//
//  TWICE, ON PURPOSE. Landing on a monitor of a different scale makes Windows
//  send WM_DPICHANGED and resize the window by the ratio of the two, and the
//  border scales with it, so it is measured again after the first move.
//
////////////////////////////////////////////////////////////////////////////////

void WindowManager::UpdateNormalPlacement (HWND hwnd)
{
    constexpr UINT  kFlags    = SWP_NOZORDER | SWP_NOACTIVATE;
    constexpr int   kPasses   = 2;
    RECT            visible   = {};
    RECT            placed    = {};
    bool            restored  = hwnd != nullptr
                                && IsWindowVisible (hwnd)
                                && !IsZoomed (hwnd)
                                && !IsIconic (hwnd);
    bool            taken     = false;
    bool            recorded  = false;



    taken = m_normalPlacement.TryTakePending (restored, visible);

    for (int pass = 0; taken && pass < kPasses; ++pass)
    {
        placed = DxuiWindowFrame::ToWindowRect (hwnd, visible);

        SetWindowPos (hwnd, nullptr, placed.left, placed.top,
                      placed.right - placed.left, placed.bottom - placed.top, kFlags);
    }

    if (taken)
    {
        WindowTrace::LogWindow ("restore.applied", "main", hwnd, "the saved visible rect, with the border measured restored");
    }

    if (restored)
    {
        recorded = m_normalPlacement.TryGetNormalRect (true, DxuiWindowFrame::GetVisibleRect (hwnd), visible);
        IGNORE_RETURN_VALUE (recorded, false);
    }
}
