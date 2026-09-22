#pragma once

#include "Pch.h"

#include "Config/GlobalUserPrefs.h"





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

    //  Which window's bounds, since the debugger window is placed by the
    //  user as the main one is and is remembered under the same key.
    enum class Target
    {
        Main,
        Debugger,
    };

    explicit WindowPlacementProfile (GlobalUserPrefs & prefs);

    bool    TryLoad (const std::string & topologyKey,
                     Bounds            & outBounds,
                     Target              target = Target::Main) const;
    void    Save    (const std::string & topologyKey,
                     const Bounds      & bounds,
                     Target              target = Target::Main);

    //  Every placement saved for this target, by topology key, so a caller
    //  with no entry for the current monitor set can fall back to one the
    //  user chose under another.
    const std::map<std::string, Bounds> &  GetAll (Target target) const;

    // One attached monitor, as EnumDisplayMonitors hands it back. Public so
    // the key can be built from a list a test wrote by hand: the Win32 walk
    // is the only part of the key that needs a machine.
    struct MonitorSnapshot
    {
        std::wstring  device;
        RECT          rcMonitor = {};
        RECT          rcWork    = {};
        DWORD         flags     = 0;
    };

    // Computes the per-monitor-topology key by enumerating attached
    // monitors and folding their device name and bounds through an FNV-1a
    // 64 hash. The enumeration is the only Win32 in it; the fold is
    // BuildTopologyKeyFrom, which tests drive directly.
    static std::string  BuildTopologyKey();

    // The key for a monitor set given as data. WHAT IS IN IT AND WHAT IS
    // NOT is the whole contract: device names and bounds, never the work
    // area, which a taskbar changes without any screen changing.
    static std::string  BuildTopologyKeyFrom (const std::vector<MonitorSnapshot> & monitors);

    // Whether a rect is a placement at all. A minimized window is parked
    // far off the desktop and an empty one is a window being torn down;
    // neither is anywhere the user put anything.
    static bool  IsPlaceableRect (const RECT & rect);

    static void  Touch (std::vector<std::string> & keys, const std::string & topologyKey);

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

    static uint64_t      HashFNV1a64        (const std::wstring & text);
    static bool          TryParseLong       (const std::wstring & text, LONG & outValue);
    static BOOL CALLBACK CollectMonitorsProc (HMONITOR hMon, HDC hdc, LPRECT prc, LPARAM lParam);

    GlobalUserPrefs  * m_prefs;
};
