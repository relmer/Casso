#include "Pch.h"

#include "Ui/Settings/SettingsJson.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/SettingsJson.h"
#include "Config/MonitorCatalog.h"
#include "Controllers/ControllerTokens.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Machines/MachineDefinitions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

int  SettingsJson::FindKey (
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
//  SettingsJson::TryGetBoolOpt
//
//  The four Get*Opt helpers share one contract: a failed read restores the
//  fallback, because the getter may have written to `out` before failing.
//
////////////////////////////////////////////////////////////////////////////////

bool  SettingsJson::TryGetBoolOpt (
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
//  SettingsJson::GetStringOpt
//
////////////////////////////////////////////////////////////////////////////////

std::string  SettingsJson::GetStringOpt (
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
//  SettingsJson::GetIntOpt
//
////////////////////////////////////////////////////////////////////////////////

int  SettingsJson::GetIntOpt (
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
//  SettingsJson::GetNumberOpt
//
////////////////////////////////////////////////////////////////////////////////

double  SettingsJson::GetNumberOpt (
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
//  SettingsJson::ParseCapability
//
////////////////////////////////////////////////////////////////////////////////

CapabilityFlag  SettingsJson::ParseCapability (
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

const char *  SettingsJson::SpeedToString (SettingsSpeedMode s)
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
//  SettingsJson::SpeedFromString
//
////////////////////////////////////////////////////////////////////////////////

SettingsSpeedMode  SettingsJson::SpeedFromString (
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

const char *  SettingsJson::ColorToString (SettingsColorMode c)
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
//  SettingsJson::ColorFromString
//
////////////////////////////////////////////////////////////////////////////////

SettingsColorMode  SettingsJson::ColorFromString (
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

const char *  SettingsJson::WriteModeToString (SettingsWriteMode mode)
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
//  SettingsJson::WriteModeFromString
//
////////////////////////////////////////////////////////////////////////////////

SettingsWriteMode  SettingsJson::WriteModeFromString (
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

JsonValue  SettingsJson::CloneJson (const JsonValue & v)
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
//  SettingsJson::AddDefinedDevices
//
//  Puts a shipped machine's internal device list into a working copy of its
//  document.
//
//  The document no longer carries one. What devices a machine has is stated in
//  code, so the file describes only what an owner configures -- what is in each
//  slot, what is on each port, which ROM to load. The hardware tree still has
//  to show the devices, and this is where they arrive, once, rather than every
//  reader learning where a device list comes from.
//
//  A machine with no definition is left untouched. Its document is the only
//  description of it there is, and overwriting that would be the same mistake
//  in the other direction.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsJson::AddDefinedDevices (const std::string & machineName, JsonValue & doc)
{
    const MachineDefinition *                       definition = MachineDefinitions::Find (machineName);
    std::vector<JsonValue>                          devices;
    std::vector<std::pair<std::string, JsonValue>>  entries;



    if (definition == nullptr || doc.GetType() != JsonType::Object)
    {
        return;
    }

    for (const InternalDevice & device : definition->internalDevices)
    {
        std::vector<std::pair<std::string, JsonValue>>  fields;

        fields.emplace_back ("type", JsonValue (device.type));
        devices.emplace_back (std::move (fields));
    }

    //  Rebuilt rather than appended to: a stale internalDevices in a user's
    //  own copy of a shipped document must not survive alongside the real one.
    for (const auto & entry : doc.GetObjectEntries())
    {
        if (entry.first != "internalDevices")
        {
            entries.emplace_back (entry.first, CloneJson (entry.second));
        }
    }

    entries.emplace_back ("internalDevices", JsonValue (std::move (devices)));

    doc = JsonValue (std::move (entries));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPortDevice
//
////////////////////////////////////////////////////////////////////////////////

std::string SettingsJson::GetPortDevice (const std::vector<SettingsMachinePort> & ports, const char * name)
{
    for (const SettingsMachinePort & entry : ports)
    {
        if (entry.name == name)
        {
            return entry.device;
        }
    }

    return std::string();
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

HRESULT SettingsJson::ExtractUiPrefs (
    const JsonValue   & mergedJson,
    SettingsUiPrefs   & outPrefs)
{
    HRESULT             hr             = S_OK;
    const JsonValue *   uiObj          = nullptr;
    const JsonValue *   wpArr          = nullptr;
    size_t              i              = 0;
    JsonType            mergedRootType = JsonType::Null;
    bool                hasUiPrefs     = false;
    SettingsColorMode   monitorColor   = SettingsColorMode::Color;



    mergedRootType = mergedJson.GetType();
    CBR (mergedRootType == JsonType::Object);

    outPrefs   = SettingsUiPrefs {};
    hasUiPrefs = mergedJson.HasObject (SettingsPanelState::kpszUiPrefsKey, uiObj);

    // THE COLOR MODE'S DEFAULT IS THE MONITOR'S PHOSPHOR, not the struct's.
    // Every other field's default is a constant, so the struct can carry it;
    // this one is a property of the tube the machine ships with, which only
    // the merged document knows. Seeded before the guard below so a machine
    // with no saved block at all still opens Settings showing what is
    // actually on screen. It used to open showing Color, and since the sheet
    // applies every page on OK, changing the speed dropped the picture out of
    // green.
    monitorColor       = (SettingsColorMode) MonitorCatalog::PhosphorSettingsIndex (
                             MonitorCatalog::ForMachineJson (mergedJson));
    outPrefs.colorMode = monitorColor;

    // No $cassoUiPrefs in the file -- struct defaults stand.
    BAIL_OUT_IF (!hasUiPrefs, S_OK);

    outPrefs.speedMode = SpeedFromString (
        GetStringOpt (*uiObj, "speedMode", "authentic"),
        SettingsSpeedMode::Authentic);

    outPrefs.colorMode = ColorFromString (
        GetStringOpt (*uiObj, "colorMode", ColorToString (monitorColor)),
        monitorColor);

    outPrefs.writeMode = WriteModeFromString (
        GetStringOpt (*uiObj, "writeMode", "buffer-and-flush"),
        SettingsWriteMode::BufferAndFlush);

    outPrefs.floppySoundEnabled = TryGetBoolOpt   (*uiObj, "floppySoundEnabled",  true);
    outPrefs.floppyMechanism    = GetStringOpt (*uiObj, "floppyMechanism",     "shugart");
    outPrefs.fastTapeLoading    = TryGetBoolOpt   (*uiObj, "fastTapeLoading",     true);
    outPrefs.tapeAutoStop       = TryGetBoolOpt   (*uiObj, "tapeAutoStop",        true);
    outPrefs.tapeIdleStop       = TryGetBoolOpt   (*uiObj, "tapeIdleStop",        true);
    outPrefs.tapeEightBit       = TryGetBoolOpt   (*uiObj, "tapeEightBit",        false);
    outPrefs.tapeRecorderConnected = TryGetBoolOpt (*uiObj, "tapeRecorderConnected", true);
    outPrefs.tapeVolume         = (float) GetNumberOpt (*uiObj, "tapeVolume",   SettingsUiPrefs::kDefaultTapeVolume);

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

HRESULT SettingsJson::ExtractMachineInfo (
    const std::string   & machineId,
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
    bool               hasLanguageCard = false;
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
    else if (MachineDefinitions::Find (machineId) != nullptr)
    {
        // The RAM a machine is born with is declared by the machine, not the
        // JSON: the JSON keeps what a person can change about their machine,
        // and how much RAM a //e has is not one of those things. A "ram"
        // array, when one is present, still wins above, which is how a
        // hand-written test document describes a machine that never shipped.
        for (const RamRegion & region : MachineDefinitions::Find (machineId)->ram)
        {
            std::string  label = "RAM (main)";

            if (!region.bank.empty() && region.bank != "main")
            {
                label  = std::format ("RAM ({})", region.bank);
                hasAux = true;
            }

            FormatRegion (label, std::format ("0x{:X}", region.address), std::format ("0x{:X}", region.size));
            totalRamBytes += region.size;
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

    // A game-port adapter needs the annunciator outputs on the game socket,
    // which is a fact about the machine rather than anything in its document.
    outInfo.supportsGamePortAdapter = MachineDefinitions::Find (machineId) != nullptr &&
                                      MachineDefinitions::Find (machineId)->hasAnnunciators;


    hrRead = mergedJson.GetArray ("internalDevices", internalDevices);
    if (SUCCEEDED (hrRead) && internalDevices != nullptr)
    {
        outInfo.devices += internalDevices->GetArraySize();

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
    }
    else if (MachineDefinitions::Find (machineId) != nullptr)
    {
        // Declared by the machine, as its RAM is.
        for (const InternalDevice & dev : MachineDefinitions::Find (machineId)->internalDevices)
        {
            outInfo.devices++;
            hasLanguageCard = hasLanguageCard || dev.type == "language-card";
        }
    }

    // A language card adds 16K of bank-switched RAM at $D000-$FFFF per 64K
    // bank ($D000-$DFFF is double-banked, so 16K in a 12K window). The base
    // "ram" entries above only cover $0000-$BFFF, so surface the LC RAM here
        // -- otherwise a 128K //e/​//c reads as only 96K. One region per bank
    // (main, plus aux when the machine has an aux bank).
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

HRESULT SettingsJson::ExtractMachinePorts (
    const JsonValue                  & mergedJson,
    std::vector<SettingsMachinePort> & outPorts)
{
    const JsonValue *  portsArr = nullptr;
    size_t             i        = 0;



    outPorts.clear();

    // No root ports is not an error and not an empty back panel -- it is a
    // machine whose hardware is carded, so there is nothing here to write
    // back and BuildJson must leave the key alone entirely.
    if (mergedJson.GetType() != JsonType::Object ||
        !mergedJson.HasArray (SettingsPanelState::kpszPortsKey, portsArr))
    {
        return S_OK;
    }

    // The x86-hosted code analysis the build server runs loses track of the
    // pointer across the || above and reports C6011 without this test.
    if (portsArr == nullptr)
    {
        return S_OK;
    }

    for (i = 0; i < portsArr->GetArraySize(); ++i)
    {
        const JsonValue      & entry = portsArr->GetArrayElement (i);
        SettingsMachinePort    port;

        if (entry.GetType() == JsonType::String)
        {
            port.device = entry.GetString();
        }
        else if (entry.GetType() == JsonType::Object)
        {
            port.name   = GetStringOpt (entry, SettingsPanelState::kpszPortNameKey,   "");
            port.device = GetStringOpt (entry, SettingsPanelState::kpszPortDeviceKey, "");
        }

        outPorts.push_back (std::move (port));
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExtractHardware
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsJson::ExtractHardware (
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
        { SettingsPanelState::kpszNetworkCardDevice,     SettingsPanelState::kpszNetworkCardName },
        { "passport",                "Passport MIDI" },
        { "serial",                  "Super Serial Card" },
        { "super-serial-card",       "Super Serial Card" },
        { "parallel",                "Parallel printer" },
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
        { "apple2-family-keyboard",         "Keyboard" },
        { "apple2-family-speaker",          "Speaker" },
        { "apple2-family-softswitches",     "Soft switches" },
        { "apple2-family-cassette",         "Cassette port" },
        // The //e-generation keyboard/soft-switch controllers are shared by the
        // //e and the //c, so the label stays machine-neutral (the machine name
        // is already shown at the top of the panel) rather than hardcoding //e.
        { "apple2e-family-keyboard",        "Keyboard" },
        { "apple2e-family-softswitches",    "Soft switches" },
        { "apple2e-family-mmu",             "Memory management unit" },
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

            {
                const JsonValue *  portsArr = nullptr;

                if (entry.HasArray (SettingsPanelState::kpszPortsKey, portsArr))
                {
                    for (j = 0; j < portsArr->GetArraySize(); ++j)
                    {
                        const JsonValue &  port = portsArr->GetArrayElement (j);

                        // A card's connectors are numbered, so they are bare
                        // strings read positionally; the object form is
                        // tolerated for symmetry with the machine's own ports.
                        hw.ports.push_back (port.GetType() == JsonType::String
                                            ? port.GetString()
                                            : GetStringOpt (port, SettingsPanelState::kpszPortDeviceKey, ""));

                        if (j == 0 && port.GetType() == JsonType::Object)
                        {
                            hw.portPath              = GetStringOpt (port, SettingsPanelState::kpszPortPathKey,       "");
                            hw.portBeforePrinter     = GetStringOpt (port, SettingsPanelState::kpszPortBeforeKey,     "");
                            hw.portPathBeforePrinter = GetStringOpt (port, SettingsPanelState::kpszPortPathBeforeKey, "");
                        }
                    }
                }
            }

            {
                const JsonValue *  switches = nullptr;
                HRESULT            hrObj    = entry.GetObject (SettingsPanelState::kpszSwitchesKey, switches);

                IGNORE_RETURN_VALUE (hrObj, S_OK);

                if (switches != nullptr)
                {
                    hw.switchesSw1 = GetStringOpt (*switches, SettingsPanelState::kpszSw1Key, "");
                    hw.switchesSw2 = GetStringOpt (*switches, SettingsPanelState::kpszSw2Key, "");
                    hw.switchesSw1BeforePrinter = GetStringOpt (*switches, SettingsPanelState::kpszSw1BeforeKey, "");
                }
            }

            if (dev == SettingsPanelState::kpszNetworkCardDevice)
            {
                const JsonValue           * network = nullptr;
                std::vector<std::string>    ignored;

                if (entry.HasObject (NetworkCardSettings::kpszJsonKey, network))
                {
                    HRESULT  hrNetwork = NetworkCardSettings::FromJson (*network, hw.network, ignored);
                    IGNORE_RETURN_VALUE (hrNetwork, S_OK);
                }
            }

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

JsonValue SettingsJson::BuildJson (
    const JsonValue                        & mergedJson,
    const std::vector<HardwareEntry>       & hw,
    const SettingsUiPrefs                  & prefs,
    const std::vector<SettingsMachinePort> & machinePorts)
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

        if (key == "internalDevices" || key == "slots" || key == SettingsPanelState::kpszUiPrefsKey)
        {
            continue;
        }

        // Root `ports` is rebuilt from the staged list below when there is
        // one; cloning it here would write the machine's back panel back
        // exactly as loaded and silently discard the toggle.
        if (key == SettingsPanelState::kpszPortsKey && !machinePorts.empty())
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
                bool                                              hadNetwork  = false;
                const JsonValue                                 * network     = nullptr;
                std::vector<std::pair<std::string, JsonValue>>    rebuilt;
                size_t                                            j           = 0;
                if (src.GetType() != JsonType::Object)
                {
                    slotArr.push_back (CloneJson (src));
                    continue;
                }

                enabledFlag = true;
                hadNetwork  = src.HasObject (NetworkCardSettings::kpszJsonKey, network);

                {
                    const HardwareEntry *  staged = nullptr;

                    for (const HardwareEntry & hwEntry : hw)
                    {
                        if (hwEntry.kind == HardwareEntryKind::Slot &&
                            hwEntry.jsonIndex == (int) i)
                        {
                            enabledFlag = hwEntry.enabled;
                            staged      = &hwEntry;
                            break;
                        }
                    }

                    for (auto & srcEntry : srcEntries)
                    {
                        if (srcEntry.first == "enabled")
                        {
                            continue;
                        }


                        if (srcEntry.first == NetworkCardSettings::kpszJsonKey && staged != nullptr)
                        {
                            continue;
                        }

                        // A staged port list replaces the source's. An EMPTY
                        // staged list means the card never declared ports, so
                        // the source key (if any) passes through untouched --
                        // a card must not acquire an empty connector list just
                        // by having been looked at.
                        if (srcEntry.first == SettingsPanelState::kpszPortsKey &&
                            staged != nullptr && !staged->ports.empty())
                        {
                            continue;
                        }

                        // Staged switches replace the source's the same way.
                        if (srcEntry.first == SettingsPanelState::kpszSwitchesKey &&
                            staged != nullptr && !staged->switchesSw1.empty())
                        {
                            continue;
                        }

                        rebuilt.emplace_back (srcEntry.first, CloneJson (srcEntry.second));
                    }

                    rebuilt.emplace_back ("enabled", JsonValue (enabledFlag));

                    // Default settings are not written, so looking at the card
                    // does not put a `network` object into the user's file.
                    if (staged != nullptr
                        && staged->type == SettingsPanelState::kpszNetworkCardDevice
                        && (hadNetwork || !(staged->network == NetworkCardSettings {})))
                    {
                        rebuilt.emplace_back (NetworkCardSettings::kpszJsonKey, staged->network.ToJson());
                    }

                    if (staged != nullptr && !staged->ports.empty())
                    {
                        std::vector<JsonValue>  portArr;

                        for (const std::string & device : staged->ports)
                        {
                            // The first connector of a card with a file
                            // attachment is written as an object, to include the
                            // file's path, and so is one the printer has, to
                            // include the connector it displaced.
                            if (portArr.empty() && (!staged->portPath.empty() || !staged->portBeforePrinter.empty()))
                            {
                                std::vector<std::pair<std::string, JsonValue>>  port;

                                port.emplace_back (SettingsPanelState::kpszPortDeviceKey, JsonValue (device));

                                if (!staged->portPath.empty())
                                {
                                    port.emplace_back (SettingsPanelState::kpszPortPathKey, JsonValue (staged->portPath));
                                }

                                if (!staged->portBeforePrinter.empty())
                                {
                                    port.emplace_back (SettingsPanelState::kpszPortBeforeKey, JsonValue (staged->portBeforePrinter));
                                }

                                if (!staged->portPathBeforePrinter.empty())
                                {
                                    port.emplace_back (SettingsPanelState::kpszPortPathBeforeKey, JsonValue (staged->portPathBeforePrinter));
                                }

                                portArr.emplace_back (JsonValue (std::move (port)));
                            }
                            else
                            {
                                portArr.emplace_back (JsonValue (device));
                            }
                        }

                        rebuilt.emplace_back (SettingsPanelState::kpszPortsKey, JsonValue (std::move (portArr)));
                    }

                    if (staged != nullptr && !staged->switchesSw1.empty())
                    {
                        std::vector<std::pair<std::string, JsonValue>>  switches;

                        switches.emplace_back (SettingsPanelState::kpszSw1Key, JsonValue (staged->switchesSw1));
                        switches.emplace_back (SettingsPanelState::kpszSw2Key, JsonValue (staged->switchesSw2));

                        if (!staged->switchesSw1BeforePrinter.empty())
                        {
                            switches.emplace_back (SettingsPanelState::kpszSw1BeforeKey, JsonValue (staged->switchesSw1BeforePrinter));
                        }

                        rebuilt.emplace_back (SettingsPanelState::kpszSwitchesKey, JsonValue (std::move (switches)));
                    }
                }

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

    // The machine's own connectors, written back in full. A user array
    // replaces the default's wholesale, so emitting only the port that
    // changed would leave the //c with a disk port and nothing else.
    if (! machinePorts.empty())
    {
        std::vector<JsonValue>  portArr;

        for (const SettingsMachinePort & port : machinePorts)
        {
            std::vector<std::pair<std::string, JsonValue>>  entry;

            entry.emplace_back (SettingsPanelState::kpszPortNameKey,   JsonValue (port.name));
            entry.emplace_back (SettingsPanelState::kpszPortDeviceKey, JsonValue (port.device));
            portArr.emplace_back (JsonValue (std::move (entry)));
        }

        root.emplace_back (SettingsPanelState::kpszPortsKey, JsonValue (std::move (portArr)));
    }

    // $cassoUiPrefs block
    uiObj.emplace_back ("speedMode",          JsonValue (std::string (SpeedToString (prefs.speedMode))));
    uiObj.emplace_back ("colorMode",          JsonValue (std::string (ColorToString (prefs.colorMode))));
    uiObj.emplace_back ("writeMode",          JsonValue (std::string (WriteModeToString (prefs.writeMode))));
    uiObj.emplace_back ("floppySoundEnabled", JsonValue (prefs.floppySoundEnabled));
    uiObj.emplace_back ("floppyMechanism",    JsonValue (prefs.floppyMechanism));
    uiObj.emplace_back ("fastTapeLoading",    JsonValue (prefs.fastTapeLoading));
    uiObj.emplace_back ("tapeVolume",         JsonValue ((double) prefs.tapeVolume));
    uiObj.emplace_back ("tapeAutoStop",       JsonValue (prefs.tapeAutoStop));
    uiObj.emplace_back ("tapeIdleStop",       JsonValue (prefs.tapeIdleStop));
    uiObj.emplace_back ("tapeEightBit",       JsonValue (prefs.tapeEightBit));
    uiObj.emplace_back ("tapeRecorderConnected", JsonValue (prefs.tapeRecorderConnected));
    // The legacy boolean is written ONLY when the machine has no disk port to
    // hold the answer. Where a port exists it is authoritative, and writing
    // both would put two answers to one question back on disk -- exactly what
    // folding it into the port was for.
    {
        bool  fHasDiskPort = false;

        for (const SettingsMachinePort & port : machinePorts)
        {
            if (port.name == SettingsPanelState::kpszDiskPortName)
            {
                fHasDiskPort = true;
                break;
            }
        }

        if (!fHasDiskPort)
        {
            uiObj.emplace_back ("externalDriveConnected",
                                JsonValue (prefs.externalDriveConnected));
        }
    }

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

    CarryUnmanagedUiPrefs (uiObj, mergedJson);

    root.emplace_back (SettingsPanelState::kpszUiPrefsKey, JsonValue (std::move (uiObj)));

    result = JsonValue (std::move (root));

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CarryUnmanagedUiPrefs
//
//  Copies forward every $cassoUiPrefs entry the emitter above did not write.
//
//  BuildJson REPLACES the block rather than editing it, so any key it does not
//  know about is dropped -- and the block holds more than this dialog manages:
//  the //c case-switch latches, and the per-machine input mapping. Pressing OK
//  in Settings reset those, from pages that never mentioned them.
//
//  Emitted keys win, so a preference the dialog does own is not resurrected
//  from the stale copy.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsJson::CarryUnmanagedUiPrefs (
    std::vector<std::pair<std::string, JsonValue>> & uiObj,
    const JsonValue                                & mergedJson)
{
    const JsonValue *  uiPrefs  = nullptr;
    bool               hasBlock = mergedJson.HasObject (SettingsPanelState::kpszUiPrefsKey, uiPrefs);
    size_t             emitted  = uiObj.size();
    size_t             i        = 0;
    size_t             j        = 0;
    bool               managed  = false;



    if (!hasBlock || uiPrefs == nullptr)
    {
        return;
    }

    for (i = 0; i < uiPrefs->GetObjectEntries().size(); ++i)
    {
        const std::pair<std::string, JsonValue> &  entry = uiPrefs->GetObjectEntries()[i];

        managed = false;

        for (j = 0; j < emitted && !managed; ++j)
        {
            managed = uiObj[j].first == entry.first;
        }

        if (!managed)
        {
            uiObj.emplace_back (entry);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArePrefsEqual
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsJson::ArePrefsEqual (
    const SettingsUiPrefs & a,
    const SettingsUiPrefs & b)
{
    return a.speedMode              == b.speedMode
        && a.colorMode              == b.colorMode
        && a.writeMode              == b.writeMode
        && a.floppySoundEnabled     == b.floppySoundEnabled
        && a.floppyMechanism        == b.floppyMechanism
        && a.fastTapeLoading        == b.fastTapeLoading
        && a.tapeVolume             == b.tapeVolume
        && a.tapeAutoStop           == b.tapeAutoStop
        && a.tapeIdleStop           == b.tapeIdleStop
        && a.tapeEightBit           == b.tapeEightBit
        && a.tapeRecorderConnected  == b.tapeRecorderConnected
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

bool SettingsJson::AreHardwareEqual (
    const std::vector<HardwareEntry> & a,
    const std::vector<HardwareEntry> & b)
{
    size_t  i     = 0;
    bool    equal = a.size() == b.size();



    for (i = 0; equal && i < a.size(); ++i)
    {
        // `ports` counts: attaching or detaching a drive is a staged edit
        // like any other, and leaving it out here would make the panel report
        // itself clean and never save the change.
        equal = a[i].kind        == b[i].kind
             && a[i].jsonIndex   == b[i].jsonIndex
             && a[i].slot        == b[i].slot
             && a[i].type        == b[i].type
             && a[i].capability  == b[i].capability
             && a[i].enabled     == b[i].enabled
             && a[i].ports       == b[i].ports
             && a[i].portPath    == b[i].portPath
             && a[i].switchesSw1 == b[i].switchesSw1
             && a[i].switchesSw2 == b[i].switchesSw2
             && a[i].switchesSw1BeforePrinter == b[i].switchesSw1BeforePrinter
             && a[i].portBeforePrinter        == b[i].portBeforePrinter
             && a[i].portPathBeforePrinter    == b[i].portPathBeforePrinter
             && a[i].network     == b[i].network;
    }

    return equal;
}
