#pragma once

#include "Pch.h"

#include "IRegistrySettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WindowPlacementProfile
//
//  Per-monitor-topology window placement persistence. The topology hash
//  collapses the set of currently attached monitors plus the active
//  monitor into a stable subkey under HKCU\Software\relmer\Casso\
//  WindowPlacement\v1\<hash>, so the saved bounds for a single-monitor
//  laptop don't bleed onto a docked multi-monitor setup.
//
//  Stateless aside from the injected `IRegistrySettings`; one instance
//  per shell is plenty. Tests construct it against `InMemoryRegistry`
//  to exercise the storage shape without touching HKCU.
//
//  All bounds are persisted as decimal `REG_SZ` so the previous
//  registry-backed callers continue to read them. The string format is
//  intentionally trivial — humans can hand-edit the values from regedit
//  and re-launch.
//
////////////////////////////////////////////////////////////////////////////////

class WindowPlacementProfile
{
public:
    struct Bounds
    {
        LONG  x = 0;
        LONG  y = 0;
        int   w = 0;
        int   h = 0;
    };

    explicit WindowPlacementProfile (IRegistrySettings & reg);

    bool    TryLoad (const std::wstring & topologySubkey,
                     Bounds             & outBounds) const;
    HRESULT Save    (const std::wstring & topologySubkey,
                     const Bounds       & bounds);

    // Computes the per-monitor-topology subkey by enumerating attached
    // monitors and folding their device name + rect + work area + flags
    // through an FNV-1a 64 hash. Pure Win32 — unit tests exercise the
    // load/save path with literal subkeys instead.
    static std::wstring  BuildTopologySubkey (HMONITOR activeMonitor);

private:
    IRegistrySettings  * m_reg;
};
