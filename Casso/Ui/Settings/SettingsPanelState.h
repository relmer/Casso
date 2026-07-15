#pragma once

#include "Pch.h"

#include "Core/JsonValue.h"
#include "Core/MachineConfig.h"






////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanelState
//
//  / — pure-logic transient snapshot backing the
//  consolidated Settings panel. Holds two parallel copies of every
//  user-mutable setting:
//
//      * `original`  -- the values as merged from the embedded default
//        plus unified user preferences at the moment `LoadFromMachine`
//        was called. Never mutated by the panel.
//      * `current`   -- the values the user is editing live in the
//        panel. Mutated by every checkbox / dropdown / slider event.
//
//  Apply diffs `current` against the default to build a new
//  `currentJson` suitable for `UserConfigStore::SaveDelta`, then
//  pushes the live-applicable fields through `ISettingsApplySink`.
//
//  Per-machine UI preferences (speed mode, color mode, write mode,
//  floppy sound, mechanism, per-drive write protect) are persisted under the
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
//  No I/O. No Win32. This is the pure-logic seam exercised
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


enum class SettingsWriteMode
{
    BufferAndFlush = 0,
    CopyOnWrite    = 1,
};


struct SettingsUiPrefs
{
    SettingsSpeedMode  speedMode             = SettingsSpeedMode::Authentic;
    SettingsColorMode  colorMode             = SettingsColorMode::Color;
    SettingsWriteMode  writeMode             = SettingsWriteMode::BufferAndFlush;
    bool               floppySoundEnabled    = true;
    std::string        floppyMechanism       = "shugart";   // "shugart" | "alps"
    bool               writeProtect[2]       = { false, false };
    // //c only: whether the optional external 5.25" drive is plugged into
    // the disk port. Reveals/hides the second drive-mount widget. Defaults
    // to not-connected -- a bare //c ships with just its internal drive.
    bool               externalDriveConnected = false;
    // //c only: whether the mouse peripheral is plugged into the DB-9 port
    // Defaults to CONNECTED (confirmed 2026-07-09): a connected-
    // but-unused mouse has no UI footprint (firmware-live gate), so
    // MousePaint works zero-config.
    bool               mouseConnected         = true;
    // Drive-audio component gains (0..1). Defaults mirror the
    // DriveAudioMixer / Disk2AudioSource sound-mix defaults.
    static constexpr float kDefaultDriveMotorVolume = 0.90f;
    static constexpr float kDefaultDriveHeadVolume  = 1.00f;
    static constexpr float kDefaultDriveDoorVolume  = 1.00f;
    // Per-drive stereo pan in [-1, +1] (-1 = hard left, +1 = hard
    // right). Drive 1 sits left-of-center, Drive 2 right-of-center,
    // mirroring DriveAudioMixer::kDefaultDriveOnePan / ...TwoPan.
    static constexpr float kDefaultDriveOnePan      = -0.5f;
    static constexpr float kDefaultDriveTwoPan      = +0.5f;
    float              driveMotorVolume      = kDefaultDriveMotorVolume;
    float              driveHeadVolume       = kDefaultDriveHeadVolume;
    float              driveDoorVolume       = kDefaultDriveDoorVolume;
    float              driveOnePan           = kDefaultDriveOnePan;
    float              driveTwoPan           = kDefaultDriveTwoPan;
    // Last-mounted disk paths per drive, stored as exe-relative when
    // possible (PathResolver::MakeExeRelativePath). Empty = no
    // remembered disk. Replaces the per-machine HKCU\...\Disk{1,2}
    // registry values that DiskSettings used to own.
    std::string        diskPath[2];
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


struct SettingsMemoryRegion
{
    std::string  name;          // "Main RAM", "Aux RAM", "System ROM" ...
    std::string  size;           // "48K" / "16K"
    std::string  addressRange;   // "$0000-$BFFF"
};


struct SettingsMachineInfo
{
    std::string                          name;
    std::string                          cpu;
    std::string                          cpuManufacturer;
    uint32_t                             clockSpeed    = 0;
    std::string                          ramSummary;    // "128K RAM" total, ROM excluded
    std::vector<SettingsMemoryRegion>    memoryRegions;
    size_t                               devices       = 0;
    // True for machines whose second drive is an optional add-on rather than
    // fixed hardware (the //c external disk port). Drives the Hardware tab's
    // "External drive" Connected/Not-connected toggle. Detected from a banked
    // system ROM (romBankSize != 0), the //c's defining trait.
    bool                                 supportsExternalDrive = false;
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
    virtual void ApplyDriveVolumes   (float motor, float head, float door) = 0;
    virtual void ApplyDrivePan       (float driveOnePan, float driveTwoPan) = 0;
    virtual void ApplyWriteProtect   (int drive, bool wp)            = 0;
    virtual void ApplyExternalDriveConnected (bool connected)        = 0;
    virtual void ApplyMouseConnected         (bool connected)        = 0;
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

    const std::string                & MachineName () const { return m_machineName; }
    const SettingsMachineInfo        & MachineInfo() const { return m_machineInfo; }
    const SettingsUiPrefs            & Prefs       () const { return m_current.prefs; }
    const std::vector<HardwareEntry> & Hardware    () const { return m_current.hardware; }
    const JsonValue                  & DefaultJson () const { return m_defaultJson; }

    // True when the (staged) hardware config includes an enabled Disk ][
    // controller (a slot whose device is "disk-ii"). Drives the settings
    // sheet's dynamic Disk tab (#84): no enabled controller -> no Disk tab.
    // The //c's drive is a BUILT-IN IWM, not a config slot, so it never
    // appears in the hardware list — a banked system ROM (the //c's
    // defining trait, the same signal that creates the IWM) counts as a
    // controller here, or the //c would wrongly lose its Disk tab.
    bool HasDiskIIController () const
    {
        if (m_machineInfo.supportsExternalDrive)   // banked ROM -> built-in IWM
        {
            return true;
        }
        for (const HardwareEntry & e : m_current.hardware)
        {
            if (e.type == "disk-ii" && e.enabled) { return true; }
        }
        return false;
    }

    void    SetSpeedMode       (SettingsSpeedMode mode);
    void    SetColorMode       (SettingsColorMode mode);
    void    SetWriteMode       (SettingsWriteMode mode);
    void    SetFloppySound     (bool enabled);
    void    SetMechanism       (const std::string & mechanism);
    void    SetDriveMotorVolume (float gain);
    void    SetDriveHeadVolume  (float gain);
    void    SetDriveDoorVolume  (float gain);
    void    SetDriveOnePan      (float pan);
    void    SetDriveTwoPan      (float pan);
    void    SetWriteProtect    (int drive, bool wp);
    void    SetExternalDriveConnected (bool connected);
    void    SetMouseConnected         (bool connected);
    HRESULT SetHardwareEnabled (size_t index, bool enabled);

    // ---- Apply ---------------------------------------------------------

    HRESULT Apply (ISettingsApplySink & sink,
                   JsonValue          & outCurrentJson) const;

    // ---- Pure helpers (exposed for tests) ------------------------------

    static HRESULT ExtractHardware    (const JsonValue            & mergedJson,
                                       std::vector<HardwareEntry> & outEntries);
    static HRESULT ExtractMachineInfo (const JsonValue            & mergedJson,
                                       SettingsMachineInfo        & outInfo);
    static HRESULT ExtractUiPrefs     (const JsonValue            & mergedJson,
                                       SettingsUiPrefs            & outPrefs);
    static JsonValue BuildJson        (const JsonValue                 & mergedJson,
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


    std::string          m_machineName;
    SettingsMachineInfo  m_machineInfo;
    JsonValue            m_defaultJson;
    JsonValue            m_mergedJson;

    Snapshot     m_original;
    Snapshot     m_current;
};
