#pragma once

#include "Pch.h"

#include "Core/JsonValue.h"
#include "Core/MachineConfig.h"






////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanelState
//
//  P7-T1 / P7-T5 — pure-logic transient snapshot backing the
//  consolidated Settings panel. Holds two parallel copies of every
//  user-mutable setting:
//
//      * `original`  -- the values as merged from
//        `<default>.json` + `<machine>_user.json` at the moment
//        `LoadFromMachine` was called. Never mutated by the panel.
//      * `current`   -- the values the user is editing live in the
//        panel. Mutated by every checkbox / dropdown / slider event.
//
//  Apply diffs `current` against the default to build a new
//  `currentJson` suitable for `UserConfigStore::SaveDelta`, then
//  pushes the live-applicable fields through `ISettingsApplySink`.
//
//  Per-machine UI preferences (speed mode, color mode, floppy sound,
//  mechanism, per-drive write protect) are persisted under the
//  reserved JSON key `$cassoUiPrefs` so they round-trip the same
//  upgrade/migration path as the rest of the machine config without
//  expanding the typed `MachineConfig` schema.
//
//  Hardware enable/disable is encoded as a per-entry boolean `enabled`
//  field on the matching `internalDevices[]` / `slots[]` object.
//  Capability-flag enforcement (FR-006/007/008) is performed inside
//  `SetHardwareEnabled` -- attempts to disable a `required` or
//  `platform-locked` entry return `E_INVALIDARG` and leave the
//  snapshot untouched.
//
//  No I/O. No RmlUi. No Win32. This is the pure-logic seam exercised
//  by `SettingsPanelStateTests` and `HardwareTreeTests`.
//
////////////////////////////////////////////////////////////////////////////////

enum class SettingsSpeedMode
{
    Authentic = 0,
    Double    = 1,
    Maximum   = 2,
};


enum class SettingsColorMode
{
    Color     = 0,
    Green     = 1,
    Amber     = 2,
    White     = 3,
};


struct SettingsUiPrefs
{
    SettingsSpeedMode  speedMode             = SettingsSpeedMode::Authentic;
    SettingsColorMode  colorMode             = SettingsColorMode::Color;
    bool               floppySoundEnabled    = true;
    std::string        floppyMechanism       = "shugart";   // "shugart" | "alps"
    bool               writeProtect[2]       = { false, false };
};


enum class HardwareEntryKind
{
    InternalDevice,
    Slot,
};


struct HardwareEntry
{
    HardwareEntryKind  kind         = HardwareEntryKind::InternalDevice;
    int                jsonIndex    = -1;             // index within internalDevices[] / slots[]
    int                slot         = 0;              // 0 for internal devices
    std::string        displayName;                   // e.g. "apple2e-keyboard" or "Slot 6: disk-ii"
    std::string        type;                          // raw "type" / "device" string
    CapabilityFlag     capability   = CapabilityFlag::Optional;
    std::string        lockReason;                    // only meaningful for PlatformLocked
    bool               enabled      = true;
};


////////////////////////////////////////////////////////////////////////////////
//
//  ISettingsApplySink
//
//  Plumbing seam for `SettingsPanelState::Apply`. Production wires
//  this to `EmulatorShell` (live-effect mutators + reset queue);
//  tests supply a recording mock.
//
////////////////////////////////////////////////////////////////////////////////

class ISettingsApplySink
{
public:
    virtual ~ISettingsApplySink() = default;

    virtual void ApplySpeedMode      (SettingsSpeedMode mode)        = 0;
    virtual void ApplyColorMode      (SettingsColorMode mode)        = 0;
    virtual void ApplyFloppySound    (bool enabled)                  = 0;
    virtual void ApplyMechanism      (const std::string & mechanism) = 0;
    virtual void ApplyWriteProtect   (int drive, bool wp)            = 0;
    virtual void QueueMachineReset   ()                              = 0;
};


////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanelState
//
////////////////////////////////////////////////////////////////////////////////

class SettingsPanelState
{
public:

    SettingsPanelState ();

    HRESULT LoadFromMachine (const std::string & machineName,
                             const JsonValue   & defaultJson,
                             const JsonValue   & mergedJson);
    void    Cancel          ();

    bool IsDirty       () const;
    bool RequiresReset () const;          // true iff any hardware enable changed

    // ---- Live (current) accessors / mutators ---------------------------

    const std::string              & MachineName   () const { return m_machineName; }
    const SettingsUiPrefs          & Prefs         () const { return m_current.prefs; }
    const std::vector<HardwareEntry> & Hardware    () const { return m_current.hardware; }

    void    SetSpeedMode       (SettingsSpeedMode mode);
    void    SetColorMode       (SettingsColorMode mode);
    void    SetFloppySound     (bool enabled);
    void    SetMechanism       (const std::string & mechanism);
    void    SetWriteProtect    (int drive, bool wp);
    HRESULT SetHardwareEnabled (size_t index, bool enabled);

    // ---- Apply ---------------------------------------------------------

    HRESULT Apply (ISettingsApplySink & sink,
                   JsonValue          & outCurrentJson) const;

    // ---- Pure helpers (exposed for tests) ------------------------------

    static HRESULT ExtractHardware (const JsonValue            & mergedJson,
                                    std::vector<HardwareEntry> & outEntries);
    static HRESULT ExtractUiPrefs  (const JsonValue            & mergedJson,
                                    SettingsUiPrefs            & outPrefs);
    static JsonValue BuildJson     (const JsonValue                 & mergedJson,
                                    const std::vector<HardwareEntry> & hw,
                                    const SettingsUiPrefs          & prefs);

private:

    struct Snapshot
    {
        SettingsUiPrefs            prefs;
        std::vector<HardwareEntry> hardware;
    };

    static bool PrefsEqual    (const SettingsUiPrefs & a, const SettingsUiPrefs & b);
    static bool HardwareEqual (const std::vector<HardwareEntry> & a,
                               const std::vector<HardwareEntry> & b);


    std::string  m_machineName;
    JsonValue    m_defaultJson;
    JsonValue    m_mergedJson;

    Snapshot     m_original;
    Snapshot     m_current;
};
