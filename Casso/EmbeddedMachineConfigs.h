#pragma once

#include "resource.h"


////////////////////////////////////////////////////////////////////////////////
//
//  EmbeddedConfig / s_kEmbeddedConfigs
//
//  Single source of truth pairing each embedded machine JSON (an RCDATA
//  resource in Casso.exe) with the $cassoMachineVersion it must carry.
//  EnsureMachineConfigs stamps upgrades from currentVersion, so a stamp
//  that drifts below its JSON silently skips a real config change -- the
//  bug that once left Apple2 stamped 6 while its JSON had moved to 7,
//  hiding the ][ game port from anyone who had already run the machine.
//
//  Two guards read these exact values to keep the stamp and the JSON in
//  lockstep: a _DEBUG self-check in EnsureMachineConfigs (fails at startup)
//  and Embedded_StampMatchesEachJsonCassoMachineVersion in
//  AssetBootstrapTests (fails in CI). Lives in a shared header precisely so
//  the test reads the real stamps rather than a hand-copied duplicate.
//
////////////////////////////////////////////////////////////////////////////////

struct EmbeddedConfig
{
    int               resourceId;
    std::string_view  machineName;     // "Apple2", "Apple2Plus", "Apple2e"
    std::string_view  fileName;        // "<machineName>.json"
    int               currentVersion;  // must match "$cassoMachineVersion" in the embedded JSON
};


inline constexpr EmbeddedConfig s_kEmbeddedConfigs[] =
{
    { IDR_MACHINE_APPLE2,     "Apple2",     "Apple2.json",     7 },
    { IDR_MACHINE_APPLE2PLUS, "Apple2Plus", "Apple2Plus.json", 8 },
    { IDR_MACHINE_APPLE2E,    "Apple2e",    "Apple2e.json",    7 },
};
