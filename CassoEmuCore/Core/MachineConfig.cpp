#include "Pch.h"

#include "MachineConfig.h"
#include "JsonParser.h"
#include "PathResolver.h"


static constexpr int    kMinSlot       = 1;
static constexpr int    kMaxSlot       = 7;
static constexpr Word   kRamMaxAddress = 0xFFFF;
static constexpr size_t kRamMaxSize    = 0x10000;





////////////////////////////////////////////////////////////////////////////////
//
//  ParseHexAddress
//
//  Parses a 6502 address from machine JSON, accepting both `0xNNNN` and the
//  period `$NNNN` spelling.
//
//  Both forms are supported because these files are read and written by people
//  who think in Apple II documentation, where addresses are always `$C000`,
//  and by tooling that emits C-style hex. Rejecting either would make one
//  audience's natural spelling an error.
//
//  A BARE number is deliberately not accepted. Without a prefix there is no
//  way to tell an intended hex value from a decimal one, and silently guessing
//  wrong relocates a device by 0x9000 addresses.
//
//  Every failure sets a message naming the offending text and what was
//  expected, since the reader is editing a config file by hand and the only
//  useful diagnostic says which value is wrong.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::ParseHexAddress (const string & str, Word & outAddr, string & outError)
{
    HRESULT       hr     = S_OK;
    LPCSTR        pszHex = str.c_str();
    char        * pEnd   = nullptr;
    unsigned long val    = 0;



    if (str.size() >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
    {
        pszHex += 2;
    }
    else if (str.size() >= 1 && str[0] == '$')
    {
        pszHex++;
    }
    else
    {
        CBRF (false, outError = format ("Invalid address format: '{}' (expected 0xNNNN or $NNNN)", str));
    }

    val = strtoul (pszHex, &pEnd, 16);

    CBRF (pEnd != pszHex && *pEnd == '\0', outError = format ("Invalid hex digits in address: '{}'",    str));
    CBRF (val <= kRamMaxAddress,           outError = format ("Address out of range: '{}' (max $FFFF)", str));

    outAddr = static_cast<Word> (val);


Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseHexSize
//
//  Sizes can be 1..0x10000 (full 64K).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::ParseHexSize (const string & str, uint32_t & outSize, string & outError)
{
    HRESULT       hr     = S_OK;
    LPCSTR        pszHex = str.c_str();
    char        * pEnd   = nullptr;
    unsigned long val    = 0;



    if (str.size() >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
    {
        pszHex += 2;
    }
    else if (str.size() >= 1 && str[0] == '$')
    {
        pszHex++;
    }
    else
    {
        CBRF (false, outError = format ("Invalid size format: '{}' (expected 0xNNNN or $NNNN)", str));
    }

    val = strtoul (pszHex, &pEnd, 16);

    CBRF (pEnd != pszHex && *pEnd == '\0', outError = format ("Invalid hex digits in size: '{}'", str));
    CBRF (val > 0 && val <= kRamMaxSize,   outError = format ("Size out of range: '{}' (1..0x10000)", str));

    outSize = static_cast<uint32_t> (val);


Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseCapabilityFlag
//
//  Reads a device's capability flag, which says whether the user may remove it.
//
//  The three values encode a real distinction in the hardware:
//
//    optional         a card the user can add or remove (a Disk ][ controller)
//    required         the machine needs it to boot, but it is still a device
//    platform-locked  soldered in -- the //c's internal drive and mouse are
//                     not cards, and no UI may offer to unplug them
//
//  An ABSENT value takes the caller's default rather than a fixed one, because
//  the sensible default differs by call site: a slot device is optional unless
//  stated, an internal device is not.
//
//  An unrecognized value is an error rather than a fallback. Misspelling
//  "platform-locked" would otherwise silently produce a //c whose internal
//  drive can be removed, which is a broken machine that looks like a working
//  one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::ParseCapabilityFlag (
    const string  & str,
    CapabilityFlag  defaultFlag,
    CapabilityFlag & outFlag,
    string         & outError)
{
    HRESULT hr = S_OK;



    if (str.empty())
    {
        outFlag = defaultFlag;
    }
    else if (str == "optional")
    {
        outFlag = CapabilityFlag::Optional;
    }
    else if (str == "required")
    {
        outFlag = CapabilityFlag::Required;
    }
    else if (str == "platform-locked")
    {
        outFlag = CapabilityFlag::PlatformLocked;
    }
    else
    {
        CBRF (false, outError = format (
            "Invalid capabilityFlag: '{}' (expected 'optional', 'required', or 'platform-locked')",
            str));
    }


Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadTiming
//
//  Reads the timing block: video standard, clock speed, and cycles per
//  scanline.
//
//  Only the scanline COUNT is implied by the video standard -- NTSC and PAL
//  differ in how many lines a frame has. Clock speed and cycles per scanline
//  stay explicit, because they are properties of the machine rather than of
//  the broadcast standard, and a European Apple II is not simply a 60 Hz one
//  with more lines.
//
//  Cycles per frame is DERIVED here rather than read, so a config cannot
//  declare a frame length that disagrees with its own scanline count and
//  per-scanline cycles. That product is what the VBL model counts against, and
//  an inconsistent value would put $C019 out of phase with the video.
//
//  Every field is required. A missing timing value has no defensible default:
//  guessing a clock speed silently mis-times every program the machine runs.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::LoadTiming (
    const JsonValue & timing,
    MachineConfig   & outConfig,
    string          & outError)
{
    HRESULT  hr            = S_OK;
    string   videoStandard;



    hr = timing.GetString ("videoStandard", videoStandard);
    CHRF (hr, outError = "Missing or invalid field: 'timing.videoStandard'");

    if (videoStandard == "ntsc")
    {
        outConfig.videoStandard     = VideoStandard::NTSC;
        outConfig.scanlinesPerFrame = kNtscScanlines;
    }
    else if (videoStandard == "pal")
    {
        outConfig.videoStandard     = VideoStandard::PAL;
        outConfig.scanlinesPerFrame = kPalScanlines;
    }
    else
    {
        CBRF (false, outError = format ("Invalid videoStandard: '{}' (expected 'ntsc' or 'pal')", videoStandard));
    }

    hr = timing.GetUint32 ("clockSpeed", outConfig.clockSpeed);
    CHRF (hr, outError = "Missing or invalid field: 'timing.clockSpeed'");

    hr = timing.GetUint32 ("cyclesPerScanline", outConfig.cyclesPerScanline);
    CHRF (hr, outError = "Missing or invalid field: 'timing.cyclesPerScanline'");

    outConfig.cyclesPerFrame = outConfig.cyclesPerScanline * outConfig.scanlinesPerFrame;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveRomFile
//
//  Helper: resolve a relative ROM path through the search paths. The
//  caller supplies the relative directory prefix (e.g.,
//  "Machines/Apple2e" or "Devices/DiskII") so per-machine and per-
//  device ROM layouts can coexist under the same set of search bases.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT ResolveRomFile (
    const string                                                                        & file,
    const fs::path                                                                      & relDirPrefix,
    const vector<fs::path>                                                              & searchPaths,
    const MachineConfigLoader::FileResolver                                             & resolver,
    string                                                                              & outResolvedPath,
    size_t                                                                              & outFileSize,
    string                                                                              & outError)
{
    HRESULT   hr         = S_OK;
    fs::path  romRelPath = relDirPrefix / file;
    fs::path  found      = resolver (searchPaths, romRelPath);
    auto      sz         = std::uintmax_t {0};
    bool      wasFound   = false;



    wasFound = !found.empty();
    CBRF (wasFound,
          outError = format ("ROM file not found: {}. "
                             "Place the file under the appropriate per-machine or per-device folder.",
                             romRelPath.string()));

    sz = fs::file_size (found);
    CBRF (sz > 0,
          outError = format ("ROM file is empty: {}", romRelPath.string()));

    outResolvedPath = found.string();
    outFileSize     = static_cast<size_t> (sz);


Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadRam
//
//  Reads the RAM region array -- an address, a size, and an optional bank name
//  per entry.
//
//  Regions are described rather than assumed because the machines differ:
//  a ][+ has one flat span, while a //e adds an auxiliary bank that overlays
//  the same addresses. The optional `bank` name is what distinguishes an aux
//  region from a main one at the same address; its absence means main, so the
//  older configs need no bank field at all.
//
//  A full 64 KB size is stored as 0. The size field is a Word, so 0x10000 does
//  not fit -- and since a zero-length region is meaningless, zero is free to
//  mean the whole address space. The bounds check runs on the 32-bit value
//  BEFORE that narrowing, so an oversized region is rejected rather than
//  silently wrapping into a valid-looking small one.
//
//  Each error names the array index, because these arrays run to several
//  entries and "missing 'size' field" without one sends the reader hunting
//  through the file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::LoadRam (
    const JsonValue & ramArray,
    MachineConfig   & outConfig,
    string          & outError)
{
    HRESULT   hr     = S_OK;
    size_t    idx    = 0;
    string    addrStr;
    string    sizeStr;
    uint32_t  size32 = 0;



    for (idx = 0; idx < ramArray.ArraySize(); idx++)
    {
        const JsonValue & entry = ramArray.ArrayAt (idx);
        RamRegion         region;



        hr = entry.GetString ("address", addrStr);
        CHRF (hr, outError = format ("ram[{}]: missing or invalid 'address' field", idx));

        hr = ParseHexAddress (addrStr, region.address, outError);
        CHR (hr);

        hr = entry.GetString ("size", sizeStr);
        CHRF (hr, outError = format ("ram[{}]: missing or invalid 'size' field", idx));

        hr = ParseHexSize (sizeStr, size32, outError);
        CHR (hr);

        CBRF (static_cast<uint32_t> (region.address) + size32 <= kRamMaxSize,
              outError = format ("ram[{}]: address ${:04X} + size ${:X} exceeds 64K",
                                 idx, region.address, size32));

        region.size = static_cast<Word> (size32 == kRamMaxSize ? 0 : size32);

        // Optional bank field (don't pollute hr with the lookup result)
        HRESULT hrBank = entry.GetString ("bank", region.bank);
        IGNORE_RETURN_VALUE (hrBank, S_OK);

        outConfig.ram.push_back (region);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadSystemRom
//
//  Reads the system ROM: address, file, and the optional bank-switching pair
//  that the //c needs and earlier machines do not.
//
//  romBankSize and romBankSelect travel together -- one without the other is
//  rejected -- because a bank size with no select register describes banking
//  nobody can perform. Their ABSENCE is meaningful and is the //e and earlier
//  case: a flat single-image ROM.
//
//  The two size checks are genuinely different, not a refactoring oversight.
//  A flat ROM must fit in 64 KB as a whole. A banked ROM maps every bank at
//  the SAME address, so it is each bank that must fit, and the file is
//  expected to be several banks long -- checking the whole file against 64 KB
//  would reject every valid //c ROM. The banked path adds the divisibility
//  test, since a file that is not a whole number of banks has a partial bank
//  at the end that would map as garbage.
//
//  The ROM path is resolved through the search paths under a per-machine
//  prefix, so two machines can ship files of the same name.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::LoadSystemRom (
    const JsonValue        & sysRomObj,
    const string           & machineName,
    const vector<fs::path> & searchPaths,
    const FileResolver     & resolver,
    MachineConfig          & outConfig,
    string                 & outError)
{
    HRESULT  hr      = S_OK;
    string   addrStr;
    fs::path relDir  = fs::path ("Machines") / machineName;



    hr = sysRomObj.GetString ("address", addrStr);
    CHRF (hr, outError = "Missing or invalid field: 'systemRom.address'");

    hr = ParseHexAddress (addrStr, outConfig.systemRom.address, outError);
    CHR (hr);

    hr = sysRomObj.GetString ("file", outConfig.systemRom.file);
    CHRF (hr, outError = "Missing or invalid field: 'systemRom.file'");

    hr = ResolveRomFile (outConfig.systemRom.file,
                         relDir,
                         searchPaths,
                         resolver,
                         outConfig.systemRom.resolvedPath,
                         outConfig.systemRom.fileSize,
                         outError);
    CHR (hr);

    // Optional bank-switched ROM (Apple //c): romBankSize + romBankSelect.
    // Absent -> flat single-image ROM (the //e and earlier).
    {
        string  bankSizeStr;
        string  bankSelectStr;

        if (sysRomObj.HasString ("romBankSize", bankSizeStr))
        {
            hr = ParseHexAddress (bankSizeStr, outConfig.systemRom.romBankSize, outError);
            CHR (hr);

            hr = sysRomObj.GetString ("romBankSelect", bankSelectStr);
            CHRF (hr, outError = "systemRom.romBankSize requires 'romBankSelect'");

            hr = ParseHexAddress (bankSelectStr, outConfig.systemRom.romBankSelect, outError);
            CHR (hr);
        }
    }

    if (outConfig.systemRom.romBankSize != 0)
    {
        // Banked: every bank is mapped at `address`, so each bank (not the
        // whole file) must fit in 64K, and the file must be a whole number
        // of banks.
        CBRF (static_cast<uint32_t> (outConfig.systemRom.address) + outConfig.systemRom.romBankSize <= kRamMaxSize,
              outError = format ("systemRom: address ${:04X} + bank size ${:X} exceeds 64K",
                                 outConfig.systemRom.address, outConfig.systemRom.romBankSize));

        CBRF (outConfig.systemRom.fileSize != 0 &&
              (outConfig.systemRom.fileSize % outConfig.systemRom.romBankSize) == 0,
              outError = format ("systemRom: file size ${:X} is not a whole number of ${:X}-byte banks",
                                 outConfig.systemRom.fileSize, outConfig.systemRom.romBankSize));
    }
    else
    {
        // Validate flat ROM fits in 64K starting at address
        CBRF (static_cast<uint32_t> (outConfig.systemRom.address) + outConfig.systemRom.fileSize <= kRamMaxSize,
              outError = format ("systemRom: address ${:04X} + size ${:X} exceeds 64K",
                                 outConfig.systemRom.address, outConfig.systemRom.fileSize));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadCharacterRom
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::LoadCharacterRom (
    const JsonValue        & charRomObj,
    const string           & machineName,
    const vector<fs::path> & searchPaths,
    const FileResolver     & resolver,
    MachineConfig          & outConfig,
    string                 & outError)
{
    HRESULT  hr     = S_OK;
    fs::path relDir = fs::path ("Machines") / machineName;



    hr = charRomObj.GetString ("file", outConfig.characterRom.file);
    CHRF (hr, outError = "Missing or invalid field: 'characterRom.file'");

    hr = ResolveRomFile (outConfig.characterRom.file,
                         relDir,
                         searchPaths,
                         resolver,
                         outConfig.characterRom.resolvedPath,
                         outConfig.characterRom.fileSize,
                         outError);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadInternalDevices
//
//  Reads the built-in (non-slot) devices: the //c's internal drive, mouse, and
//  serial ports, which are soldered to the board rather than plugged into a
//  slot.
//
//  Only `type` is required. Everything else describes how the UI may treat the
//  device, and the defaults are chosen so an older config that predates those
//  fields still loads and still means what it meant.
//
//  The capability default here is REQUIRED, the opposite of the slot default
//  (FR-015). An internal device is part of the machine: a //c without its
//  keyboard is not a configuration anyone asked for, whereas an empty slot is
//  ordinary. Both defaults are supplied by their caller rather than baked into
//  ParseCapabilityFlag for exactly this reason.
//
//  lockReason carries the user-facing explanation for why a locked device
//  cannot be removed, so the UI can say something better than "disabled".
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::LoadInternalDevices (
    const JsonValue & devArray,
    MachineConfig   & outConfig,
    string          & outError)
{
    HRESULT  hr  = S_OK;
    size_t   idx = 0;



    for (idx = 0; idx < devArray.ArraySize(); idx++)
    {
        const JsonValue  & entry   = devArray.ArrayAt (idx);
        InternalDevice     dev;
        string             flagStr;
        HRESULT            hrFlag  = S_OK;
        HRESULT            hrLock  = S_OK;



        hr = entry.GetString ("type", dev.type);
        CHRF (hr, outError = format ("internalDevices[{}]: missing or invalid 'type' field", idx));

        // Optional: capabilityFlag (default = Required per FR-015).
        hrFlag = entry.GetString ("capabilityFlag", flagStr);

        if (SUCCEEDED (hrFlag))
        {
            hr = ParseCapabilityFlag (flagStr,
                                      CapabilityFlag::Required,
                                      dev.capabilityFlag,
                                      outError);
            CHRF (hr, outError = format ("internalDevices[{}]: {}", idx, outError));
        }

        // Optional: lockReason
        hrLock = entry.GetString ("lockReason", dev.lockReason);
        IGNORE_RETURN_VALUE (hrLock, S_OK);

        outConfig.internalDevices.push_back (dev);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParsePorts
//
//  Reads a `ports` array, which describes what the owner's CONNECTORS have
//  attached to them. The owner is a slot entry for a card's ports, or the
//  machine root for a machine whose hardware is built in -- a //c has no
//  slots, it has a back panel, and both are the same idea.
//
//  A port entry takes either form:
//
//      "ports": [ "disk-ii-drive", "" ]                             numbered
//      "ports": [ { "name": "disk", "device": "disk-ii-drive" } ]   named
//
//  The short form is for connectors told apart only by their order, like the
//  Disk II Interface's two drive ports. The object form is for connectors with
//  an identity of their own, like a //c's disk/serial/joystick ports, which
//  are emphatically not interchangeable.
//
//  NOTHING HERE IS AN ERROR. An empty or absent `device` is an unoccupied
//  connector, which is a real hardware state and the most common one; a
//  malformed entry becomes an unoccupied connector too, because refusing to
//  load a machine over a junk port would cost the user their whole config to
//  punish a field that has a harmless reading.
//
////////////////////////////////////////////////////////////////////////////////

void MachineConfigLoader::ParsePorts (
    const JsonValue    & owner,
    vector<PortConfig> & outPorts)
{
    const JsonValue * portsArray = nullptr;
    HRESULT           hrPorts    = owner.GetArray ("ports", portsArray);

    IGNORE_RETURN_VALUE (hrPorts, S_OK);

    if (portsArray == nullptr)
    {
        // No key at all. Leave the vector empty, which every caller reads as
        // "this machine's default" -- NOT as a machine with no connectors.
        return;
    }

    for (size_t idx = 0; idx < portsArray->ArraySize(); idx++)
    {
        const JsonValue & entry = portsArray->ArrayAt (idx);
        PortConfig        port;

        if (entry.GetType() == JsonType::String)
        {
            port.device = entry.GetString();
        }
        else if (entry.GetType() == JsonType::Object)
        {
            HRESULT hrName = entry.GetString ("name",   port.name);
            HRESULT hrDev  = entry.GetString ("device", port.device);

            IGNORE_RETURN_VALUE (hrName, S_OK);
            IGNORE_RETURN_VALUE (hrDev,  S_OK);
        }

        // Anything else (null, a number, a nested array) leaves both fields
        // empty and lands here as an unoccupied connector, on purpose.
        outPorts.push_back (port);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadSlots
//
//  Reads the peripheral slot array: which slot, what device, and the optional
//  256-byte boot ROM that lives at $Cn00.
//
//  A slot must declare a device, a ROM, or both, but never neither. All three
//  combinations are real hardware: a card with firmware, a card with none, and
//  a bare ROM in a slot with no emulated device behind it.
//
//  Slot ROMs are required to be exactly 256 bytes because that is the size of
//  the $Cn00 window the hardware maps them into. A larger file is not a ROM
//  that gets truncated -- it is a different kind of image (an expansion ROM,
//  or a whole card dump) that would map as garbage, so it is rejected with its
//  actual size named.
//
//  ROM search is routed by DEVICE FAMILY, not by machine: a `disk-ii` ROM
//  resolves under Devices/DiskII so every machine that has one shares the same
//  file, while an unrecognized device falls back to the per-machine folder.
//  That is what lets future families (Mockingboard, SSC) collocate their boot
//  ROMs with their other resources.
//
//  The capability default is OPTIONAL here -- a card is something the user may
//  unplug -- which is the reverse of LoadInternalDevices.
//
//  `enabled` defaults to true and is written as false by Settings > Hardware;
//  the machine builder then skips installing this slot's device and ROM
//  entirely, which is how a slot is emptied without editing the config's
//  contents.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::LoadSlots (
    const JsonValue        & slotsArray,
    const string           & machineName,
    const vector<fs::path> & searchPaths,
    const FileResolver     & resolver,
    MachineConfig          & outConfig,
    string                 & outError)
{
    HRESULT  hr           = S_OK;
    size_t   idx          = 0;
    bool     hasDev       = false;
    bool     hasRom       = false;
    fs::path machineRel   = fs::path ("Machines") / machineName;
    fs::path disk2Rel     = fs::path ("Devices") / "DiskII";



    for (idx = 0; idx < slotsArray.ArraySize(); idx++)
    {
        const JsonValue & entry = slotsArray.ArrayAt (idx);
        SlotConfig        slot;
        fs::path          slotRomRel;



        hr = entry.GetInt ("slot", slot.slot);
        CHRF (hr, outError = format ("slots[{}]: missing or invalid 'slot' field", idx));

        CBRF (slot.slot >= kMinSlot && slot.slot <= kMaxSlot,
              outError = format ("slots[{}]: slot must be {}-{}, got {}",
                                 idx, kMinSlot, kMaxSlot, slot.slot));

        // Optional: device (don't pollute hr with the lookup result)
        HRESULT hrDev = entry.GetString ("device", slot.device);
        IGNORE_RETURN_VALUE (hrDev, S_OK);
        hasDev = !slot.device.empty();

        // Optional: rom (don't pollute hr with the lookup result)
        HRESULT hrRom = entry.GetString ("rom", slot.rom);
        IGNORE_RETURN_VALUE (hrRom, S_OK);
        hasRom = !slot.rom.empty();

        CBRF (hasDev || hasRom,
              outError = format ("slots[{}]: must specify 'device' and/or 'rom'", idx));

        ParsePorts (entry, slot.ports);

        if (hasRom)
        {
            // Slot ROMs whose device is a known per-device family live
            // under the matching `Devices/<Family>/` folder so future
            // device families (Mockingboard, SSC, ...) can collocate
            // their resources alongside their boot ROMs. Anything we
            // don't recognise falls back to the per-machine folder.
            slotRomRel = (slot.device == "disk-ii") ? disk2Rel : machineRel;

            hr = ResolveRomFile (slot.rom,
                                 slotRomRel,
                                 searchPaths,
                                 resolver,
                                 slot.resolvedRomPath,
                                 slot.romSize,
                                 outError);
            CHR (hr);

            CBRF (slot.romSize == 0x100,
                  outError = format ("slots[{}]: slot ROM '{}' must be 256 bytes, got {}",
                                     idx, slot.rom, slot.romSize));
        }

        // Optional: capabilityFlag (default = Optional per FR-015).
        {
            string   flagStr;
            HRESULT  hrFlag = entry.GetString ("capabilityFlag", flagStr);

            if (SUCCEEDED (hrFlag))
            {
                hr = ParseCapabilityFlag (flagStr,
                                          CapabilityFlag::Optional,
                                          slot.capabilityFlag,
                                          outError);
                CHRF (hr, outError = format ("slots[{}]: {}", idx, outError));
            }
        }

        // Optional: lockReason
        {
            HRESULT hrLock = entry.GetString ("lockReason", slot.lockReason);
            IGNORE_RETURN_VALUE (hrLock, S_OK);
        }

        // Optional: enabled (default true). A user disabling the slot in
        // Settings > Hardware writes "enabled": false; the machine builder
        // then skips installing this slot's device + ROM.
        {
            HRESULT hrEnabled = entry.GetBool ("enabled", slot.enabled);
            IGNORE_RETURN_VALUE (hrEnabled, S_OK);
        }

        outConfig.slots.push_back (slot);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectRomFiles
//
//  Light-weight pass that returns the relative ROM filenames
//  referenced by a machine config (systemRom.file, characterRom.file,
//  and any slots[].rom). Filenames are returned without "ROMs/"
//  prefix and without resolution. Used for pre-flight existence
//  checks (e.g. to offer the user a download).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::CollectRomFiles (
    const string   & jsonText,
    vector<string> & outFiles,
    string         & outError)
{
    HRESULT             hr             = S_OK;
    JsonValue           root;
    JsonParseError      parseError;
    const JsonValue   * pSystemRom     = nullptr;
    const JsonValue   * pCharRom       = nullptr;
    const JsonValue   * pSlots         = nullptr;
    string              file;
    HRESULT             hrOpt          = S_OK;
    size_t              idx            = 0;



    outFiles.clear();

    hr = JsonParser::Parse (jsonText, root, parseError);

    if (FAILED (hr))
    {
        outError = format ("JSON parse error at line {}, column {}: {}",
                           parseError.line, parseError.column, parseError.message);
    }

    CHR (hr);

    hrOpt = root.GetObject ("systemRom", pSystemRom);

    if (SUCCEEDED (hrOpt))
    {
        HRESULT hrFile = pSystemRom->GetString ("file", file);

        if (SUCCEEDED (hrFile) && !file.empty())
        {
            outFiles.push_back (file);
        }
    }

    hrOpt = root.GetObject ("characterRom", pCharRom);

    if (SUCCEEDED (hrOpt))
    {
        HRESULT hrFile = pCharRom->GetString ("file", file);

        if (SUCCEEDED (hrFile) && !file.empty())
        {
            outFiles.push_back (file);
        }
    }

    hrOpt = root.GetArray ("slots", pSlots);

    if (SUCCEEDED (hrOpt))
    {
        for (idx = 0; idx < pSlots->ArraySize(); idx++)
        {
            const JsonValue & entry  = pSlots->ArrayAt (idx);
            HRESULT           hrFile = entry.GetString ("rom", file);

            if (SUCCEEDED (hrFile) && !file.empty())
            {
                outFiles.push_back (file);
            }
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Load
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::Load (
    const string           & jsonText,
    const string           & machineName,
    const vector<fs::path> & searchPaths,
    MachineConfig          & outConfig,
    string                 & outError)
{
    return Load (jsonText, machineName, searchPaths, PathResolver::FindFile,
                 outConfig, outError);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Load
//
//  Parses a machine JSON document into a MachineConfig, resolving every ROM
//  reference through the caller's search paths.
//
//  ROM resolution is injected as a FileResolver rather than performed here,
//  which is what makes this whole loader testable: a test supplies a resolver
//  backed by a table instead of the disk, so config parsing is covered without
//  shipping ROM files into the test tree.
//
//  Required and optional fields are a deliberate split, not an accident of
//  what happened to be written first:
//
//    required  name, cpu, timing, ram, systemRom, internalDevices, video,
//              keyboard -- a machine missing any of these cannot be built,
//              and defaulting one would fabricate a machine nobody described
//    optional  characterRom (a ][+ has no separate character generator to
//              load), slots (a //c has none)
//
//  internalDevices is required but may be EMPTY. Present-and-empty says "this
//  machine has no built-in devices"; absent says the config was written
//  against an older schema, and the two deserve different treatment.
//
//  Validation happens during the load, not after: the CPU string is checked
//  against the two supported cores here, and each sub-loader validates its own
//  section. So the first error the reader sees names the actual offending
//  field rather than a downstream symptom.
//
//  Every failure sets outError to a message naming the field, since the reader
//  is hand-editing JSON and a bare failure code tells them nothing about which
//  line to fix.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::Load (
    const string           & jsonText,
    const string           & machineName,
    const vector<fs::path> & searchPaths,
    const FileResolver     & resolver,
    MachineConfig          & outConfig,
    string                 & outError)
{
    HRESULT              hr             = S_OK;
    JsonValue            root;
    JsonParseError       parseError;
    const JsonValue    * pTiming        = nullptr;
    const JsonValue    * pRamArray      = nullptr;
    const JsonValue    * pSystemRom     = nullptr;
    const JsonValue    * pInternalDevs  = nullptr;
    const JsonValue    * pVideo         = nullptr;
    const JsonValue    * pKeyboard      = nullptr;



    // Parse JSON
    hr = JsonParser::Parse (jsonText, root, parseError);

    if (FAILED (hr))
    {
        outError = format ("JSON parse error at line {}, column {}: {}",
                           parseError.line, parseError.column, parseError.message);
    }

    CHR (hr);

    // Required: name, cpu
    hr = root.GetString ("name", outConfig.name);
    CHRF (hr, outError = "Missing or invalid field: 'name'");

    hr = root.GetString ("cpu", outConfig.cpu);
    CHRF (hr, outError = "Missing or invalid field: 'cpu'");

    CBRF (outConfig.cpu == "6502" || outConfig.cpu == "65C02",
          outError = format ("Invalid CPU type: '{}' (expected '6502' or '65C02')", outConfig.cpu));

    // Required: timing
    hr = root.GetObject ("timing", pTiming);
    CHRF (hr, outError = "Missing required field: 'timing'");

    hr = LoadTiming (*pTiming, outConfig, outError);
    CHR (hr);

    // Required: ram (array)
    hr = root.GetArray ("ram", pRamArray);
    CHRF (hr, outError = "Missing required field: 'ram'");

    hr = LoadRam (*pRamArray, outConfig, outError);
    CHR (hr);

    // Required: systemRom (object)
    hr = root.GetObject ("systemRom", pSystemRom);
    CHRF (hr, outError = "Missing required field: 'systemRom'");

    hr = LoadSystemRom (*pSystemRom, machineName, searchPaths, resolver,
                        outConfig, outError);
    CHR (hr);

    // Optional: characterRom (object)
    {
        const JsonValue * pOpt = nullptr;
        HRESULT           hrOpt = root.GetObject ("characterRom", pOpt);

        if (SUCCEEDED (hrOpt))
        {
            hr = LoadCharacterRom (*pOpt, machineName, searchPaths, resolver,
                                   outConfig, outError);
            CHR (hr);
        }
    }

    // Required: internalDevices (array, may be empty)
    hr = root.GetArray ("internalDevices", pInternalDevs);
    CHRF (hr, outError = "Missing required field: 'internalDevices'");

    hr = LoadInternalDevices (*pInternalDevs, outConfig, outError);
    CHR (hr);

    // Optional: slots (array)
    {
        const JsonValue * pOpt = nullptr;
        HRESULT           hrOpt = root.GetArray ("slots", pOpt);

        if (SUCCEEDED (hrOpt))
        {
            hr = LoadSlots (*pOpt, machineName, searchPaths, resolver,
                            outConfig, outError);
            CHR (hr);
        }
    }

    // Optional: ports (array) -- the MACHINE's own connectors, for hardware
    // that is built in rather than carded. A //e's drive ports belong to the
    // card in slot 6 and are read above; a //c's belong to the machine, so a
    // slotless machine still gets to say what it has and what is on it.
    ParsePorts (root, outConfig.ports);

    // Required: video
    hr = root.GetObject ("video", pVideo);
    CHRF (hr, outError = "Missing required field: 'video'");
    LoadVideoConfig (*pVideo, outConfig);

    // Required: keyboard
    hr = root.GetObject ("keyboard", pKeyboard);
    CHRF (hr, outError = "Missing required field: 'keyboard'");
    LoadKeyboardConfig (*pKeyboard, outConfig);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetValue
//
//  Reads one declaratively-described field out of a JSON object and stores it
//  through a member pointer.
//
//  The Field descriptor carries a key plus exactly one non-null member
//  pointer, and that pointer's TYPE selects how the value is read: a string
//  member reads a string, a Word member reads a string and parses it as a hex
//  address, an int member reads a number. That is what keeps the descriptor
//  tables driving this purely declarative -- a field is a key and a
//  destination, with no parse function to choose and no way to pair a key with
//  the wrong reader.
//
//  A descriptor with every pointer null is a silent no-op rather than an
//  error, so a table can carry a conditionally-unused entry without a guard at
//  each call site.
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
HRESULT MachineConfigLoader::GetValue (
    const JsonValue  & entry,
    const Field<T>   & f,
    T                & dest,
    string           & outError)
{
    HRESULT hr = S_OK;



    if (f.strDest != nullptr)
    {
        hr = entry.GetString (f.key, dest.*(f.strDest));
        CHR (hr);
    }
    else if (f.wDest != nullptr)
    {
        string addrStr;



        hr = entry.GetString (f.key, addrStr);
        CHR (hr);

        hr = ParseHexAddress (addrStr, dest.*(f.wDest), outError);
        CHR (hr);
    }
    else if (f.intDest != nullptr)
    {
        hr = entry.GetInt (f.key, dest.*(f.intDest));
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadVideoConfig
//
////////////////////////////////////////////////////////////////////////////////

void MachineConfigLoader::LoadVideoConfig (const JsonValue & video, MachineConfig & outConfig)
{
    HRESULT           hr     = S_OK;
    const JsonValue * pModes = nullptr;



    hr = video.GetArray ("modes", pModes);
    if (SUCCEEDED (hr))
    {
        for (size_t i = 0; i < pModes->ArraySize(); i++)
        {
            if (pModes->ArrayAt (i).GetType() == JsonType::String)
            {
                outConfig.videoConfig.modes.push_back (pModes->ArrayAt (i).GetString());
            }
        }
    }

    video.GetInt ("width",  outConfig.videoConfig.width);
    video.GetInt ("height", outConfig.videoConfig.height);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadKeyboardConfig
//
////////////////////////////////////////////////////////////////////////////////

void MachineConfigLoader::LoadKeyboardConfig (const JsonValue & keyboard, MachineConfig & outConfig)
{
    keyboard.GetString ("type", outConfig.keyboardType);
}
