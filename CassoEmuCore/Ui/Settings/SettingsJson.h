#pragma once

#include "Pch.h"

#include "Ui/Settings/SettingsPanelState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsJson
//
//  The machine document as the settings sheet reads and writes it: the UI
//  prefs, the machine's description, its hardware and ports read out of the
//  merged document, and the document built back from what is staged.
//
////////////////////////////////////////////////////////////////////////////////

class SettingsJson
{
public:
    // Pure helpers (exposed for tests)

    static HRESULT ExtractHardware    (const JsonValue            & mergedJson,
                                       std::vector<HardwareEntry> & outEntries);
    static HRESULT ExtractMachineInfo (const std::string          & machineId,
                                       const JsonValue            & mergedJson,
                                       SettingsMachineInfo        & outInfo);
    static HRESULT ExtractUiPrefs     (const JsonValue            & mergedJson,
                                       SettingsUiPrefs            & outPrefs);

    // The machine's own connectors, read from the root `ports` array. Absent
    // leaves the list empty, which BuildJson takes as "this machine never had
    // a back panel to write back".
    static std::string  GetPortDevice (const std::vector<SettingsMachinePort> & ports, const char * name);
    static HRESULT ExtractMachinePorts (const JsonValue                       & mergedJson,
                                        std::vector<SettingsMachinePort>      & outPorts);

    static JsonValue BuildJson        (const JsonValue                        & mergedJson,
                                       const std::vector<HardwareEntry>       & hw,
                                       const SettingsUiPrefs                  & prefs,
                                       const std::vector<SettingsMachinePort> & machinePorts = {});

    // The stored spellings of the enumerated UI prefs. Public because the
    // panel is not the only writer: the command bar's monitor-color picker
    // saves the same key, and both have to write the same word for it.
    static const char *        SpeedToString       (SettingsSpeedMode s);
    static SettingsSpeedMode   SpeedFromString     (const std::string & s, SettingsSpeedMode fallback);
    static const char *        ColorToString       (SettingsColorMode c);
    static SettingsColorMode   ColorFromString     (const std::string & s, SettingsColorMode fallback);
    static const char *        WriteModeToString   (SettingsWriteMode mode);
    static SettingsWriteMode   WriteModeFromString (const std::string & s, SettingsWriteMode fallback);

    // Appends every $cassoUiPrefs entry of `mergedJson` that `uiObj` does not
    // already carry, so a preference this dialog knows nothing about survives
    // a save.
    static void CarryUnmanagedUiPrefs (
        std::vector<std::pair<std::string, JsonValue>> & uiObj,
        const JsonValue                               & mergedJson);

    static bool ArePrefsEqual    (const SettingsUiPrefs & a, const SettingsUiPrefs & b);
    static bool AreHardwareEqual (const std::vector<HardwareEntry> & a,
                                  const std::vector<HardwareEntry> & b);

    static int  FindKey (
        const std::vector<std::pair<std::string, JsonValue>> & entries,
        const std::string                                    & key);

    static bool  TryGetBoolOpt (
        const JsonValue   & obj,
        const std::string & key,
        bool                fallback);

    static std::string  GetStringOpt (
        const JsonValue   & obj,
        const std::string & key,
        const std::string & fallback);

    static int  GetIntOpt (
        const JsonValue   & obj,
        const std::string & key,
        int                 fallback);

    static double  GetNumberOpt (
        const JsonValue   & obj,
        const std::string & key,
        double              fallback);

    static CapabilityFlag  ParseCapability (
        const std::string & str,
        CapabilityFlag      fallback);

    static JsonValue  CloneJson (const JsonValue & v);

    // Puts a shipped machine's internal device list into a working copy of its
    // document. The document itself no longer carries one: what devices a
    // machine has is stated in code, so the file describes only what an owner
    // configures. The hardware tree still has to show them, and this is where
    // they arrive rather than in each reader.
    //
    // A machine with no definition is left untouched -- its document is the
    // only description of it there is.
    static void  AddDefinedDevices (const std::string & machineName, JsonValue & doc);
};