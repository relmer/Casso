#include "Pch.h"

#include "WindowManager.h"

#include "../RegistrySettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr uint64_t  s_kFnvOffset = 1469598103934665603ull;
    constexpr uint64_t  s_kFnvPrime  = 1099511628211ull;
    constexpr int       s_kHashHexChars = 16;


    struct MonitorSnapshot
    {
        std::wstring  device;
        RECT          rcMonitor  = {};
        RECT          rcWork     = {};
        DWORD         flags      = 0;
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
//  BuildPlacementSubkeyForMonitor
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WindowManager::BuildPlacementSubkeyForMonitor (HMONITOR activeMonitor)
{
    std::vector<MonitorSnapshot>  monitors;
    std::wstring                  activeDevice;
    MONITORINFOEXW                activeInfo  = { sizeof (activeInfo) };
    std::wstring                  canonical;
    uint64_t                      hash        = 0;
    wchar_t                       hashHex[s_kHashHexChars + 1] = {};
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
    swprintf_s (hashHex, _countof (hashHex), L"%016llX", hash);

    return std::wstring (L"WindowPlacement\\v1\\") + hashHex;
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
    HRESULT       hr      = S_OK;
    std::wstring  subkey;
    std::wstring  sx;
    std::wstring  sy;
    std::wstring  sw;
    std::wstring  sh;
    LONG          x       = 0;
    LONG          y       = 0;
    LONG          w       = 0;
    LONG          h       = 0;
    RECT          wr      = {};
    HMONITOR      hMon    = nullptr;



    subkey = BuildPlacementSubkeyForMonitor (activeMonitor);

    hr = RegistrySettings::ReadString (subkey.c_str(), L"x", sx);
    if (hr != S_OK) { return false; }
    hr = RegistrySettings::ReadString (subkey.c_str(), L"y", sy);
    if (hr != S_OK) { return false; }
    hr = RegistrySettings::ReadString (subkey.c_str(), L"w", sw);
    if (hr != S_OK) { return false; }
    hr = RegistrySettings::ReadString (subkey.c_str(), L"h", sh);
    if (hr != S_OK) { return false; }

    if (!TryParseLong (sx, x) || !TryParseLong (sy, y) || !TryParseLong (sw, w) || !TryParseLong (sh, h))
    {
        return false;
    }

    if (w <= 0 || h <= 0)
    {
        return false;
    }

    wr.left   = x;
    wr.top    = y;
    wr.right  = x + w;
    wr.bottom = y + h;

    hMon = MonitorFromRect (&wr, MONITOR_DEFAULTTONULL);
    if (hMon == nullptr)
    {
        return false;
    }

    outX = x;
    outY = y;
    outW = static_cast<int> (w);
    outH = static_cast<int> (h);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveWindowPlacement
//
////////////////////////////////////////////////////////////////////////////////

void WindowManager::SaveWindowPlacement (HWND hwnd, bool fullscreen)
{
    HMONITOR      hMon    = nullptr;
    std::wstring  subkey;
    RECT          wr      = {};
    int           width   = 0;
    int           height  = 0;
    HRESULT       hr      = S_OK;



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

    subkey = BuildPlacementSubkeyForMonitor (hMon);

    hr = RegistrySettings::WriteString (subkey.c_str(), L"x", std::to_wstring (wr.left));
    IGNORE_RETURN_VALUE (hr, S_OK);
    hr = RegistrySettings::WriteString (subkey.c_str(), L"y", std::to_wstring (wr.top));
    IGNORE_RETURN_VALUE (hr, S_OK);
    hr = RegistrySettings::WriteString (subkey.c_str(), L"w", std::to_wstring (width));
    IGNORE_RETURN_VALUE (hr, S_OK);
    hr = RegistrySettings::WriteString (subkey.c_str(), L"h", std::to_wstring (height));
    IGNORE_RETURN_VALUE (hr, S_OK);
}
