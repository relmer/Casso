#include "Pch.h"

#include "Config/WindowPlacementProfile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

uint64_t  WindowPlacementProfile::HashFNV1a64 (const std::wstring & text)
{
    uint64_t  hash = kFnvOffset;



    for (wchar_t ch : text)
    {
        uint64_t  code = static_cast<uint64_t> (ch);

        hash ^= (code & 0xFFu);
        hash *= kFnvPrime;
        hash ^= ((code >> 8) & 0xFFu);
        hash *= kFnvPrime;
    }

    return hash;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowPlacementProfile::TryParseLong
//
////////////////////////////////////////////////////////////////////////////////

bool  WindowPlacementProfile::TryParseLong (const std::wstring & text, LONG & outValue)
{
    wchar_t * end    = nullptr;
    long      parsed = 0;
    bool      ok     = !text.empty();



    // wcstol must have consumed the WHOLE string: "12x" is a malformed
    // placement value, not 12.
    if (ok)
    {
        parsed = wcstol (text.c_str(), &end, 10);
        ok     = (end != nullptr && *end == L'\0');
    }

    if (ok)
    {
        outValue = static_cast<LONG> (parsed);
    }

    return ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowPlacementProfile::CollectMonitorsProc
//
//  EnumDisplayMonitors callback: snapshots each monitor's identity and rects
//  into the caller's list, which becomes the topology key.
//
//  The return value carries the API's own meaning and the two false-ish exits
//  mean OPPOSITE things: FALSE aborts the entire enumeration, TRUE continues
//  it. So a null list stops the walk -- there is nowhere to put results -- but
//  a monitor whose info could not be read returns TRUE and lets the next one
//  be tried, since a partial topology beats none.
//
//  The device NAME is captured alongside the rects because two monitors can
//  share identical geometry; the name is what distinguishes them, and the
//  topology key must change when the arrangement does.
//
//  The work rect is snapshotted as well as the monitor rect, since a taskbar
//  moving between edges changes where a window may legitimately sit without
//  changing the display arrangement.
//
////////////////////////////////////////////////////////////////////////////////

BOOL CALLBACK WindowPlacementProfile::CollectMonitorsProc (HMONITOR hMon, HDC hdc, LPRECT prc, LPARAM lParam)
{
    std::vector<MonitorSnapshot> *  list = reinterpret_cast<std::vector<MonitorSnapshot> *> (lParam);
    MONITORINFOEXW                  mi   = { sizeof (mi) };
    MonitorSnapshot                 snap;



    // The two false-ish exits mean opposite things to EnumDisplayMonitors:
    // FALSE aborts the whole enumeration (no list to fill, so stop), while
    // TRUE continues it (this one monitor failed, try the next).
    BOOL  keepEnumerating = (list != nullptr);

    UNREFERENCED_PARAMETER (hdc);
    UNREFERENCED_PARAMETER (prc);

    if (keepEnumerating && GetMonitorInfoW (hMon, &mi))
    {
        snap.device    = mi.szDevice;
        snap.rcMonitor = mi.rcMonitor;
        snap.rcWork    = mi.rcWork;
        snap.flags     = mi.dwFlags;

        list->push_back (snap);
    }

    return keepEnumerating;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowPlacementProfile::WindowPlacementProfile
//
////////////////////////////////////////////////////////////////////////////////

WindowPlacementProfile::WindowPlacementProfile (GlobalUserPrefs & prefs)
    : m_prefs (&prefs)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowPlacementProfile::BuildTopologyKey
//
//  Folds the current monitor set into a deterministic 16-hex-char FNV-1a
//  hash. Two different physical topologies will (with extremely high
//  probability) produce different keys.
//
//  NEITHER THE ACTIVE MONITOR NOR THE WORK AREA IS PART OF THE KEY.
//
//  A window is saved against the monitor it sits on and restored before it
//  exists, when the active monitor is whichever one the OS hands out -- so
//  folding that in gave the two operations different keys for one
//  arrangement, and every launch restored the stale rect stored under the
//  startup key.
//
//  The work area is worse, because it moves on its own: a taskbar that
//  auto-hides, or an appbar docking, changes it without anything about the
//  monitors changing. The log caught three keys inside ten seconds, each
//  holding part of one arrangement. A taskbar sliding away is not a
//  different set of screens.
//
////////////////////////////////////////////////////////////////////////////////

std::string WindowPlacementProfile::BuildTopologyKey()
{
    std::vector<MonitorSnapshot>  monitors;



    EnumDisplayMonitors (nullptr, nullptr, CollectMonitorsProc, reinterpret_cast<LPARAM> (&monitors));

    return BuildTopologyKeyFrom (monitors);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowPlacementProfile::BuildTopologyKeyFrom
//
//  The fold, with no machine in it. Monitors are sorted first, so the order
//  the OS happens to enumerate them in cannot change the key.
//
////////////////////////////////////////////////////////////////////////////////

std::string WindowPlacementProfile::BuildTopologyKeyFrom (const std::vector<MonitorSnapshot> & monitors)
{
    std::vector<MonitorSnapshot>  sorted                     = monitors;
    std::wstring                  canonical;
    uint64_t                      hash                       = 0;
    char                          hashHex[kHashHexChars + 1] = {};



    std::sort (sorted.begin(), sorted.end(),
               [] (const MonitorSnapshot & a, const MonitorSnapshot & b)
               {
                   if (a.device != b.device) { return a.device < b.device; }
                   if (a.rcMonitor.left != b.rcMonitor.left) { return a.rcMonitor.left < b.rcMonitor.left; }
                   if (a.rcMonitor.top != b.rcMonitor.top) { return a.rcMonitor.top < b.rcMonitor.top; }
                   if (a.rcMonitor.right != b.rcMonitor.right) { return a.rcMonitor.right < b.rcMonitor.right; }
                   return a.rcMonitor.bottom < b.rcMonitor.bottom;
               });

    for (const MonitorSnapshot & m : sorted)
    {
        canonical += m.device;
        canonical += L"|";
        canonical += std::to_wstring (m.rcMonitor.left);
        canonical += L",";
        canonical += std::to_wstring (m.rcMonitor.top);
        canonical += L",";
        canonical += std::to_wstring (m.rcMonitor.right);
        canonical += L",";
        canonical += std::to_wstring (m.rcMonitor.bottom);
        canonical += L";";
    }

    hash = HashFNV1a64 (canonical);
    sprintf_s (hashHex, _countof (hashHex), "%016llX", hash);

    return std::string (hashHex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowPlacementProfile::IsPlaceableRect
//
//  A minimized window is parked at -32000, far off any desktop, and a window
//  being torn down reports an empty rect. Neither is a place the user put
//  anything, and one of each reached the preferences file before this.
//
////////////////////////////////////////////////////////////////////////////////

bool WindowPlacementProfile::IsPlaceableRect (const RECT & rect)
{
    //  Further out than any real desktop reaches. GetSystemMetrics gives the
    //  virtual screen, which a minimized window sits well outside of.
    constexpr LONG  kParked = -30000;



    if (rect.right <= rect.left || rect.bottom <= rect.top)
    {
        return false;
    }

    return rect.left > kParked && rect.top > kParked;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowPlacementProfile::TryLoad
//
//  Reads the bounds for `topologyKey` from GlobalUserPrefs::window
//  ::placements. Missing entries (or zero-sized stored bounds) return
//  false so callers fall back to default-centered placement.
//
////////////////////////////////////////////////////////////////////////////////

bool WindowPlacementProfile::TryLoad (
    const std::string & topologyKey,
    Bounds            & outBounds,
    Target              target) const
{
    bool  found = false;



    // A zero-sized stored placement counts as absent -- restoring it would
    // hand the user an invisible window.
    if (m_prefs != nullptr)
    {
        const std::map<std::string, Bounds> &  saved = (target == Target::Debugger) ? m_prefs->window.debuggerPlacements
                                                                                    : m_prefs->window.placements;
        auto  it = saved.find (topologyKey);

        if (it != saved.end() && it->second.w > 0 && it->second.h > 0)
        {
            outBounds = it->second;
            found     = true;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowPlacementProfile::GetAll
//
////////////////////////////////////////////////////////////////////////////////

const std::map<std::string, WindowPlacementProfile::Bounds> & WindowPlacementProfile::GetAll (Target target) const
{
    static const std::map<std::string, Bounds>  none;



    if (m_prefs == nullptr)
    {
        return none;
    }

    return (target == Target::Debugger) ? m_prefs->window.debuggerPlacements : m_prefs->window.placements;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowPlacementProfile::Save
//
//  Writes the bounds into the GlobalUserPrefs window-placements map.
//  Persistence to disk is the caller's responsibility -- the same Save
//  pattern as every other GlobalUserPrefs mutation.
//
////////////////////////////////////////////////////////////////////////////////

void WindowPlacementProfile::Save (
    const std::string & topologyKey,
    const Bounds      & bounds,
    Target              target)
{
    if (m_prefs == nullptr)
    {
        return;
    }

    if (target == Target::Debugger)
    {
        m_prefs->window.debuggerPlacements[topologyKey] = bounds;
        Touch (m_prefs->window.touchedDebugger, topologyKey);
        return;
    }

    m_prefs->window.placements[topologyKey] = bounds;
    Touch (m_prefs->window.touched, topologyKey);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowPlacementProfile::Touch
//
//  Records that this session's user put a window here, so the save keeps it
//  and leaves every other key to whatever is on disk.
//
////////////////////////////////////////////////////////////////////////////////

void WindowPlacementProfile::Touch (std::vector<std::string> & keys, const std::string & topologyKey)
{
    if (std::find (keys.begin(), keys.end(), topologyKey) == keys.end())
    {
        keys.push_back (topologyKey);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowPlacementProfile::FitToWorkArea
//
//  Size first, then position. Fitting the size to the work area is what keeps
//  the caption reachable in the ordinary case; the position clamp is what
//  keeps it reachable in the case the size clamp cannot fix, when a minimum
//  window size is larger than the monitor.
//
//  The size is fitted BY UNIFORM SCALE, not per axis. Clamping each axis on
//  its own silently changes the requested proportions, and the caller asked
//  for that shape for a reason: the desk-scene window is sized so the scene
//  exactly fills it, so a window squeezed on one axis only gets a scene that
//  no longer fits its frame and letterboxes itself inside, leaving wide bands
//  of dead space down the sides and along the bottom. Shrinking both axes by
//  the same factor keeps the shape and keeps the content flush.
//
//  The position clamp is written as a max() of the low edge against a high
//  edge that may fall BELOW it. When the window fits, the high edge is the
//  larger and the window is centered inside the range. When it does not, the
//  high edge goes negative relative to the low one and the max() pins the
//  window to the work area's top-left -- putting every pixel of overflow off
//  the right and bottom, where it costs only visibility, never reachability.
//
////////////////////////////////////////////////////////////////////////////////

RECT WindowPlacementProfile::FitToWorkArea (const RECT & work,
                                            int          desiredWidth,
                                            int          desiredHeight,
                                            int          minWidth,
                                            int          minHeight)
{
    int    workW = (int) (work.right  - work.left);
    int    workH = (int) (work.bottom - work.top);
    float  scale = 1.0f;
    int    w     = 0;
    int    h     = 0;
    int    x     = 0;
    int    y     = 0;
    RECT   out   = {};



    if (desiredWidth > 0 && desiredWidth > workW)
    {
        scale = std::min (scale, (float) workW / (float) desiredWidth);
    }

    if (desiredHeight > 0 && desiredHeight > workH)
    {
        scale = std::min (scale, (float) workH / (float) desiredHeight);
    }

    // The minimums are applied after the uniform scale, so a minimum larger
    // than the monitor is the only thing that can break the proportions --
    // and that is the case the position clamp below exists to survive.
    w = std::max ((int) ((float) desiredWidth  * scale + 0.5f), minWidth);
    h = std::max ((int) ((float) desiredHeight * scale + 0.5f), minHeight);



    // Centered where there is room; pinned to the work-area origin where
    // there is not. Never past it in either axis.
    x = std::max ((int) work.left, std::min ((int) work.left + (workW - w) / 2, (int) work.right  - w));
    y = std::max ((int) work.top,  std::min ((int) work.top  + (workH - h) / 2, (int) work.bottom - h));

    out.left   = x;
    out.top    = y;
    out.right  = x + w;
    out.bottom = y + h;

    return out;
}
