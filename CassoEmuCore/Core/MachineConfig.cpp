#include "Pch.h"

#include "MachineConfig.h"
#include "JsonParser.h"
#include "PathResolver.h"





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
    CBRF (val <= 0xFFFF,                   outError = format ("Address out of range: '{}' (max $FFFF)", str));

    outAddr = static_cast<Word> (val);


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
    HRESULT              hr         = S_OK;
    JsonValue            root;
    JsonParseError       parseError;
    const JsonValue    * pTiming    = nullptr;
    const JsonValue    * pMemArray  = nullptr;
    const JsonValue    * pDevArray  = nullptr;
    const JsonValue    * pVideo     = nullptr;
    const JsonValue    * pKeyboard  = nullptr;



    // Parse JSON
    hr = JsonParser::Parse (jsonText, root, parseError);

    if (FAILED (hr))
    {
        outError = format ("JSON parse error at line {}, column {}: {}",
                           parseError.line, parseError.column, parseError.message);
    }

    CHR (hr);

    // Required fields
    hr = root.GetString ("name", outConfig.name);
    CHRF (hr, outError = "Missing or invalid field: 'name'");

    hr = root.GetString ("cpu", outConfig.cpu);
    CHRF (hr, outError = "Missing or invalid field: 'cpu'");

    CBRF (outConfig.cpu == "6502",
          outError = format ("Invalid CPU type: '{}' (expected '6502')", outConfig.cpu));

    // Required: timing object
    hr = root.GetObject ("timing", pTiming);
    CHRF (hr, outError = "Missing required field: 'timing'");

    hr = LoadTiming (*pTiming, outConfig, outError);
    CHR (hr);

    // Required sub-structures
    hr = root.GetArray ("memory", pMemArray);
    CHRF (hr, outError = "Missing required field: 'memory'");

    hr = LoadMemoryRegions (*pMemArray, searchPaths, resolver, outConfig, outError);
    CHR (hr);

    hr = root.GetArray ("devices", pDevArray);
    CHRF (hr, outError = "Missing required field: 'devices'");

    hr = LoadDevices (*pDevArray, outConfig, outError);
    CHR (hr);

    hr = root.GetObject ("video", pVideo);
    CHRF (hr, outError = "Missing required field: 'video'");
    LoadVideoConfig (*pVideo, outConfig);

    hr = root.GetObject ("keyboard", pKeyboard);
    CHRF (hr, outError = "Missing required field: 'keyboard'");
    LoadKeyboardConfig (*pKeyboard, outConfig);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadMemoryRegions
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::LoadMemoryRegions (
    const JsonValue        & memArray,
    const vector<fs::path> & searchPaths,
    const FileResolver     & resolver,
    MachineConfig          & outConfig,
    string                 & outError)
{
    static const Field<MemoryRegion> krgFields[] =
    {
        { "type",   true,  &MemoryRegion::type, nullptr, nullptr },
        { "start",  true,  nullptr,               &MemoryRegion::start, nullptr },
        { "end",    true,  nullptr,               &MemoryRegion::end, nullptr },
        { "file",   false, &MemoryRegion::file, nullptr, nullptr },
        { "bank",   false, &MemoryRegion::bank, nullptr, nullptr },
        { "target", false, &MemoryRegion::target, nullptr, nullptr },
    };

    HRESULT   hr        = S_OK;
    size_t    idxRegion = 0;
    size_t    idxField  = 0;
    fs::path  romRelPath;
    fs::path  found;



    for (idxRegion = 0; idxRegion < memArray.ArraySize(); idxRegion++)
    {
        const JsonValue & entry = memArray.ArrayAt (idxRegion);
        MemoryRegion      region;



        for (idxField = 0; idxField < _countof (krgFields); idxField++)
        {
            const Field<MemoryRegion> & f = krgFields[idxField];



            hr = GetValue (entry, f, region, outError);
            if (f.fRequired)
            {
                CHR (hr);
            }
            else 
            {
                IGNORE_RETURN_VALUE (hr, S_OK);
            }
        }

        CBRF (region.end >= region.start,
              outError = format ("memory[{}]: end (${:04X}) < start (${:04X})",
                                 idxRegion, 
                                 region.end, 
                                 region.start));

        CBRF (!(region.type == "rom" && region.file.empty() && region.target.empty()),
              outError = format ("memory[{}]: ROM region requires 'file' field", 
                                 idxRegion));

        // Resolve ROM file path
        if (!region.file.empty())
        {
            romRelPath = fs::path ("ROMs") / region.file;
            found      = resolver (searchPaths, romRelPath);

            CBRF (!found.empty(),
                  outError = format ("ROM file not found: ROMs/{}. "
                                     "Run scripts/FetchRoms.ps1 to download ROM images.",
                                     region.file));

            region.resolvedPath = found.string();
        }

        outConfig.memoryRegions.push_back (region);
    }

Error:
    // Populate error message from indices if we bailed out
    if (FAILED (hr) && outError.empty())
    {
        if (idxField < _countof (krgFields))
        {
            outError = format ("memory[{}]: missing or invalid '{}' field",
                               idxRegion, 
                               krgFields[idxField].key);
        }
        else
        {
            outError = format ("memory[{}]: invalid configuration", 
                               idxRegion);
        }
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfigLoader::GetValue
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
//  MachineConfigLoader::LoadDevices
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::LoadDevices (
    const JsonValue & devArray,
    MachineConfig   & outConfig,
    string          & outError)
{
    static const Field<DeviceConfig> krgFields[] =
    {
        { "type",    true,  &DeviceConfig::type, nullptr,                nullptr             },
        { "address", false, nullptr,             &DeviceConfig::address, nullptr             },
        { "start",   false, nullptr,             &DeviceConfig::start,   nullptr             },
        { "end",     false, nullptr,             &DeviceConfig::end,     nullptr             },
        { "slot",    false, nullptr,             nullptr,                &DeviceConfig::slot },
    };

    HRESULT hr       = S_OK;
    size_t  idxDev   = 0;
    size_t  idxField = 0;



    for (idxDev = 0; idxDev < devArray.ArraySize(); idxDev++)
    {
        const JsonValue & entry  = devArray.ArrayAt (idxDev);
        DeviceConfig      device;



        for (idxField = 0; idxField < _countof (krgFields); idxField++)
        {
            const Field<DeviceConfig> & f = krgFields[idxField];



            hr = GetValue (entry, f, device, outError);
            if (f.fRequired)
            {
                CHR (hr);
            }
            else
            {
                IGNORE_RETURN_VALUE (hr, S_OK);
            }
        }

        // Post-process address mapping flags
        if (device.address != 0)
        {
            device.start      = device.address;
            device.end        = device.address;
            device.hasAddress = true;
        }

        if (device.start != 0 && device.end != 0 && !device.hasAddress)
        {
            device.hasRange = true;
        }

        if (device.slot != 0)
        {
            device.hasSlot = true;

            CBRF (device.slot >= 1 && device.slot <= 7,
                  outError = format ("devices[{}]: slot must be 1-7, got {}",
                                     idxDev, 
                                     device.slot));

            device.start = static_cast<Word> (0xC080 + device.slot * 16);
            device.end   = static_cast<Word> (0xC08F + device.slot * 16);
        }

        outConfig.devices.push_back (device);
    }

Error:
    if (FAILED (hr) && outError.empty())
    {
        if (idxField < _countof (krgFields))
        {
            outError = format ("devices[{}]: missing or invalid '{}' field",
                               idxDev,
                               krgFields[idxField].key);
        }
        else
        {
            outError = format ("devices[{}]: invalid configuration", 
                               idxDev);
        }
    }

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




