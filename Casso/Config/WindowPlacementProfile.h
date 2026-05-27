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

private:
    GlobalUserPrefs  * m_prefs;
};
