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
//  LoadTiming
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
//  Helper: resolve a relative ROM path through the search paths.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT ResolveRomFile (
    const string                                                                        & file,
    const vector<fs::path>                                                              & searchPaths,
    const MachineConfigLoader::FileResolver                                             & resolver,
    string                                                                              & outResolvedPath,
    size_t                                                                              & outFileSize,
    string                                                                              & outError)
{
    HRESULT   hr         = S_OK;
    fs::path  romRelPath = fs::path ("ROMs") / file;
    fs::path  found      = resolver (searchPaths, romRelPath);
    auto      sz         = std::uintmax_t {0};



    CBRF (!found.empty (),
          outError = format ("ROM file not found: ROMs/{}. "
                             "Run scripts/FetchRoms.ps1 to download ROM images.",
                             file));

    sz = fs::file_size (found);
    CBRF (sz > 0,
          outError = format ("ROM file is empty: ROMs/{}", file));

    outResolvedPath = found.string ();
    outFileSize     = static_cast<size_t> (sz);


Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadRam
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



    for (idx = 0; idx < ramArray.ArraySize (); idx++)
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
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::LoadSystemRom (
    const JsonValue        & sysRomObj,
    const vector<fs::path> & searchPaths,
    const FileResolver     & resolver,
    MachineConfig          & outConfig,
    string                 & outError)
{
    HRESULT  hr      = S_OK;
    string   addrStr;



    hr = sysRomObj.GetString ("address", addrStr);
    CHRF (hr, outError = "Missing or invalid field: 'systemRom.address'");

    hr = ParseHexAddress (addrStr, outConfig.systemRom.address, outError);
    CHR (hr);

    hr = sysRomObj.GetString ("file", outConfig.systemRom.file);
    CHRF (hr, outError = "Missing or invalid field: 'systemRom.file'");

    hr = ResolveRomFile (outConfig.systemRom.file,
                         searchPaths,
                         resolver,
                         outConfig.systemRom.resolvedPath,
                         outConfig.systemRom.fileSize,
                         outError);
    CHR (hr);

    // Validate ROM fits in 64K starting at address
    CBRF (static_cast<uint32_t> (outConfig.systemRom.address) + outConfig.systemRom.fileSize <= kRamMaxSize,
          outError = format ("systemRom: address ${:04X} + size ${:X} exceeds 64K",
                             outConfig.systemRom.address, outConfig.systemRom.fileSize));

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
    const vector<fs::path> & searchPaths,
    const FileResolver     & resolver,
    MachineConfig          & outConfig,
    string                 & outError)
{
    HRESULT hr = S_OK;



    hr = charRomObj.GetString ("file", outConfig.characterRom.file);
    CHRF (hr, outError = "Missing or invalid field: 'characterRom.file'");

    hr = ResolveRomFile (outConfig.characterRom.file,
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
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::LoadInternalDevices (
    const JsonValue & devArray,
    MachineConfig   & outConfig,
    string          & outError)
{
    HRESULT  hr  = S_OK;
    size_t   idx = 0;



    for (idx = 0; idx < devArray.ArraySize (); idx++)
    {
        const JsonValue & entry = devArray.ArrayAt (idx);
        InternalDevice    dev;



        hr = entry.GetString ("type", dev.type);
        CHRF (hr, outError = format ("internalDevices[{}]: missing or invalid 'type' field", idx));

        outConfig.internalDevices.push_back (dev);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadSlots
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::LoadSlots (
    const JsonValue        & slotsArray,
    const vector<fs::path> & searchPaths,
    const FileResolver     & resolver,
    MachineConfig          & outConfig,
    string                 & outError)
{
    HRESULT  hr     = S_OK;
    size_t   idx    = 0;
    bool     hasDev = false;
    bool     hasRom = false;



    for (idx = 0; idx < slotsArray.ArraySize (); idx++)
    {
        const JsonValue & entry = slotsArray.ArrayAt (idx);
        SlotConfig        slot;



        hr = entry.GetInt ("slot", slot.slot);
        CHRF (hr, outError = format ("slots[{}]: missing or invalid 'slot' field", idx));

        CBRF (slot.slot >= kMinSlot && slot.slot <= kMaxSlot,
              outError = format ("slots[{}]: slot must be {}-{}, got {}",
                                 idx, kMinSlot, kMaxSlot, slot.slot));

        // Optional: device (don't pollute hr with the lookup result)
        HRESULT hrDev = entry.GetString ("device", slot.device);
        IGNORE_RETURN_VALUE (hrDev, S_OK);
        hasDev = !slot.device.empty ();

        // Optional: rom (don't pollute hr with the lookup result)
        HRESULT hrRom = entry.GetString ("rom", slot.rom);
        IGNORE_RETURN_VALUE (hrRom, S_OK);
        hasRom = !slot.rom.empty ();

        CBRF (hasDev || hasRom,
              outError = format ("slots[{}]: must specify 'device' and/or 'rom'", idx));

        if (hasRom)
        {
            hr = ResolveRomFile (slot.rom,
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

        outConfig.slots.push_back (slot);
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
    const vector<fs::path> & searchPaths,
    MachineConfig          & outConfig,
    string                 & outError)
{
    return Load (jsonText, searchPaths, PathResolver::FindFile, outConfig, outError);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Load (with resolver)
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::Load (
    const string           & jsonText,
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

    CBRF (outConfig.cpu == "6502",
          outError = format ("Invalid CPU type: '{}' (expected '6502')", outConfig.cpu));

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

    hr = LoadSystemRom (*pSystemRom, searchPaths, resolver, outConfig, outError);
    CHR (hr);

    // Optional: characterRom (object)
    // Optional: characterRom (object)
    {
        const JsonValue * pOpt = nullptr;
        HRESULT           hrOpt = root.GetObject ("characterRom", pOpt);

        if (SUCCEEDED (hrOpt))
        {
            hr = LoadCharacterRom (*pOpt, searchPaths, resolver, outConfig, outError);
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
            hr = LoadSlots (*pOpt, searchPaths, resolver, outConfig, outError);
            CHR (hr);
        }
    }

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
//  GetValue (template, kept for compatibility with internal use)
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
        for (size_t i = 0; i < pModes->ArraySize (); i++)
        {
            if (pModes->ArrayAt (i).GetType () == JsonType::String)
            {
                outConfig.videoConfig.modes.push_back (pModes->ArrayAt (i).GetString ());
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
