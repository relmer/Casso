#include "Pch.h"

#include "WindowPlacementProfile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr uint64_t  s_kFnvOffset    = 1469598103934665603ull;
    constexpr uint64_t  s_kFnvPrime     = 1099511628211ull;
    constexpr int       s_kHashHexChars = 16;


    struct MonitorSnapshot
    {
        std::wstring  device;
        RECT          rcMonitor = {};
        RECT          rcWork    = {};
        DWORD         flags     = 0;
    };


    uint64_t  HashFNV1a64 (const std::wstring & text)
    {
        uint64_t  hash = s_kFnvOffset;
        size_t    i    = 0;



        for (i = 0; i < text.size(); ++i)
        {
            uint64_t  code = static_cast<uint64_t> (text[i]);

            hash ^= (code & 0xFFu);
            hash *= s_kFnvPrime;
            hash ^= ((code >> 8) & 0xFFu);
            hash *= s_kFnvPrime;
        }

        return hash;
    }


    bool  TryParseLong (const std::wstring & text, LONG & outValue)
    {
        wchar_t * end    = nullptr;
        long      parsed = 0;



        if (text.empty())
        {
            return false;
        }

        parsed = wcstol (text.c_str(), &end, 10);
        if (end == nullptr || *end != L'\0')
        {
            return false;
        }

        outValue = static_cast<LONG> (parsed);
        return true;
    }


    BOOL CALLBACK CollectMonitorsProc (HMONITOR hMon, HDC hdc, LPRECT prc, LPARAM lParam)
    {
        std::vector<MonitorSnapshot> *  list = reinterpret_cast<std::vector<MonitorSnapshot> *> (lParam);
        MONITORINFOEXW                  mi   = { sizeof (mi) };
        MonitorSnapshot                 snap;



        UNREFERENCED_PARAMETER (hdc);
        UNREFERENCED_PARAMETER (prc);

        if (list == nullptr)
        {
            return FALSE;
        }

        if (!GetMonitorInfoW (hMon, &mi))
        {
            return TRUE;
        }

        snap.device    = mi.szDevice;
        snap.rcMonitor = mi.rcMonitor;
        snap.rcWork    = mi.rcWork;
        snap.flags     = mi.dwFlags;

        list->push_back (snap);
        return TRUE;
    }
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
    char                          hashHex[s_kHashHexChars + 1] = {};
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

    for (i = 0; i < monitors.size(); ++i)
    {
        const MonitorSnapshot & m = monitors[i];

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
    if (m_prefs == nullptr)
    {
        return false;
    }

    auto  it = m_prefs->window.placements.find (topologyKey);
    if (it == m_prefs->window.placements.end())
    {
        return false;
    }
    if (it->second.w <= 0 || it->second.h <= 0)
    {
        return false;
    }

    outBounds = it->second;
    return true;
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
