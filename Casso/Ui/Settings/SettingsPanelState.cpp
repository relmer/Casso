#include "Pch.h"

#include "SettingsPanelState.h"


#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"






////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr const char *  s_kpszUiPrefsKey      = "$cassoUiPrefs";
    constexpr const char *  s_kpszVersionKey      = "$cassoMachineVersion";


    int  FindKey (
        const std::vector<std::pair<std::string, JsonValue>> & entries,
        const std::string                                    & key)
    {
        int  i = 0;
        for (i = 0; i < (int) entries.size(); ++i)
        {
            if (entries[(size_t) i].first == key)
            {
                return i;
            }
        }
        return -1;
    }


    bool  GetBoolOpt (
        const JsonValue   & obj,
        const std::string & key,
        bool                fallback)
    {
        bool      out = fallback;
        HRESULT   hr  = obj.GetBool (key, out);
        if (FAILED (hr))
        {
            return fallback;
        }
        return out;
    }


    std::string  GetStringOpt (
        const JsonValue   & obj,
        const std::string & key,
        const std::string & fallback)
    {
        std::string  out = fallback;
        HRESULT      hr  = obj.GetString (key, out);
        if (FAILED (hr))
        {
            return fallback;
        }
        return out;
    }


    int  GetIntOpt (
        const JsonValue   & obj,
        const std::string & key,
        int                 fallback)
    {
        int      out = fallback;
        HRESULT  hr  = obj.GetInt (key, out);
        if (FAILED (hr))
        {
            return fallback;
        }
        return out;
    }


    double  GetNumberOpt (
        const JsonValue   & obj,
        const std::string & key,
        double              fallback)
    {
        double   out = fallback;
        HRESULT  hr  = obj.GetNumber (key, out);
        if (FAILED (hr))
        {
            return fallback;
        }
        return out;
    }


    CapabilityFlag  ParseCapability (
        const std::string & str,
        CapabilityFlag      fallback)
    {
        if (str == "optional")        return CapabilityFlag::Optional;
        if (str == "required")        return CapabilityFlag::Required;
        if (str == "platform-locked") return CapabilityFlag::PlatformLocked;
        return fallback;
    }


    [[maybe_unused]] const char *  CapabilityToString (CapabilityFlag c)
    {
        switch (c)
        {
            case CapabilityFlag::Optional:       return "optional";
            case CapabilityFlag::Required:       return "required";
            case CapabilityFlag::PlatformLocked: return "platform-locked";
        }
        return "optional";
    }


    const char *  SpeedToString (SettingsSpeedMode s)
    {
        switch (s)
        {
            case SettingsSpeedMode::Authentic: return "authentic";
            case SettingsSpeedMode::Double:    return "double";
            case SettingsSpeedMode::Maximum:   return "maximum";
        }
        return "authentic";
    }


    SettingsSpeedMode  SpeedFromString (
        const std::string & s,
        SettingsSpeedMode   fallback)
    {
        if (s == "authentic") return SettingsSpeedMode::Authentic;
        if (s == "double")    return SettingsSpeedMode::Double;
        if (s == "maximum")   return SettingsSpeedMode::Maximum;
        return fallback;
    }


    const char *  ColorToString (SettingsColorMode c)
    {
        switch (c)
        {
            case SettingsColorMode::Color: return "color";
            case SettingsColorMode::Green: return "green";
            case SettingsColorMode::Amber: return "amber";
            case SettingsColorMode::White: return "white";
        }
        return "color";
    }


    SettingsColorMode  ColorFromString (
        const std::string & s,
        SettingsColorMode   fallback)
    {
        if (s == "color") return SettingsColorMode::Color;
        if (s == "green") return SettingsColorMode::Green;
        if (s == "amber") return SettingsColorMode::Amber;
        if (s == "white") return SettingsColorMode::White;
        return fallback;
    }


    const char *  WriteModeToString (SettingsWriteMode mode)
    {
        switch (mode)
        {
            case SettingsWriteMode::BufferAndFlush: return "buffer-and-flush";
            case SettingsWriteMode::CopyOnWrite:    return "copy-on-write";
        }
        return "buffer-and-flush";
    }


    SettingsWriteMode  WriteModeFromString (
        const std::string & s,
        SettingsWriteMode  fallback)
    {
        if (s == "buffer-and-flush") return SettingsWriteMode::BufferAndFlush;
        if (s == "copy-on-write")    return SettingsWriteMode::CopyOnWrite;
        return fallback;
    }


    // Deep-copy a JsonValue by writing+re-parsing. Cheap enough for the
    // settings panel snapshot (one-time at Show()) and avoids needing
    // a public clone API on JsonValue.
    JsonValue  CloneJson (const JsonValue & v)
    {
        std::string          text;
        JsonWriter::Options  opts;
        JsonParseError       err;
        JsonValue            out;

        opts.fPretty = false;

        if (FAILED (JsonWriter::Write (v, opts, text)))
        {
            return JsonValue();
        }
        if (FAILED (JsonParser::Parse (text, out, err)))
        {
            return JsonValue();
        }
        return out;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanelState
//
////////////////////////////////////////////////////////////////////////////////

SettingsPanelState::SettingsPanelState()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadFromMachine
//
//  Reload the snapshot from a freshly-merged machine config.
//  `mergedJson` is the result of `UserConfigStore::Load` (or
//  equivalently `MergeJson (defaultJson, userJson)`). `defaultJson`
//  is the unmerged embedded default for the machine; it is kept so
//  `Apply` can diff against it for `SaveDelta`. Both must be JSON
//  objects -- anything else returns E_INVALIDARG.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanelState::LoadFromMachine (
    const std::string  & machineName,
    const JsonValue    & defaultJson,
    const JsonValue    & mergedJson)
{
    HRESULT  hr = S_OK;


    CBR (defaultJson.GetType() == JsonType::Object);
    CBR (mergedJson .GetType() == JsonType::Object);

    m_machineName = machineName;
    m_defaultJson = CloneJson (defaultJson);
    m_mergedJson  = CloneJson (mergedJson);

    m_original = Snapshot {};

    hr = ExtractUiPrefs (mergedJson, m_original.prefs);
    CHR (hr);

    hr = ExtractMachineInfo (mergedJson, m_machineInfo);
    CHR (hr);

    hr = ExtractHardware (mergedJson, m_original.hardware);
    CHR (hr);

    m_current = m_original;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Cancel
//
//  Reset `current` to match `original` (used by the Cancel button
//  and implicitly by `LoadFromMachine`).
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::Cancel()
{
    m_current = m_original;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsDirty
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsPanelState::IsDirty() const
{
    if (! PrefsEqual (m_original.prefs, m_current.prefs))
    {
        return true;
    }
    if (! HardwareEqual (m_original.hardware, m_current.hardware))
    {
        return true;
    }
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RequiresReset
//
//  Per FR-010: a reset is required iff the hardware-enable bits
//  changed. Speed / color / sound / mechanism / write-protect are
//  all live-applicable.
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsPanelState::RequiresReset() const
{
    size_t  i = 0;



    if (m_original.hardware.size() != m_current.hardware.size())
    {
        return true;
    }
    for (i = 0; i < m_current.hardware.size(); ++i)
    {
        if (m_original.hardware[i].enabled != m_current.hardware[i].enabled)
        {
            return true;
        }
    }
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Mutators
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetSpeedMode (SettingsSpeedMode mode)
{
    m_current.prefs.speedMode = mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetColorMode
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetColorMode (SettingsColorMode mode)
{
    m_current.prefs.colorMode = mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetWriteMode
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetWriteMode (SettingsWriteMode mode)
{
    m_current.prefs.writeMode = mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFloppySound
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetFloppySound (bool enabled)
{
    m_current.prefs.floppySoundEnabled = enabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMechanism
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetMechanism (const std::string & mechanism)
{
    m_current.prefs.floppyMechanism = mechanism;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDriveMotorVolume / SetDriveHeadVolume / SetDriveDoorVolume
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetDriveMotorVolume (float gain)
{
    m_current.prefs.driveMotorVolume = gain;
}





void SettingsPanelState::SetDriveHeadVolume (float gain)
{
    m_current.prefs.driveHeadVolume = gain;
}





void SettingsPanelState::SetDriveDoorVolume (float gain)
{
    m_current.prefs.driveDoorVolume = gain;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDriveOnePan / SetDriveTwoPan
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetDriveOnePan (float pan)
{
    m_current.prefs.driveOnePan = pan;
}





void SettingsPanelState::SetDriveTwoPan (float pan)
{
    m_current.prefs.driveTwoPan = pan;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetWriteProtect
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetWriteProtect (int drive, bool wp)
{
    if (drive < 0 || drive >= 2)
    {
        return;
    }
    m_current.prefs.writeProtect[drive] = wp;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetExternalDriveConnected
//
//  //c external-drive port toggle. A live-effect UI pref -- it only
//  reveals/hides the second drive-mount widget, so unlike a hardware
//  enable it never sets RequiresReset.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetExternalDriveConnected (bool connected)
{
    m_current.prefs.externalDriveConnected = connected;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetHardwareEnabled
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanelState::SetHardwareEnabled (size_t index, bool enabled)
{
    HRESULT  hr = S_OK;



    CBR (index < m_current.hardware.size());

    if (! enabled)
    {
        // FR-007 / FR-008: required and platform-locked entries
        // cannot be turned off. User input -- non-asserting.
        CBR (m_current.hardware[index].capability == CapabilityFlag::Optional);
    }

    m_current.hardware[index].enabled = enabled;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Apply
//
//  Pushes the live-applicable diffs through `sink` and emits the
//  updated machine JSON ready for `UserConfigStore::SaveDelta`.
//  Always emits `outCurrentJson` (even when nothing changed) so
//  callers can drop the result straight into SaveDelta.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanelState::Apply (
    ISettingsApplySink & sink,
    JsonValue          & outCurrentJson) const
{
    HRESULT  hr = S_OK;
    int      i  = 0;


    // Live-effect fields (FR-011 -- always pushed; cheap, idempotent).
    sink.ApplySpeedMode   (m_current.prefs.speedMode);
    sink.ApplyColorMode   (m_current.prefs.colorMode);
    sink.ApplyFloppySound (m_current.prefs.floppySoundEnabled);
    sink.ApplyMechanism   (m_current.prefs.floppyMechanism);
    sink.ApplyDriveVolumes (m_current.prefs.driveMotorVolume,
                            m_current.prefs.driveHeadVolume,
                            m_current.prefs.driveDoorVolume);
    sink.ApplyDrivePan     (m_current.prefs.driveOnePan,
                            m_current.prefs.driveTwoPan);
    for (i = 0; i < 2; ++i)
    {
        sink.ApplyWriteProtect (i, m_current.prefs.writeProtect[i]);
    }
    sink.ApplyExternalDriveConnected (m_current.prefs.externalDriveConnected);

    // FR-010: any hardware enable diff requires the caller to confirm
    // and the machine to be reset. Queue the reset request; the
    // production sink (EmulatorShell) gates it behind a modal confirm.
    if (RequiresReset())
    {
        sink.QueueMachineReset();
    }

    outCurrentJson = BuildJson (m_mergedJson, m_current.hardware, m_current.prefs);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExtractUiPrefs
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanelState::ExtractUiPrefs (
    const JsonValue   & mergedJson,
    SettingsUiPrefs   & outPrefs)
{
    HRESULT             hr     = S_OK;
    const JsonValue *   uiObj  = nullptr;
    const JsonValue *   wpArr  = nullptr;
    size_t              i      = 0;


    CBR (mergedJson.GetType() == JsonType::Object);

    outPrefs = SettingsUiPrefs {};

    hr = mergedJson.GetObject (s_kpszUiPrefsKey, uiObj);
    if (FAILED (hr) || uiObj == nullptr)
    {
        // No $cassoUiPrefs in the file -- struct defaults stand.
        return S_OK;
    }
    _Analysis_assume_ (uiObj != nullptr);

    outPrefs.speedMode = SpeedFromString (
        GetStringOpt (*uiObj, "speedMode", "authentic"),
        SettingsSpeedMode::Authentic);

    outPrefs.colorMode = ColorFromString (
        GetStringOpt (*uiObj, "colorMode", "color"),
        SettingsColorMode::Color);

    outPrefs.writeMode = WriteModeFromString (
        GetStringOpt (*uiObj, "writeMode", "buffer-and-flush"),
        SettingsWriteMode::BufferAndFlush);

    outPrefs.floppySoundEnabled = GetBoolOpt   (*uiObj, "floppySoundEnabled", true);
    outPrefs.floppyMechanism    = GetStringOpt (*uiObj, "floppyMechanism",    "shugart");

    outPrefs.externalDriveConnected = GetBoolOpt (*uiObj, "externalDriveConnected", false);

    outPrefs.driveMotorVolume = (float) GetNumberOpt (*uiObj, "driveMotorVolume", SettingsUiPrefs::kDefaultDriveMotorVolume);
    outPrefs.driveHeadVolume  = (float) GetNumberOpt (*uiObj, "driveHeadVolume",  SettingsUiPrefs::kDefaultDriveHeadVolume);
    outPrefs.driveDoorVolume  = (float) GetNumberOpt (*uiObj, "driveDoorVolume",  SettingsUiPrefs::kDefaultDriveDoorVolume);
    outPrefs.driveOnePan      = (float) GetNumberOpt (*uiObj, "driveOnePan",      SettingsUiPrefs::kDefaultDriveOnePan);
    outPrefs.driveTwoPan      = (float) GetNumberOpt (*uiObj, "driveTwoPan",      SettingsUiPrefs::kDefaultDriveTwoPan);

    outPrefs.diskPath[0] = GetStringOpt (*uiObj, "disk1Path", "");
    outPrefs.diskPath[1] = GetStringOpt (*uiObj, "disk2Path", "");

    if (SUCCEEDED (uiObj->GetArray ("writeProtect", wpArr)) && wpArr != nullptr)
    {
        for (i = 0; i < wpArr->ArraySize() && i < 2; ++i)
        {
            const JsonValue & entry = wpArr->ArrayAt (i);
            if (entry.GetType() == JsonType::Bool)
            {
                outPrefs.writeProtect[i] = entry.GetBool();
            }
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExtractMachineInfo
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanelState::ExtractMachineInfo (
    const JsonValue     & mergedJson,
    SettingsMachineInfo & outInfo)
{
    HRESULT            hr              = S_OK;
    HRESULT            hrRead          = S_OK;
    const JsonValue  * timingObj       = nullptr;
    const JsonValue  * ramArray        = nullptr;
    const JsonValue  * romObj          = nullptr;
    const JsonValue  * internalDevices = nullptr;
    const JsonValue  * slots           = nullptr;
    bool               hasAux          = false;
    uint32_t           totalRamBytes   = 0;

    auto ParseHex = [] (const std::string & str) -> uint32_t
    {
        // Accept "0x" / "0X" / no prefix. Returns 0 on parse failure
        // (which renders harmlessly as an empty/zero region line below).
        size_t        i   = 0;
        uint32_t      out = 0;
        if (str.size() >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
        {
            i = 2;
        }
        for (; i < str.size(); ++i)
        {
            char c = str[i];
            int  d = -1;
            if      (c >= '0' && c <= '9') { d = c - '0';      }
            else if (c >= 'a' && c <= 'f') { d = c - 'a' + 10; }
            else if (c >= 'A' && c <= 'F') { d = c - 'A' + 10; }
            else                            { return 0; }
            out = (out << 4) | (uint32_t) d;
        }
        return out;
    };

    auto FormatSize = [] (uint32_t bytes) -> std::string
    {
        if (bytes == 0)
        {
            return "0";
        }
        if (bytes >= 1024 && (bytes % 1024) == 0)
        {
            return std::format ("{}K", bytes / 1024);
        }
        return std::format ("{}B", bytes);
    };

    auto FormatRegion = [&] (const std::string & label,
                              const std::string & addrStr,
                              const std::string & sizeStr)
    {
        uint32_t  addr = ParseHex (addrStr);
        uint32_t  size = ParseHex (sizeStr);
        if (size == 0)
        {
            return;
        }
        uint32_t              end    = addr + size - 1;
        SettingsMemoryRegion  region;
        region.name         = label;
        region.size         = FormatSize (size);
        region.addressRange = std::format ("${:04X}-${:04X}", addr, end);
        outInfo.memoryRegions.push_back (std::move (region));
    };



    CBR (mergedJson.GetType() == JsonType::Object);

    outInfo = SettingsMachineInfo {};

    hrRead = mergedJson.GetString ("name", outInfo.name);
    if (FAILED (hrRead))
    {
        outInfo.name.clear();
    }

    hrRead = mergedJson.GetString ("cpu", outInfo.cpu);
    if (FAILED (hrRead))
    {
        outInfo.cpu.clear();
    }

    hrRead = mergedJson.GetString ("cpuManufacturer", outInfo.cpuManufacturer);
    if (FAILED (hrRead))
    {
        outInfo.cpuManufacturer.clear();
    }

    hrRead = mergedJson.GetObject ("timing", timingObj);
    if (SUCCEEDED (hrRead) && timingObj != nullptr)
    {
        hrRead = timingObj->GetUint32 ("clockSpeed", outInfo.clockSpeed);
        if (FAILED (hrRead))
        {
            outInfo.clockSpeed = 0;
        }
    }

    hrRead = mergedJson.GetArray ("ram", ramArray);
    if (SUCCEEDED (hrRead) && ramArray != nullptr)
    {
        size_t  i = 0;
        for (i = 0; i < ramArray->ArraySize(); ++i)
        {
            const JsonValue &  entry  = ramArray->ArrayAt (i);
            std::string        addr;
            std::string        size;
            std::string        bank;
            std::string        label;

            if (entry.GetType() != JsonType::Object)
            {
                continue;
            }
            IGNORE_RETURN_VALUE (hrRead, entry.GetString ("address", addr));
            IGNORE_RETURN_VALUE (hrRead, entry.GetString ("size",    size));
            IGNORE_RETURN_VALUE (hrRead, entry.GetString ("bank",    bank));

            if (bank.empty() || bank == "main")
            {
                label = "RAM (main)";
            }
            else
            {
                label   = std::format ("RAM ({})", bank);
                hasAux  = true;
            }
            FormatRegion (label, addr, size);
            totalRamBytes += ParseHex (size);
        }
    }

    hrRead = mergedJson.GetObject ("systemRom", romObj);
    if (SUCCEEDED (hrRead) && romObj != nullptr)
    {
        std::string  addr;
        std::string  size;
        std::string  bankSizeStr;

        IGNORE_RETURN_VALUE (hrRead, romObj->GetString ("address",     addr));
        IGNORE_RETURN_VALUE (hrRead, romObj->GetString ("size",        size));
        IGNORE_RETURN_VALUE (hrRead, romObj->GetString ("romBankSize", bankSizeStr));

        uint32_t  bankSize = ParseHex (bankSizeStr);

        if (bankSize != 0 && ! addr.empty())
        {
            // Banked system ROM (//c): two `romBankSize` banks share one
            // address window, toggled by $C028 -- only one is visible at a
            // time. So the *mapped range* is a single bank span while the
            // *installed* ROM is twice that (32K in a 16K window on the //c).
            // Report the true installed size + the window, and name the row
            // so the size/range mismatch reads as intentional banking.
            uint32_t              startAddr = ParseHex (addr);
            uint32_t              windowEnd = startAddr + bankSize - 1;
            SettingsMemoryRegion  region;
            region.name         = "System ROM (2 banks)";
            region.size         = FormatSize (bankSize * 2);
            region.addressRange = std::format ("${:04X}-${:04X}", startAddr, windowEnd);
            outInfo.memoryRegions.push_back (std::move (region));

            // A banked system ROM is the //c's defining trait; it is also the
            // one machine with an optional external drive (its second drive is
            // an add-on, not fixed hardware). Surface that so the Hardware tab
            // can offer the External-drive Connected/Not-connected toggle.
            outInfo.supportsExternalDrive = true;
        }
        else
        {
            // Flat system ROM (][ / ][+ / //e). Size defaults to fill-to-
            // $FFFF when omitted in the schema; compute end from address.
            if (size.empty() && ! addr.empty())
            {
                uint32_t  startAddr = ParseHex (addr);
                if (startAddr < 0x10000u)
                {
                    size = std::format ("0x{:X}", 0x10000u - startAddr);
                }
            }
            FormatRegion ("System ROM", addr, size);
        }
    }

    hrRead = mergedJson.GetArray ("internalDevices", internalDevices);
    if (SUCCEEDED (hrRead) && internalDevices != nullptr)
    {
        outInfo.devices += internalDevices->ArraySize();

        // A language card adds 16K of bank-switched RAM at $D000-$FFFF per 64K
        // bank ($D000-$DFFF is double-banked, so 16K in a 12K window). The base
        // "ram" entries above only cover $0000-$BFFF, so surface the LC RAM here
        // -- otherwise a 128K //e/​//c reads as only 96K. One region per bank
        // (main, plus aux when the machine has an aux bank).
        bool  hasLanguageCard = false;
        for (size_t d = 0; d < internalDevices->ArraySize(); ++d)
        {
            const JsonValue &  dev = internalDevices->ArrayAt (d);
            std::string        type;

            if (dev.GetType() == JsonType::Object &&
                SUCCEEDED (dev.GetString ("type", type)) &&
                type == "language-card")
            {
                hasLanguageCard = true;
                break;
            }
        }

        if (hasLanguageCard)
        {
            auto addLcRam = [&] (const std::string & label)
            {
                SettingsMemoryRegion  region;
                region.name         = label;
                region.size         = FormatSize (0x4000);   // 16K ($D000 double-banked)
                region.addressRange = "$D000-$FFFF";
                outInfo.memoryRegions.push_back (std::move (region));
                totalRamBytes += 0x4000;
            };

            addLcRam ("RAM (main, bank-switched)");
            if (hasAux)
            {
                addLcRam ("RAM (aux, bank-switched)");
            }
        }
    }

    hrRead = mergedJson.GetArray ("slots", slots);
    if (SUCCEEDED (hrRead) && slots != nullptr)
    {
        outInfo.devices += slots->ArraySize();
    }

    // Headline total: sum every RAM region (main + aux + language-card banks;
    // ROM is excluded) so a 128K //e/​//c reads its full 128K at a glance above
    // the per-region breakdown.
    if (totalRamBytes != 0)
    {
        outInfo.ramSummary = FormatSize (totalRamBytes) + " RAM";
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExtractHardware
//
//  Walks a merged machine JSON object and pulls out the typed
//  hardware-tree representation. Slot/internal-device order is
//  preserved. Default capability per FR-015: internal devices ->
//  Required, slot entries -> Optional. JSON-level overrides
//  (`capabilityFlag` + `lockReason`) win where present.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanelState::ExtractHardware (
    const JsonValue              & mergedJson,
    std::vector<HardwareEntry>   & outEntries)
{
    static const struct { const char * id; const char * display; }  s_kDeviceDisplayNames[] =
    {
        { "disk-ii",                 "Disk ][" },
        { "smartport",               "SmartPort" },
        { "mockingboard",            "Mockingboard" },
        { "passport",                "Passport MIDI" },
        { "serial",                  "Super Serial Card" },
        { "parallel",                "Parallel Printer" },
        { "videx",                   "Videx 80-Column" },
        { "ramworks",                "RamWorks" },
        { "ramfactor",               "RAMFactor" },
        { "saturn128",               "Saturn 128K" },
        { "language-card",           "Language Card" },
        { "extended-80-column",      "Extended 80-Column Card" },
        { "cassette",                "Cassette" },
        { "speaker",                 "Speaker" },
        { "keyboard",                "Keyboard" },
        { "joystick",                "Joystick" },
        { "paddle",                  "Paddle" },
        { "monitor",                 "Monitor" },
        { "apple2-keyboard",         "Keyboard" },
        { "apple2-speaker",          "Speaker" },
        { "apple2-softswitches",     "Soft Switches" },
        // The //e-generation keyboard/soft-switch controllers are shared by the
        // //e and the //c, so the label stays machine-neutral (the machine name
        // is already shown at the top of the panel) rather than hardcoding //e.
        { "apple2e-keyboard",        "Keyboard" },
        { "apple2e-softswitches",    "Soft Switches" },
        { "apple2e-mmu",             "Memory Management Unit" },
    };

    HRESULT             hr       = S_OK;
    const JsonValue *   devArr   = nullptr;
    const JsonValue *   slotArr  = nullptr;
    size_t              i        = 0;
    size_t              j        = 0;


    CBR (mergedJson.GetType() == JsonType::Object);

    outEntries.clear();

    if (SUCCEEDED (mergedJson.GetArray ("internalDevices", devArr)) && devArr != nullptr)
    {
        for (i = 0; i < devArr->ArraySize(); ++i)
        {
            const JsonValue & entry = devArr->ArrayAt (i);
            if (entry.GetType() != JsonType::Object)
            {
                continue;
            }

            HardwareEntry  hw;
            std::string    devType = GetStringOpt (entry, "type", "");
            std::string    friendly = devType;

            for (j = 0; j < sizeof (s_kDeviceDisplayNames) / sizeof (s_kDeviceDisplayNames[0]); ++j)
            {
                if (devType == s_kDeviceDisplayNames[j].id)
                {
                    friendly = s_kDeviceDisplayNames[j].display;
                    break;
                }
            }

            hw.kind        = HardwareEntryKind::InternalDevice;
            hw.jsonIndex   = (int) i;
            hw.slot        = 0;
            hw.type        = devType;
            hw.displayName = friendly;
            hw.capability  = ParseCapability (
                GetStringOpt (entry, "capabilityFlag", ""),
                CapabilityFlag::Required);   // FR-015 default for internal
            hw.lockReason  = GetStringOpt (entry, "lockReason", "");
            hw.enabled     = GetBoolOpt   (entry, "enabled",    true);

            outEntries.push_back (std::move (hw));
        }
    }

    if (SUCCEEDED (mergedJson.GetArray ("slots", slotArr)) && slotArr != nullptr)
    {
        for (i = 0; i < slotArr->ArraySize(); ++i)
        {
            const JsonValue & entry = slotArr->ArrayAt (i);
            if (entry.GetType() != JsonType::Object)
            {
                continue;
            }

            HardwareEntry  hw;
            int            slotNum  = GetIntOpt (entry, "slot", 0);
            std::string    dev      = GetStringOpt (entry, "device", "");
            std::string    devNice  = dev;

            for (j = 0; j < sizeof (s_kDeviceDisplayNames) / sizeof (s_kDeviceDisplayNames[0]); ++j)
            {
                if (dev == s_kDeviceDisplayNames[j].id)
                {
                    devNice = s_kDeviceDisplayNames[j].display;
                    break;
                }
            }

            hw.kind        = HardwareEntryKind::Slot;
            hw.jsonIndex   = (int) i;
            hw.slot        = slotNum;
            hw.type        = dev;
            hw.displayName = std::string ("Slot ") + std::to_string (slotNum)
                           + ": " + (dev.empty() ? std::string ("(rom only)") : devNice);
            hw.capability  = ParseCapability (
                GetStringOpt (entry, "capabilityFlag", ""),
                CapabilityFlag::Optional);   // FR-015 default for slots
            hw.lockReason  = GetStringOpt (entry, "lockReason", "");
            hw.enabled     = GetBoolOpt   (entry, "enabled",    true);

            outEntries.push_back (std::move (hw));
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildJson
//
//  Returns a brand-new object with:
//
//      * every key from `mergedJson` preserved verbatim
//        EXCEPT internalDevices / slots / $cassoUiPrefs
//      * internalDevices / slots rebuilt from the matching entries of
//        `mergedJson` with the `enabled` bit overlaid from `hw`
//      * $cassoUiPrefs emitted from `prefs`
//
////////////////////////////////////////////////////////////////////////////////

JsonValue SettingsPanelState::BuildJson (
    const JsonValue                       & mergedJson,
    const std::vector<HardwareEntry>      & hw,
    const SettingsUiPrefs                 & prefs)
{
    std::vector<std::pair<std::string, JsonValue>>  root;
    std::vector<std::pair<std::string, JsonValue>>  uiObj;
    std::vector<JsonValue>                          wpArr;
    std::vector<JsonValue>                          devArr;
    std::vector<JsonValue>                          slotArr;
    const std::vector<std::pair<std::string, JsonValue>> *  entries = nullptr;
    size_t                                          i = 0;


    if (mergedJson.GetType() != JsonType::Object)
    {
        return JsonValue();
    }

    entries = &mergedJson.GetObjectEntries();

    for (i = 0; i < entries->size(); ++i)
    {
        const std::string & key = (*entries)[i].first;
        const JsonValue   & val = (*entries)[i].second;

        if (key == "internalDevices" || key == "slots" || key == s_kpszUiPrefsKey)
        {
            continue;
        }
        root.emplace_back (key, CloneJson (val));
    }

    // Rebuild internalDevices preserving original per-entry JSON
    // (so unrelated fields like "type" / "lockReason" round-trip).
    if (mergedJson.GetType() == JsonType::Object)
    {
        const JsonValue *  devSrc  = nullptr;
        const JsonValue *  slotSrc = nullptr;

        if (SUCCEEDED (mergedJson.GetArray ("internalDevices", devSrc)) && devSrc != nullptr)
        {
            for (i = 0; i < devSrc->ArraySize(); ++i)
            {
                const JsonValue & src = devSrc->ArrayAt (i);
                if (src.GetType() != JsonType::Object)
                {
                    devArr.push_back (CloneJson (src));
                    continue;
                }

                // Find matching hw entry by (kind, jsonIndex).
                bool   found        = false;
                bool   enabledFlag  = true;
                size_t h            = 0;
                for (h = 0; h < hw.size(); ++h)
                {
                    if (hw[h].kind == HardwareEntryKind::InternalDevice &&
                        hw[h].jsonIndex == (int) i)
                    {
                        enabledFlag = hw[h].enabled;
                        found       = true;
                        break;
                    }
                }
                (void) found;

                std::vector<std::pair<std::string, JsonValue>>  rebuilt;
                const auto &  srcEntries = src.GetObjectEntries();
                size_t        j          = 0;
                for (j = 0; j < srcEntries.size(); ++j)
                {
                    if (srcEntries[j].first == "enabled")
                    {
                        continue;
                    }
                    rebuilt.emplace_back (srcEntries[j].first, CloneJson (srcEntries[j].second));
                }
                rebuilt.emplace_back ("enabled", JsonValue (enabledFlag));
                devArr.emplace_back (JsonValue (std::move (rebuilt)));
            }
        }

        if (SUCCEEDED (mergedJson.GetArray ("slots", slotSrc)) && slotSrc != nullptr)
        {
            for (i = 0; i < slotSrc->ArraySize(); ++i)
            {
                const JsonValue & src = slotSrc->ArrayAt (i);
                if (src.GetType() != JsonType::Object)
                {
                    slotArr.push_back (CloneJson (src));
                    continue;
                }

                bool   enabledFlag = true;
                size_t h           = 0;
                for (h = 0; h < hw.size(); ++h)
                {
                    if (hw[h].kind == HardwareEntryKind::Slot &&
                        hw[h].jsonIndex == (int) i)
                    {
                        enabledFlag = hw[h].enabled;
                        break;
                    }
                }

                std::vector<std::pair<std::string, JsonValue>>  rebuilt;
                const auto &  srcEntries = src.GetObjectEntries();
                size_t        j          = 0;
                for (j = 0; j < srcEntries.size(); ++j)
                {
                    if (srcEntries[j].first == "enabled")
                    {
                        continue;
                    }
                    rebuilt.emplace_back (srcEntries[j].first, CloneJson (srcEntries[j].second));
                }
                rebuilt.emplace_back ("enabled", JsonValue (enabledFlag));
                slotArr.emplace_back (JsonValue (std::move (rebuilt)));
            }
        }
    }

    if (! devArr.empty())
    {
        root.emplace_back ("internalDevices", JsonValue (std::move (devArr)));
    }
    if (! slotArr.empty())
    {
        root.emplace_back ("slots", JsonValue (std::move (slotArr)));
    }

    // $cassoUiPrefs block
    uiObj.emplace_back ("speedMode",          JsonValue (std::string (SpeedToString (prefs.speedMode))));
    uiObj.emplace_back ("colorMode",          JsonValue (std::string (ColorToString (prefs.colorMode))));
    uiObj.emplace_back ("writeMode",          JsonValue (std::string (WriteModeToString (prefs.writeMode))));
    uiObj.emplace_back ("floppySoundEnabled", JsonValue (prefs.floppySoundEnabled));
    uiObj.emplace_back ("floppyMechanism",    JsonValue (prefs.floppyMechanism));
    uiObj.emplace_back ("externalDriveConnected", JsonValue (prefs.externalDriveConnected));
    uiObj.emplace_back ("driveMotorVolume",   JsonValue ((double) prefs.driveMotorVolume));
    uiObj.emplace_back ("driveHeadVolume",    JsonValue ((double) prefs.driveHeadVolume));
    uiObj.emplace_back ("driveDoorVolume",    JsonValue ((double) prefs.driveDoorVolume));
    uiObj.emplace_back ("driveOnePan",        JsonValue ((double) prefs.driveOnePan));
    uiObj.emplace_back ("driveTwoPan",        JsonValue ((double) prefs.driveTwoPan));
    uiObj.emplace_back ("disk1Path",          JsonValue (prefs.diskPath[0]));
    uiObj.emplace_back ("disk2Path",          JsonValue (prefs.diskPath[1]));

    wpArr.emplace_back (JsonValue (prefs.writeProtect[0]));
    wpArr.emplace_back (JsonValue (prefs.writeProtect[1]));
    uiObj.emplace_back ("writeProtect", JsonValue (std::move (wpArr)));

    root.emplace_back (s_kpszUiPrefsKey, JsonValue (std::move (uiObj)));

    return JsonValue (std::move (root));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrefsEqual
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsPanelState::PrefsEqual (
    const SettingsUiPrefs & a,
    const SettingsUiPrefs & b)
{
    if (a.speedMode             != b.speedMode)             return false;
    if (a.colorMode             != b.colorMode)             return false;
    if (a.writeMode             != b.writeMode)             return false;
    if (a.floppySoundEnabled    != b.floppySoundEnabled)    return false;
    if (a.floppyMechanism       != b.floppyMechanism)       return false;
    if (a.externalDriveConnected != b.externalDriveConnected) return false;
    if (a.driveMotorVolume      != b.driveMotorVolume)       return false;
    if (a.driveHeadVolume       != b.driveHeadVolume)        return false;
    if (a.driveDoorVolume       != b.driveDoorVolume)        return false;
    if (a.driveOnePan           != b.driveOnePan)            return false;
    if (a.driveTwoPan           != b.driveTwoPan)            return false;
    if (a.diskPath[0]           != b.diskPath[0])           return false;
    if (a.diskPath[1]           != b.diskPath[1])           return false;
    if (a.writeProtect[0]       != b.writeProtect[0])       return false;
    if (a.writeProtect[1]       != b.writeProtect[1])       return false;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwareEqual
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsPanelState::HardwareEqual (
    const std::vector<HardwareEntry> & a,
    const std::vector<HardwareEntry> & b)
{
    size_t  i = 0;



    if (a.size() != b.size())
    {
        return false;
    }
    for (i = 0; i < a.size(); ++i)
    {
        if (a[i].kind       != b[i].kind)       return false;
        if (a[i].jsonIndex  != b[i].jsonIndex)  return false;
        if (a[i].slot       != b[i].slot)       return false;
        if (a[i].type       != b[i].type)       return false;
        if (a[i].capability != b[i].capability) return false;
        if (a[i].enabled    != b[i].enabled)    return false;
    }
    return true;
}
