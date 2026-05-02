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
        CBR_SetError (false, outError = format ("Invalid address format: '{}' (expected 0xNNNN or $NNNN)", str));
    }

    val = strtoul (pszHex, &pEnd, 16);

    CBR_SetError (pEnd != pszHex && *pEnd == '\0', outError = format ("Invalid hex digits in address: '{}'",    str));
    CBR_SetError (val <= 0xFFFF,                   outError = format ("Address out of range: '{}' (max $FFFF)", str));

    outAddr = static_cast<Word> (val);


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
    HRESULT              hr         = S_OK;
    JsonValue            root;
    JsonParseError       parseError;
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
    CHR_SetError (root.GetString ("name", outConfig.name),
                  outError = "Missing or invalid field: 'name'");

    CHR_SetError (root.GetString ("cpu", outConfig.cpu),
                  outError = "Missing or invalid field: 'cpu'");

    CBR_SetError (outConfig.cpu == "6502",
                  outError = format ("Invalid CPU type: '{}' (expected '6502')", outConfig.cpu));

    CHR_SetError (root.GetUint32 ("clockSpeed", outConfig.clockSpeed),
                  outError = "Missing or invalid field: 'clockSpeed'");

    // Required sub-structures
    CHR_SetError (root.GetArray ("memory", pMemArray),
                  outError = "Missing required field: 'memory'");

    hr = LoadMemoryRegions (*pMemArray, searchPaths, outConfig, outError);
    CHR (hr);

    CHR_SetError (root.GetArray ("devices", pDevArray),
                  outError = "Missing required field: 'devices'");

    hr = LoadDevices (*pDevArray, outConfig, outError);
    CHR (hr);

    CHR_SetError (root.GetObject ("video", pVideo),
                  outError = "Missing required field: 'video'");
    LoadVideoConfig (*pVideo, outConfig);

    CHR_SetError (root.GetObject ("keyboard", pKeyboard),
                  outError = "Missing required field: 'keyboard'");
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
    MachineConfig          & outConfig,
    string                 & outError)
{
    HRESULT hr = S_OK;



    for (size_t i = 0; i < memArray.ArraySize(); i++)
    {
        const JsonValue & entry = memArray.ArrayAt (i);
        MemoryRegion      region;
        string            addrStr;
        fs::path          romRelPath;
        fs::path          found;

        CBR (entry.IsObject());

        hr = entry.GetString ("type", region.type);
        CBR_SetError (SUCCEEDED (hr), outError = format ("memory[{}]: missing 'type' field", i));

        hr = entry.GetString ("start", addrStr);
        CBR_SetError (SUCCEEDED (hr), outError = format ("memory[{}]: missing 'start' field", i));
        hr = ParseHexAddress (addrStr, region.start, outError);
        CHR (hr);

        hr = entry.GetString ("end", addrStr);
        CBR_SetError (SUCCEEDED (hr), outError = format ("memory[{}]: missing 'end' field", i));
        hr = ParseHexAddress (addrStr, region.end, outError);
        CHR (hr);

        CBR_SetError (region.end >= region.start,
                      outError = format ("memory[{}]: end (0x{:04X}) < start (0x{:04X})",
                                         i, region.end, region.start));

        entry.GetString ("file", region.file);
        entry.GetString ("bank", region.bank);
        entry.GetString ("target", region.target);

        CBR_SetError (!(region.type == "rom" && region.file.empty() && region.target.empty()),
                      outError = format ("memory[{}]: ROM region requires 'file' field", i));

        if (!region.file.empty())
        {
            romRelPath = fs::path ("roms") / region.file;
            found      = PathResolver::FindFile (searchPaths, romRelPath);

            CBR_SetError (!found.empty(),
                          outError = format ("ROM file not found: roms/{}. "
                                            "Run scripts/FetchRoms.ps1 to download ROM images.",
                                            region.file));

            region.resolvedPath = found.string();
        }

        outConfig.memoryRegions.push_back (region);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadDevices
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::LoadDevices (
    const JsonValue & devArray,
    MachineConfig   & outConfig,
    string          & outError)
{
    HRESULT hr = S_OK;



    for (size_t i = 0; i < devArray.ArraySize(); i++)
    {
        const JsonValue & entry = devArray.ArrayAt (i);
        DeviceConfig      device;
        string            addrStr;

        CBR (entry.IsObject());

        hr = entry.GetString ("type", device.type);
        CBR_SetError (SUCCEEDED (hr), outError = format ("devices[{}]: missing 'type' field", i));

        if (SUCCEEDED (entry.GetString ("address", addrStr)))
        {
            hr = ParseHexAddress (addrStr, device.address, outError);
            CHR (hr);

            device.start      = device.address;
            device.end        = device.address;
            device.hasAddress = true;
        }

        if (SUCCEEDED (entry.GetString ("start", addrStr)))
        {
            hr = ParseHexAddress (addrStr, device.start, outError);
            CHR (hr);

            hr = entry.GetString ("end", addrStr);
            CBR_SetError (SUCCEEDED (hr), outError = format ("devices[{}]: missing 'end' field", i));
            hr = ParseHexAddress (addrStr, device.end, outError);
            CHR (hr);

            device.hasRange = true;
        }

        if (SUCCEEDED (entry.GetInt ("slot", device.slot)))
        {
            device.hasSlot = true;

            CBR_SetError (device.slot >= 1 && device.slot <= 7,
                          outError = format ("devices[{}]: slot must be 1-7, got {}",
                                            i, device.slot));

            device.start = static_cast<Word> (0xC080 + device.slot * 16);
            device.end   = static_cast<Word> (0xC08F + device.slot * 16);
        }

        outConfig.devices.push_back (device);
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
    const JsonValue * pModes = nullptr;



    if (SUCCEEDED (video.GetArray ("modes", pModes)))
    {
        for (size_t i = 0; i < pModes->ArraySize (); i++)
        {
            if (pModes->ArrayAt (i).IsString ())
            {
                outConfig.videoConfig.modes.push_back (pModes->ArrayAt (i).GetString ());
            }
        }
    }

    video.GetInt ("width", outConfig.videoConfig.width);
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




