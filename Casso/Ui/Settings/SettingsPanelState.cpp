#include "Pch.h"

#include "SettingsPanelState.h"


#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

int  SettingsPanelState::FindKey (
    const std::vector<std::pair<std::string, JsonValue>> & entries,
    const std::string                                    & key)
{
    int  i     = 0;
    int  found = -1;



    for (i = 0; i < (int) entries.size() && found < 0; ++i)
    {
        if (entries[(size_t) i].first == key)
        {
            found = i;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanelState::TryGetBoolOpt
//
//  The four Get*Opt helpers share one contract: a failed read restores the
//  fallback, because the getter may have written to `out` before failing.
//
////////////////////////////////////////////////////////////////////////////////

bool  SettingsPanelState::TryGetBoolOpt (
    const JsonValue   & obj,
    const std::string & key,
    bool                fallback)
{
    bool      out = fallback;
    HRESULT   hr  = obj.GetBool (key, out);



    if (FAILED (hr))
    {
        out = fallback;
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanelState::GetStringOpt
//
////////////////////////////////////////////////////////////////////////////////

std::string  SettingsPanelState::GetStringOpt (
    const JsonValue   & obj,
    const std::string & key,
    const std::string & fallback)
{
    std::string  out = fallback;
    HRESULT      hr  = obj.GetString (key, out);



    if (FAILED (hr))
    {
        out = fallback;
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanelState::GetIntOpt
//
////////////////////////////////////////////////////////////////////////////////

int  SettingsPanelState::GetIntOpt (
    const JsonValue   & obj,
    const std::string & key,
    int                 fallback)
{
    int      out = fallback;
    HRESULT  hr  = obj.GetInt (key, out);



    if (FAILED (hr))
    {
        out = fallback;
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanelState::GetNumberOpt
//
////////////////////////////////////////////////////////////////////////////////

double  SettingsPanelState::GetNumberOpt (
    const JsonValue   & obj,
    const std::string & key,
    double              fallback)
{
    double   out = fallback;
    HRESULT  hr  = obj.GetNumber (key, out);



    if (FAILED (hr))
    {
        out = fallback;
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanelState::ParseCapability
//
////////////////////////////////////////////////////////////////////////////////

CapabilityFlag  SettingsPanelState::ParseCapability (
    const std::string & str,
    CapabilityFlag      fallback)
{
    CapabilityFlag  flag = fallback;



    if      (str == "optional")        { flag = CapabilityFlag::Optional;       }
    else if (str == "required")        { flag = CapabilityFlag::Required;       }
    else if (str == "platform-locked") { flag = CapabilityFlag::PlatformLocked; }

    return flag;
}


[[maybe_unused]] const char *  CapabilityToString (CapabilityFlag c)
{
    // Also the answer for a value outside the enum.
    const char *  text = "optional";


    switch (c)
    {
        case CapabilityFlag::Optional:       text = "optional";        break;
        case CapabilityFlag::Required:       text = "required";        break;
        case CapabilityFlag::PlatformLocked: text = "platform-locked"; break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SpeedToString
//
////////////////////////////////////////////////////////////////////////////////

const char *  SettingsPanelState::SpeedToString (SettingsSpeedMode s)
{
    // Also the answer for a value outside the enum.
    const char *  text = "authentic";



    switch (s)
    {
        case SettingsSpeedMode::Authentic: text = "authentic"; break;
        case SettingsSpeedMode::Double:    text = "double";    break;
        case SettingsSpeedMode::Maximum:   text = "maximum";   break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanelState::SpeedFromString
//
////////////////////////////////////////////////////////////////////////////////

SettingsSpeedMode  SettingsPanelState::SpeedFromString (
    const std::string & s,
    SettingsSpeedMode   fallback)
{
    SettingsSpeedMode  mode = fallback;



    if      (s == "authentic") { mode = SettingsSpeedMode::Authentic; }
    else if (s == "double")    { mode = SettingsSpeedMode::Double;    }
    else if (s == "maximum")   { mode = SettingsSpeedMode::Maximum;   }

    return mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorToString
//
////////////////////////////////////////////////////////////////////////////////

const char *  SettingsPanelState::ColorToString (SettingsColorMode c)
{
    // Also the answer for a value outside the enum.
    const char *  text = "color";



    switch (c)
    {
        case SettingsColorMode::Color: text = "color"; break;
        case SettingsColorMode::Green: text = "green"; break;
        case SettingsColorMode::Amber: text = "amber"; break;
        case SettingsColorMode::White: text = "white"; break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanelState::ColorFromString
//
////////////////////////////////////////////////////////////////////////////////

SettingsColorMode  SettingsPanelState::ColorFromString (
    const std::string & s,
    SettingsColorMode   fallback)
{
    SettingsColorMode  mode = fallback;



    if      (s == "color") { mode = SettingsColorMode::Color; }
    else if (s == "green") { mode = SettingsColorMode::Green; }
    else if (s == "amber") { mode = SettingsColorMode::Amber; }
    else if (s == "white") { mode = SettingsColorMode::White; }

    return mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteModeToString
//
////////////////////////////////////////////////////////////////////////////////

const char *  SettingsPanelState::WriteModeToString (SettingsWriteMode mode)
{
    // Also the answer for a value outside the enum.
    const char *  text = "buffer-and-flush";



    switch (mode)
    {
        case SettingsWriteMode::BufferAndFlush: text = "buffer-and-flush"; break;
        case SettingsWriteMode::CopyOnWrite:    text = "copy-on-write";    break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanelState::WriteModeFromString
//
////////////////////////////////////////////////////////////////////////////////

SettingsWriteMode  SettingsPanelState::WriteModeFromString (
    const std::string & s,
    SettingsWriteMode  fallback)
{
    SettingsWriteMode  mode = fallback;



    if      (s == "buffer-and-flush") { mode = SettingsWriteMode::BufferAndFlush; }
    else if (s == "copy-on-write")    { mode = SettingsWriteMode::CopyOnWrite;    }

    return mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CloneJson
//
//  Deep-copy a JsonValue by writing+re-parsing. Cheap enough for the
//  settings panel snapshot (one-time at Show()) and avoids needing
//  a public clone API on JsonValue.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue  SettingsPanelState::CloneJson (const JsonValue & v)
{
    std::string          text;
    JsonWriter::Options  opts;
    JsonParseError       err;
    JsonValue            out;
    HRESULT              hrWrite = S_OK;
    HRESULT              hrParse = S_OK;



    opts.fPretty = false;

    hrWrite = JsonWriter::Write (v, opts, text);
    hrParse = SUCCEEDED (hrWrite) ? JsonParser::Parse (text, out, err) : hrWrite;

    // A round trip that fails anywhere yields an empty value, never a
    // half-populated one.
    if (FAILED (hrParse))
    {
        out = JsonValue();
    }

    return out;
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
    HRESULT   hr              = S_OK;
    JsonType  defaultRootType = JsonType::Null;
    JsonType  mergedRootType  = JsonType::Null;



    defaultRootType = defaultJson.GetType();
    mergedRootType  = mergedJson.GetType();

    CBR (defaultRootType == JsonType::Object);
    CBR (mergedRootType  == JsonType::Object);

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
    return !ArePrefsEqual    (m_original.prefs,    m_current.prefs)
        || !AreHardwareEqual (m_original.hardware, m_current.hardware);
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
    size_t  i       = 0;
    bool    changed = m_original.hardware.size() != m_current.hardware.size();



    for (i = 0; !changed && i < m_current.hardware.size(); ++i)
    {
        changed = m_original.hardware[i].enabled != m_current.hardware[i].enabled;
    }

    return changed;
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





////////////////////////////////////////////////////////////////////////////////
//
//  SetDriveDoorVolume
//
////////////////////////////////////////////////////////////////////////////////

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
//  SetMouseConnected
//
//  //c mouse-port toggle. Live UI pref: never sets RequiresReset.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetMouseConnected (bool connected)
{
    m_current.prefs.mouseConnected = connected;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetHardwareEnabled
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanelState::SetHardwareEnabled (size_t index, bool enabled)
{
    HRESULT  hr            = S_OK;
    size_t   hardwareCount = 0;



    hardwareCount = m_current.hardware.size();
    CBR (index < hardwareCount);

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
    sink.ApplyMouseConnected (m_current.prefs.mouseConnected);

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
//  Reads the $cassoUiPrefs block into the settings panel's own state struct.
//
//  The struct is RESET to defaults before anything is read, so a file that
//  omits a key -- or omits the block entirely -- yields the defaults rather
//  than whatever the previous machine's settings left behind. That matters
//  because this runs again on every machine switch.
//
//  Every field is read optionally with its default supplied inline, which
//  keeps the default and the key adjacent instead of split between here and a
//  constructor where they could drift apart.
//
//  Enum-valued fields go through their FromString helpers with a fallback, so
//  an unrecognized value -- from a newer build or a hand edit -- lands on a
//  valid setting instead of an out-of-range enumerator.
//
//  Kept as a static function over a JsonValue so the projection is testable
//  without a settings sheet or a window.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanelState::ExtractUiPrefs (
    const JsonValue   & mergedJson,
    SettingsUiPrefs   & outPrefs)
{
    HRESULT             hr             = S_OK;
    const JsonValue *   uiObj          = nullptr;
    const JsonValue *   wpArr          = nullptr;
    size_t              i              = 0;
    JsonType            mergedRootType = JsonType::Null;
    bool                hasUiPrefs     = false;



    mergedRootType = mergedJson.GetType();
    CBR (mergedRootType == JsonType::Object);

    outPrefs   = SettingsUiPrefs {};
    hasUiPrefs = mergedJson.HasObject (kpszUiPrefsKey, uiObj);

    // No $cassoUiPrefs in the file -- struct defaults stand.
    BAIL_OUT_IF (!hasUiPrefs, S_OK);

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

    outPrefs.floppySoundEnabled = TryGetBoolOpt   (*uiObj, "floppySoundEnabled",  true);
    outPrefs.floppyMechanism    = GetStringOpt (*uiObj, "floppyMechanism",     "shugart");

    outPrefs.externalDriveConnected = TryGetBoolOpt (*uiObj, "externalDriveConnected", false);
    outPrefs.mouseConnected         = TryGetBoolOpt (*uiObj, "mouseConnected", true);

    outPrefs.driveMotorVolume = (float) GetNumberOpt (*uiObj, "driveMotorVolume", SettingsUiPrefs::kDefaultDriveMotorVolume);
    outPrefs.driveHeadVolume  = (float) GetNumberOpt (*uiObj, "driveHeadVolume",  SettingsUiPrefs::kDefaultDriveHeadVolume);
    outPrefs.driveDoorVolume  = (float) GetNumberOpt (*uiObj, "driveDoorVolume",  SettingsUiPrefs::kDefaultDriveDoorVolume);
    outPrefs.driveOnePan      = (float) GetNumberOpt (*uiObj, "driveOnePan",      SettingsUiPrefs::kDefaultDriveOnePan);
    outPrefs.driveTwoPan      = (float) GetNumberOpt (*uiObj, "driveTwoPan",      SettingsUiPrefs::kDefaultDriveTwoPan);

    outPrefs.diskPath[0] = GetStringOpt (*uiObj, "disk1Path", "");
    outPrefs.diskPath[1] = GetStringOpt (*uiObj, "disk2Path", "");

    if (uiObj->HasArray ("writeProtect", wpArr))
    {
        for (i = 0; i < wpArr->GetArraySize() && i < 2; ++i)
        {
            const JsonValue & entry = wpArr->GetArrayElement (i);
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
//  Builds the read-only hardware summary the Hardware page displays: CPU,
//  clock, memory regions, ROM, and the device inventory.
//
//  It reads the MERGED JSON rather than the built MachineConfig, so the page
//  can describe a machine that is not currently running -- which is what lets
//  the machine dropdown preview a different machine's specs before switching
//  to it.
//
//  That is also why the parsing is local rather than reusing
//  MachineConfigLoader: this needs a tolerant projection for DISPLAY, where a
//  malformed field should render as a blank line, while the loader needs a
//  strict validation that refuses to build a broken machine. Same input, two
//  legitimately different failure policies.
//
//  Hence the local ParseHex returning 0 on failure and FormatSize collapsing
//  to a readable "128K" only when the value divides evenly -- both chosen so
//  odd data degrades into a harmless row instead of an error.
//
//  Regions are summarized rather than listed verbatim: the total is
//  accumulated for the header, and aux memory is detected so the breakdown can
//  say which bank a region belongs to.
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
    JsonType           mergedRootType  = JsonType::Null;



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
        uint32_t              addr   = ParseHex (addrStr);
        uint32_t              size   = ParseHex (sizeStr);
        uint32_t              end    = 0;
        SettingsMemoryRegion  region;
        if (size == 0)
        {
            return;
        }

        end = addr + size - 1;
        region.name         = label;
        region.size         = FormatSize (size);
        region.addressRange = std::format ("${:04X}-${:04X}", addr, end);
        outInfo.memoryRegions.push_back (std::move (region));
    };



    mergedRootType = mergedJson.GetType();
    CBR (mergedRootType == JsonType::Object);

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
        for (i = 0; i < ramArray->GetArraySize(); ++i)
        {
            const JsonValue &  entry  = ramArray->GetArrayElement (i);
            std::string        addr;
            std::string        size;
            std::string        bank;
            std::string        label;

            if (entry.GetType() != JsonType::Object)
            {
                continue;
            }

            hrRead = entry.GetString ("address", addr);
            IGNORE_RETURN_VALUE (hrRead, S_OK);
            hrRead = entry.GetString ("size", size);
            IGNORE_RETURN_VALUE (hrRead, S_OK);
            hrRead = entry.GetString ("bank", bank);
            IGNORE_RETURN_VALUE (hrRead, S_OK);

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
        uint32_t     bankSize    = 0;

        hrRead = romObj->GetString ("address", addr);
        IGNORE_RETURN_VALUE (hrRead, S_OK);
        hrRead = romObj->GetString ("size", size);
        IGNORE_RETURN_VALUE (hrRead, S_OK);
        hrRead = romObj->GetString ("romBankSize", bankSizeStr);
        IGNORE_RETURN_VALUE (hrRead, S_OK);

        bankSize = ParseHex (bankSizeStr);

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
        bool  hasLanguageCard = false;

        outInfo.devices += internalDevices->GetArraySize();

        // A language card adds 16K of bank-switched RAM at $D000-$FFFF per 64K
        // bank ($D000-$DFFF is double-banked, so 16K in a 12K window). The base
        // "ram" entries above only cover $0000-$BFFF, so surface the LC RAM here
        // -- otherwise a 128K //e/​//c reads as only 96K. One region per bank
        // (main, plus aux when the machine has an aux bank).
        for (size_t d = 0; d < internalDevices->GetArraySize(); ++d)
        {
            const JsonValue &  dev = internalDevices->GetArrayElement (d);
            std::string        type;

            if (dev.GetType() == JsonType::Object &&
                dev.HasString ("type", type) &&
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
        outInfo.devices += slots->GetArraySize();
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
    HRESULT            hr             = S_OK;
    const JsonValue  * devArr         = nullptr;
    const JsonValue  * slotArr        = nullptr;
    size_t             i              = 0;
    size_t             j              = 0;
    JsonType           mergedRootType = {};



    static const struct { const char * id; const char * display; }  s_kDeviceDisplayNames[] =
    {
        { "disk-ii",                 "Disk ][" },
        { "smartport",               "SmartPort" },
        { "mockingboard",            "Mockingboard A (sound)" },
        { "mockingboard-c",          "Mockingboard C (sound + speech)" },
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

    mergedRootType = JsonType::Null;


    mergedRootType = mergedJson.GetType();
    CBR (mergedRootType == JsonType::Object);

    outEntries.clear();

    if (mergedJson.HasArray ("internalDevices", devArr))
    {
        for (i = 0; i < devArr->GetArraySize(); ++i)
        {
            const JsonValue  & entry    = devArr->GetArrayElement (i);
            HardwareEntry      hw;
            std::string        friendly;
            if (entry.GetType() != JsonType::Object)
            {
                continue;
            }

            std::string    devType = GetStringOpt (entry, "type", "");
            friendly = devType;

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
            hw.enabled     = TryGetBoolOpt   (entry, "enabled",    true);

            outEntries.push_back (std::move (hw));
        }
    }

    if (mergedJson.HasArray ("slots", slotArr))
    {
        for (i = 0; i < slotArr->GetArraySize(); ++i)
        {
            const JsonValue  & entry   = slotArr->GetArrayElement (i);
            HardwareEntry      hw;
            std::string        devNice;
            if (entry.GetType() != JsonType::Object)
            {
                continue;
            }

            int            slotNum  = GetIntOpt (entry, "slot", 0);
            std::string    dev      = GetStringOpt (entry, "device", "");
            devNice = dev;

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
            hw.enabled     = TryGetBoolOpt   (entry, "enabled",    true);

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
    std::vector<std::pair<std::string, JsonValue>>          root;
    std::vector<std::pair<std::string, JsonValue>>          uiObj;
    std::vector<JsonValue>                                  wpArr;
    std::vector<JsonValue>                                  devArr;
    std::vector<JsonValue>                                  slotArr;
    const std::vector<std::pair<std::string, JsonValue>>  * entries = nullptr;
    HRESULT                                                 hr      = S_OK;
    size_t                                                  i       = 0;
    // Default-constructed, so a non-object input yields a NULL value -- not an
    // empty object, which is what returning JsonValue (std::move (root)) early
    // would have produced.
    JsonValue                                       result;
    JsonType                                        rootType = JsonType::Null;



    rootType = mergedJson.GetType();

    BAIL_OUT_IF (rootType != JsonType::Object, S_OK);

    entries = &mergedJson.GetObjectEntries();

    for (i = 0; i < entries->size(); ++i)
    {
        const std::string & key = (*entries)[i].first;
        const JsonValue   & val = (*entries)[i].second;

        if (key == "internalDevices" || key == "slots" || key == kpszUiPrefsKey)
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

        if (mergedJson.HasArray ("internalDevices", devSrc))
        {
            for (i = 0; i < devSrc->GetArraySize(); ++i)
            {
                // GetObjectEntries is a plain accessor (empty for
                // non-objects), so the binding is safe before the type test.
                const JsonValue                                 & src         = devSrc->GetArrayElement (i);
                const auto                                      & srcEntries  = src.GetObjectEntries();
                bool                                              found       = false;
                bool                                              enabledFlag = false;
                std::vector<std::pair<std::string, JsonValue>>    rebuilt;
                size_t                                            j           = 0;
                if (src.GetType() != JsonType::Object)
                {
                    devArr.push_back (CloneJson (src));
                    continue;
                }

                // Find matching hw entry by (kind, jsonIndex).
                enabledFlag = true;

                for (const HardwareEntry & hwEntry : hw)
                {
                    if (hwEntry.kind == HardwareEntryKind::InternalDevice &&
                        hwEntry.jsonIndex == (int) i)
                    {
                        enabledFlag = hwEntry.enabled;
                        found       = true;
                        break;
                    }
                }

                (void) found;

                for (auto & srcEntry : srcEntries)
                {
                    if (srcEntry.first == "enabled")
                    {
                        continue;
                    }

                    rebuilt.emplace_back (srcEntry.first, CloneJson (srcEntry.second));
                }

                rebuilt.emplace_back ("enabled", JsonValue (enabledFlag));
                devArr.emplace_back (JsonValue (std::move (rebuilt)));
            }
        }

        if (mergedJson.HasArray ("slots", slotSrc))
        {
            for (i = 0; i < slotSrc->GetArraySize(); ++i)
            {
                // Same accessor-before-type-test shape as the
                // internalDevices loop above.
                const JsonValue                                 & src         = slotSrc->GetArrayElement (i);
                const auto                                      & srcEntries  = src.GetObjectEntries();
                bool                                              enabledFlag = false;
                std::vector<std::pair<std::string, JsonValue>>    rebuilt;
                size_t                                            j           = 0;
                if (src.GetType() != JsonType::Object)
                {
                    slotArr.push_back (CloneJson (src));
                    continue;
                }

                enabledFlag = true;

                for (const HardwareEntry & hwEntry : hw)
                {
                    if (hwEntry.kind == HardwareEntryKind::Slot &&
                        hwEntry.jsonIndex == (int) i)
                    {
                        enabledFlag = hwEntry.enabled;
                        break;
                    }
                }

                for (auto & srcEntry : srcEntries)
                {
                    if (srcEntry.first == "enabled")
                    {
                        continue;
                    }

                    rebuilt.emplace_back (srcEntry.first, CloneJson (srcEntry.second));
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
    uiObj.emplace_back ("mouseConnected",         JsonValue (prefs.mouseConnected));
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

    root.emplace_back (kpszUiPrefsKey, JsonValue (std::move (uiObj)));

    result = JsonValue (std::move (root));

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArePrefsEqual
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsPanelState::ArePrefsEqual (
    const SettingsUiPrefs & a,
    const SettingsUiPrefs & b)
{
    return a.speedMode              == b.speedMode
        && a.colorMode              == b.colorMode
        && a.writeMode              == b.writeMode
        && a.floppySoundEnabled     == b.floppySoundEnabled
        && a.floppyMechanism        == b.floppyMechanism
        && a.externalDriveConnected == b.externalDriveConnected
        && a.mouseConnected         == b.mouseConnected
        && a.driveMotorVolume       == b.driveMotorVolume
        && a.driveHeadVolume        == b.driveHeadVolume
        && a.driveDoorVolume        == b.driveDoorVolume
        && a.driveOnePan            == b.driveOnePan
        && a.driveTwoPan            == b.driveTwoPan
        && a.diskPath[0]            == b.diskPath[0]
        && a.diskPath[1]            == b.diskPath[1]
        && a.writeProtect[0]        == b.writeProtect[0]
        && a.writeProtect[1]        == b.writeProtect[1];
}





////////////////////////////////////////////////////////////////////////////////
//
//  AreHardwareEqual
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsPanelState::AreHardwareEqual (
    const std::vector<HardwareEntry> & a,
    const std::vector<HardwareEntry> & b)
{
    size_t  i     = 0;
    bool    equal = a.size() == b.size();



    for (i = 0; equal && i < a.size(); ++i)
    {
        equal = a[i].kind       == b[i].kind
             && a[i].jsonIndex  == b[i].jsonIndex
             && a[i].slot       == b[i].slot
             && a[i].type       == b[i].type
             && a[i].capability == b[i].capability
             && a[i].enabled    == b[i].enabled;
    }

    return equal;
}
