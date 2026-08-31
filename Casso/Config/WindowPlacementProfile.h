#pragma once

#include "Pch.h"

#include "GlobalUserPrefs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WindowPlacementProfile
//
//  Per-monitor-topology window placement persistence. The topology hash
//  collapses the set of currently attached monitors plus the active
//  monitor into a stable key that gets stored in
//  GlobalUserPrefs::window.placements, so the saved bounds for a single-
//  monitor laptop don't bleed onto a docked multi-monitor setup.
//
//  Stateless aside from the injected `GlobalUserPrefs`; one instance
//  per shell is plenty. Tests construct it against a stack-allocated
//  GlobalUserPrefs to exercise the load / save shape.
//
////////////////////////////////////////////////////////////////////////////////

class WindowPlacementProfile
{
public:
    using Bounds = GlobalUserPrefs::WindowBounds;

    explicit WindowPlacementProfile (GlobalUserPrefs & prefs);

    bool    TryLoad (const std::string & topologyKey,
                     Bounds            & outBounds) const;
    void    Save    (const std::string & topologyKey,
                     const Bounds      & bounds);

    // Computes the per-monitor-topology key by enumerating attached
    // monitors and folding their device name + rect + work area + flags
    // through an FNV-1a 64 hash. Pure Win32 -- unit tests exercise the
    // load / save path with literal keys instead.
    static std::string  BuildTopologyKey (HMONITOR activeMonitor);

    // Places a window of the desired size on a monitor's work area, centered
    // where it fits, under one rule that outranks centering: THE CAPTION'S
    // TOP-LEFT CORNER IS NEVER OFF SCREEN. A window with its top-left off the
    // work area cannot be grabbed, moved, or closed by pointer -- the user is
    // left with a window they can only reach by keyboard.
    //
    // The size is fitted to the work area first, so overflow is already the
    // unlikely case: it survives only when a minimum window size exceeds the
    // monitor, and then the overflow is pushed to the RIGHT and BOTTOM edges,
    // which cost nothing but visibility. Centering applies only within
    // whatever room is left.
    //
    // Pure geometry, no Win32 state -- `work` is a monitor's work area and
    // the result is a window rect in the same coordinates.
    static RECT  FitToWorkArea (const RECT & work,
                                int          desiredWidth,
                                int          desiredHeight,
                                int          minWidth  = 0,
                                int          minHeight = 0);

private:
    static constexpr uint64_t  kFnvOffset    = 1469598103934665603ull;
    static constexpr uint64_t  kFnvPrime     = 1099511628211ull;
    static constexpr int       kHashHexChars = 16;

    // One attached monitor, as EnumDisplayMonitors hands it back. Nested
    // rather than file-scope: a bare struct in a .cpp has external linkage,
    // so two translation units defining different types under one name is
    // an ODR violation the linker will not report.
    struct MonitorSnapshot
    {
        std::wstring  device;
        RECT          rcMonitor = {};
        RECT          rcWork    = {};
        DWORD         flags     = 0;
    };

    static uint64_t      HashFNV1a64        (const std::wstring & text);
    static bool          TryParseLong       (const std::wstring & text, LONG & outValue);
    static BOOL CALLBACK CollectMonitorsProc (HMONITOR hMon, HDC hdc, LPRECT prc, LPARAM lParam);

    GlobalUserPrefs  * m_prefs;
};
