#pragma once

#include "Pch.h"

#include "../Config/WindowPlacementProfile.h"

struct GlobalUserPrefs;




////////////////////////////////////////////////////////////////////////////////
//
//  WindowManager
//
//  Owner of per-monitor-topology window-placement persistence. Backing
//  store is GlobalUserPrefs::window::placements (JSON-persisted via
//  UserConfigStore::SaveAll). Mutates the live prefs object via the
//  injected `WindowPlacementProfile` and then invokes the supplied
//  `savePrefs` callback so the change lands on disk.
//
////////////////////////////////////////////////////////////////////////////////

class WindowManager
{
public:
    using SavePrefsFn = std::function<void()>;

    WindowManager  (GlobalUserPrefs & prefs, SavePrefsFn savePrefs);

    void  SaveWindowPlacement       (HWND hwnd,
                                     bool fullscreen);
    bool  TryLoadSavedWindowPlacement (HMONITOR  activeMonitor,
                                       LONG    & outX,
                                       LONG    & outY,
                                       int     & outW,
                                       int     & outH) const;

    // Exposed for tests and for callers that need the same monitor
    // topology key without going through the load/save helpers.
    static std::string  BuildPlacementKeyForMonitor (HMONITOR activeMonitor);

private:
    mutable WindowPlacementProfile  m_profile;
    SavePrefsFn                     m_savePrefs;
};
