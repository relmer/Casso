#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WindowManager
//
//  Owner of per-monitor-topology window-placement persistence. Today
//  the backing store is the legacy `HKCU\Software\relmer\Casso\
//  WindowPlacement\v1\<hash>` registry tree; a later sub-phase swaps
//  that out for the JSON-backed WindowPlacementProfile in
//  GlobalUserPrefs without changing this class's surface.
//
//  All entry points are stateless (no member data), so a single
//  shared instance is owned by EmulatorShell.
//
////////////////////////////////////////////////////////////////////////////////

class WindowManager
{
public:
    WindowManager  () = default;
    ~WindowManager () = default;

    void  SaveWindowPlacement       (HWND hwnd,
                                     bool fullscreen);
    bool  TryLoadSavedWindowPlacement (HMONITOR  activeMonitor,
                                       LONG    & outX,
                                       LONG    & outY,
                                       int     & outW,
                                       int     & outH) const;

    // Exposed for tests and for callers that need the same monitor
    // topology key without going through the load/save helpers.
    static std::wstring  BuildPlacementSubkeyForMonitor (HMONITOR activeMonitor);
};
