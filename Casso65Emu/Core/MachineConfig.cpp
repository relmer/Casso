#include "Pch.h"

#include "MachineConfig.h"
#include "JsonParser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ParseHexAddress
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigLoader::ParseHexAddress (const std::string & str, Word & outAddr, std::string & outError)
{
    HRESULT hr = S_OK;

    if (str.size () >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
    {
        unsigned long val = std::strtoul (str.c_str (), nullptr, 16);
        outAddr = static_cast<Word> (val);
    }
    else if (str.size () >= 1 && str[0] == '$')
    {
        unsigned long val = std::strtoul (str.c_str () + 1, nullptr, 16);
        outAddr = static_cast<Word> (val);
    }
    else
    {
        outError = std::format ("Invalid address format: '{}' (expected 0xNNNN or $NNNN)", str);
        hr = E_FAIL;
        goto Error;
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
    const std::string & jsonText,
    const std::string & basePath,
    MachineConfig & outConfig,
    std::string & outError)
{
    HRESULT        hr = S_OK;
    JsonValue      root;
    JsonParseError parseError;

    // Parse JSON
    if (FAILED (JsonParser::Parse (jsonText, root, parseError)))
    {
        outError = std::format ("JSON parse error at line {}, column {}: {}",
            parseError.line, parseError.column, parseError.message);
        hr = E_FAIL;
        goto Error;
    }

    CBREx (root.IsObject (), E_FAIL);

    // Required: name
    if (!root.HasKey ("name") || !root.Get ("name").IsString ())
    {
        outError = "Missing required field: 'name'";
        hr = E_FAIL;
        goto Error;
    }
    outConfig.name = root.Get ("name").GetString ();

    // Required: cpu
    if (!root.HasKey ("cpu") || !root.Get ("cpu").IsString ())
    {
        outError = "Missing required field: 'cpu'";
        hr = E_FAIL;
        goto Error;
    }
    outConfig.cpu = root.Get ("cpu").GetString ();

    if (outConfig.cpu != "6502" && outConfig.cpu != "65c02")
    {
        outError = std::format ("Invalid CPU type: '{}' (expected '6502' or '65c02')", outConfig.cpu);
        hr = E_FAIL;
        goto Error;
    }

    // Required: clockSpeed
    if (!root.HasKey ("clockSpeed") || !root.Get ("clockSpeed").IsNumber ())
    {
        outError = "Missing required field: 'clockSpeed'";
        hr = E_FAIL;
        goto Error;
    }
    outConfig.clockSpeed = static_cast<uint32_t> (root.Get ("clockSpeed").GetNumber ());

    // Required: memory array
    if (!root.HasKey ("memory") || !root.Get ("memory").IsArray ())
    {
        outError = "Missing required field: 'memory'";
        hr = E_FAIL;
        goto Error;
    }

    {
        const JsonValue & memArray = root.Get ("memory");

        for (size_t i = 0; i < memArray.ArraySize (); i++)
        {
            const JsonValue & entry = memArray.ArrayAt (i);
            MemoryRegion region;

            CBREx (entry.IsObject (), E_FAIL);

            if (!entry.HasKey ("type") || !entry.Get ("type").IsString ())
            {
                outError = std::format ("memory[{}]: missing 'type' field", i);
                hr = E_FAIL;
                goto Error;
            }
            region.type = entry.Get ("type").GetString ();

            if (!entry.HasKey ("start") || !entry.Get ("start").IsString ())
            {
                outError = std::format ("memory[{}]: missing 'start' field", i);
                hr = E_FAIL;
                goto Error;
            }
            CHR (ParseHexAddress (entry.Get ("start").GetString (), region.start, outError));

            if (!entry.HasKey ("end") || !entry.Get ("end").IsString ())
            {
                outError = std::format ("memory[{}]: missing 'end' field", i);
                hr = E_FAIL;
                goto Error;
            }
            CHR (ParseHexAddress (entry.Get ("end").GetString (), region.end, outError));

            if (region.end < region.start)
            {
                outError = std::format ("memory[{}]: end (0x{:04X}) < start (0x{:04X})",
                    i, region.end, region.start);
                hr = E_FAIL;
                goto Error;
            }

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

            // ROM entries must have a file
            if (region.type == "rom" && region.file.empty () && region.target.empty ())
            {
                outError = std::format ("memory[{}]: ROM region requires 'file' field", i);
                hr = E_FAIL;
                goto Error;
            }

            // Validate ROM file exists
            if (!region.file.empty ())
            {
                std::string romPath = basePath + "/roms/" + region.file;
                std::ifstream test (romPath, std::ios::binary);

                if (!test.good ())
                {
                    outError = std::format (
                        "ROM file not found: roms/{}. Place Apple II ROM images in the roms/ directory.",
                        region.file);
                    hr = E_FAIL;
                    goto Error;
                }
            }

            outConfig.memoryRegions.push_back (region);
        }
    }

    // Required: devices array
    if (!root.HasKey ("devices") || !root.Get ("devices").IsArray ())
    {
        outError = "Missing required field: 'devices'";
        hr = E_FAIL;
        goto Error;
    }

    {
        const JsonValue & devArray = root.Get ("devices");

        for (size_t i = 0; i < devArray.ArraySize (); i++)
        {
            const JsonValue & entry = devArray.ArrayAt (i);
            DeviceConfig device;

            CBREx (entry.IsObject (), E_FAIL);

            if (!entry.HasKey ("type") || !entry.Get ("type").IsString ())
            {
                outError = std::format ("devices[{}]: missing 'type' field", i);
                hr = E_FAIL;
                goto Error;
            }
            device.type = entry.Get ("type").GetString ();

            // Parse address mapping — exactly one of: address, start+end, slot
            if (entry.HasKey ("address") && entry.Get ("address").IsString ())
            {
                CHR (ParseHexAddress (entry.Get ("address").GetString (), device.address, outError));
                device.start      = device.address;
                device.end        = device.address;
                device.hasAddress = true;
            }

            if (entry.HasKey ("start") && entry.Get ("start").IsString () &&
                entry.HasKey ("end") && entry.Get ("end").IsString ())
            {
                CHR (ParseHexAddress (entry.Get ("start").GetString (), device.start, outError));
                CHR (ParseHexAddress (entry.Get ("end").GetString (), device.end, outError));
                device.hasRange = true;
            }

            if (entry.HasKey ("slot") && entry.Get ("slot").IsNumber ())
            {
                device.slot    = entry.Get ("slot").GetInt ();
                device.hasSlot = true;

                if (device.slot < 1 || device.slot > 7)
                {
                    outError = std::format ("devices[{}]: slot must be 1-7, got {}", i, device.slot);
                    hr = E_FAIL;
                    goto Error;
                }

                // Auto-map slot addresses
                device.start = static_cast<Word> (0xC080 + device.slot * 16);
                device.end   = static_cast<Word> (0xC08F + device.slot * 16);
            }

            outConfig.devices.push_back (device);
        }
    }

    // Required: video object
    if (!root.HasKey ("video") || !root.Get ("video").IsObject ())
    {
        outError = "Missing required field: 'video'";
        hr = E_FAIL;
        goto Error;
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
    if (!root.HasKey ("keyboard") || !root.Get ("keyboard").IsObject ())
    {
        outError = "Missing required field: 'keyboard'";
        hr = E_FAIL;
        goto Error;
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
