#include "Pch.h"

#include "WindowPlacementProfile.h"





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
//  Folds the current monitor set + active monitor into a deterministic
//  16-hex-char FNV-1a hash. Two different physical topologies will (with
//  extremely high probability) produce different keys.
//
////////////////////////////////////////////////////////////////////////////////

std::string WindowPlacementProfile::BuildTopologyKey (HMONITOR activeMonitor)
{
    std::vector<MonitorSnapshot>  monitors;
    std::wstring                  activeDevice;
    MONITORINFOEXW                activeInfo  = { sizeof (activeInfo) };
    std::wstring                  canonical;
    uint64_t                      hash        = 0;
    char                          hashHex[kHashHexChars + 1] = {};
    size_t                        i           = 0;



    EnumDisplayMonitors (nullptr, nullptr, CollectMonitorsProc, reinterpret_cast<LPARAM> (&monitors));

    std::sort (monitors.begin(), monitors.end(),
               [] (const MonitorSnapshot & a, const MonitorSnapshot & b)
               {
                   if (a.device != b.device) { return a.device < b.device; }
                   if (a.rcMonitor.left != b.rcMonitor.left) { return a.rcMonitor.left < b.rcMonitor.left; }
                   if (a.rcMonitor.top != b.rcMonitor.top) { return a.rcMonitor.top < b.rcMonitor.top; }
                   if (a.rcMonitor.right != b.rcMonitor.right) { return a.rcMonitor.right < b.rcMonitor.right; }
                   return a.rcMonitor.bottom < b.rcMonitor.bottom;
               });

    if (activeMonitor != nullptr && GetMonitorInfoW (activeMonitor, &activeInfo))
    {
        activeDevice = activeInfo.szDevice;
    }

    for (auto & monitor : monitors)
    {
        const MonitorSnapshot & m = monitor;

        canonical += m.device;
        canonical += L"|";
        canonical += std::to_wstring (m.rcMonitor.left);
        canonical += L",";
        canonical += std::to_wstring (m.rcMonitor.top);
        canonical += L",";
        canonical += std::to_wstring (m.rcMonitor.right);
        canonical += L",";
        canonical += std::to_wstring (m.rcMonitor.bottom);
        canonical += L"|";
        canonical += std::to_wstring (m.rcWork.left);
        canonical += L",";
        canonical += std::to_wstring (m.rcWork.top);
        canonical += L",";
        canonical += std::to_wstring (m.rcWork.right);
        canonical += L",";
        canonical += std::to_wstring (m.rcWork.bottom);
        canonical += L"|";
        canonical += std::to_wstring (m.flags);
        canonical += L";";
    }

    canonical += L"active=";
    canonical += activeDevice;

    hash = HashFNV1a64 (canonical);
    sprintf_s (hashHex, _countof (hashHex), "%016llX", hash);

    return std::string (hashHex);
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
    Bounds            & outBounds) const
{
    bool  found = false;

    // A zero-sized stored placement counts as absent -- restoring it would
    // hand the user an invisible window.
    if (m_prefs != nullptr)
    {
        auto  it = m_prefs->window.placements.find (topologyKey);

        if (it != m_prefs->window.placements.end() && it->second.w > 0 && it->second.h > 0)
        {
            outBounds = it->second;
            found     = true;
        }
    }

    return found;
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
    const Bounds      & bounds)
{
    if (m_prefs == nullptr)
    {
        return;
    }

    m_prefs->window.placements[topologyKey] = bounds;
}
