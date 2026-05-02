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
    HRESULT hr = S_OK;

    if (str.size () >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
    {
        unsigned long val = strtoul (str.c_str (), nullptr, 16);
        outAddr = static_cast<Word> (val);
    }
    else if (str.size () >= 1 && str[0] == '$')
    {
        unsigned long val = strtoul (str.c_str () + 1, nullptr, 16);
        outAddr = static_cast<Word> (val);
    }
    else
    {
        CBR_SetError (false, outError = format ("Invalid address format: '{}' (expected 0xNNNN or $NNNN)", str));
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
    const string & jsonText,
    const vector<fs::path> & searchPaths,
    MachineConfig & outConfig,
    string & outError)
{
    HRESULT        hr = S_OK;
    JsonValue      root;
    JsonParseError parseError;

    // Parse JSON
    hr = JsonParser::Parse (jsonText, root, parseError);

    {
        bool parseOk = SUCCEEDED (hr);

        if (!parseOk)
        {
            outError = format ("JSON parse error at line {}, column {}: {}",
                parseError.line, parseError.column, parseError.message);
        }
        CBR (parseOk);
    }

    CBR (root.IsObject ());

    // Required: name
    {
        bool hasName = root.HasKey ("name") && root.Get ("name").IsString ();
        CBR_SetError (hasName, outError = "Missing required field: 'name'");
    }
    outConfig.name = root.Get ("name").GetString ();

    // Required: cpu
    {
        bool hasCpu = root.HasKey ("cpu") && root.Get ("cpu").IsString ();
        CBR_SetError (hasCpu, outError = "Missing required field: 'cpu'");
    }
    outConfig.cpu = root.Get ("cpu").GetString ();

    {
        bool validCpu = (outConfig.cpu == "6502");
        CBR_SetError (validCpu, outError = format ("Invalid CPU type: '{}' (expected '6502')", outConfig.cpu));
    }

    // Required: clockSpeed
    {
        bool hasClockSpeed = root.HasKey ("clockSpeed") && root.Get ("clockSpeed").IsNumber ();
        CBR_SetError (hasClockSpeed, outError = "Missing required field: 'clockSpeed'");
    }
    outConfig.clockSpeed = static_cast<uint32_t> (root.Get ("clockSpeed").GetNumber ());

    // Required: memory array
    {
        bool hasMemory = root.HasKey ("memory") && root.Get ("memory").IsArray ();
        CBR_SetError (hasMemory, outError = "Missing required field: 'memory'");
    }

    {
        const JsonValue & memArray = root.Get ("memory");

        for (size_t i = 0; i < memArray.ArraySize (); i++)
        {
            const JsonValue & entry = memArray.ArrayAt (i);
            MemoryRegion region;

            CBR (entry.IsObject ());

            {
                bool hasType = entry.HasKey ("type") && entry.Get ("type").IsString ();
                CBR_SetError (hasType, outError = format ("memory[{}]: missing 'type' field", i));
            }
            region.type = entry.Get ("type").GetString ();

            {
                bool hasStart = entry.HasKey ("start") && entry.Get ("start").IsString ();
                CBR_SetError (hasStart, outError = format ("memory[{}]: missing 'start' field", i));
            }
            {
                const string & startStr = entry.Get ("start").GetString ();
                hr = ParseHexAddress (startStr, region.start, outError);
                CHR (hr);
            }

            {
                bool hasEnd = entry.HasKey ("end") && entry.Get ("end").IsString ();
                CBR_SetError (hasEnd, outError = format ("memory[{}]: missing 'end' field", i));
            }
            {
                const string & endStr = entry.Get ("end").GetString ();
                hr = ParseHexAddress (endStr, region.end, outError);
                CHR (hr);
            }

            CBR_SetError (region.end >= region.start, outError = format ("memory[{}]: end (0x{:04X}) < start (0x{:04X})", i, region.end, region.start));

            if (entry.HasKey ("file") && entry.Get ("file").IsString ())
            {
                region.file = entry.Get ("file").GetString ();
            }

            if (entry.HasKey ("bank") && entry.Get ("bank").IsString ())
            {
                region.bank = entry.Get ("bank").GetString ();
            }

            if (entry.HasKey ("target") && entry.Get ("target").IsString ())
            {
                region.target = entry.Get ("target").GetString ();
            }

            {
                bool romHasFile = !(region.type == "rom" && region.file.empty () && region.target.empty ());
                CBR_SetError (romHasFile, outError = format ("memory[{}]: ROM region requires 'file' field", i));
            }

            // Validate ROM file exists — search all paths independently
            if (!region.file.empty ())
            {
                fs::path romRelPath = fs::path ("roms") / region.file;
                fs::path found      = PathResolver::FindFile (searchPaths, romRelPath);

                CBR_SetError (!found.empty (), outError = format (
                    "ROM file not found: roms/{}. Run scripts/FetchRoms.ps1 to download ROM images.",
                    region.file));

                region.resolvedPath = found.string ();
            }

            outConfig.memoryRegions.push_back (region);
        }
    }

    // Required: devices array
    {
        bool hasDevices = root.HasKey ("devices") && root.Get ("devices").IsArray ();
        CBR_SetError (hasDevices, outError = "Missing required field: 'devices'");
    }

    {
        const JsonValue & devArray = root.Get ("devices");

        for (size_t i = 0; i < devArray.ArraySize (); i++)
        {
            const JsonValue & entry = devArray.ArrayAt (i);
            DeviceConfig device;

            CBR (entry.IsObject ());

            {
                bool hasType = entry.HasKey ("type") && entry.Get ("type").IsString ();
                CBR_SetError (hasType, outError = format ("devices[{}]: missing 'type' field", i));
            }
            device.type = entry.Get ("type").GetString ();

            // Parse address mapping — exactly one of: address, start+end, slot
            if (entry.HasKey ("address") && entry.Get ("address").IsString ())
            {
                const string & addrStr = entry.Get ("address").GetString ();
                hr = ParseHexAddress (addrStr, device.address, outError);
                CHR (hr);
                device.start      = device.address;
                device.end        = device.address;
                device.hasAddress = true;
            }

            if (entry.HasKey ("start") && entry.Get ("start").IsString () &&
                entry.HasKey ("end") && entry.Get ("end").IsString ())
            {
                const string & devStartStr = entry.Get ("start").GetString ();
                hr = ParseHexAddress (devStartStr, device.start, outError);
                CHR (hr);
                const string & devEndStr = entry.Get ("end").GetString ();
                hr = ParseHexAddress (devEndStr, device.end, outError);
                CHR (hr);
                device.hasRange = true;
            }

            if (entry.HasKey ("slot") && entry.Get ("slot").IsNumber ())
            {
                device.slot    = entry.Get ("slot").GetInt ();
                device.hasSlot = true;

                CBR_SetError (device.slot >= 1 && device.slot <= 7, outError = format ("devices[{}]: slot must be 1-7, got {}", i, device.slot));

                // Auto-map slot addresses
                device.start = static_cast<Word> (0xC080 + device.slot * 16);
                device.end   = static_cast<Word> (0xC08F + device.slot * 16);
            }

            outConfig.devices.push_back (device);
        }
    }

    // Required: video object
    {
        bool hasVideo = root.HasKey ("video") && root.Get ("video").IsObject ();
        CBR_SetError (hasVideo, outError = "Missing required field: 'video'");
    }

    {
        const JsonValue & video = root.Get ("video");

        if (video.HasKey ("modes") && video.Get ("modes").IsArray ())
        {
            const JsonValue & modes = video.Get ("modes");

            for (size_t i = 0; i < modes.ArraySize (); i++)
            {
                if (modes.ArrayAt (i).IsString ())
                {
                    outConfig.videoConfig.modes.push_back (modes.ArrayAt (i).GetString ());
                }
            }
        }

        if (video.HasKey ("width") && video.Get ("width").IsNumber ())
        {
            outConfig.videoConfig.width = video.Get ("width").GetInt ();
        }

        if (video.HasKey ("height") && video.Get ("height").IsNumber ())
        {
            outConfig.videoConfig.height = video.Get ("height").GetInt ();
        }
    }

    // Required: keyboard object
    {
        bool hasKeyboard = root.HasKey ("keyboard") && root.Get ("keyboard").IsObject ();
        CBR_SetError (hasKeyboard, outError = "Missing required field: 'keyboard'");
    }

    {
        const JsonValue & keyboard = root.Get ("keyboard");

        if (keyboard.HasKey ("type") && keyboard.Get ("type").IsString ())
        {
            outConfig.keyboardType = keyboard.Get ("type").GetString ();
        }
    }

Error:
    return hr;
}
