#pragma once

#include "Pch.h"
#include "JsonParser.h"
#include "PathResolver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryRegion
//
////////////////////////////////////////////////////////////////////////////////

struct MemoryRegion
{
    string type;           // "ram" or "rom"
    Word        start = 0;
    Word        end   = 0;
    string file;           // Required for ROM (from JSON)
    string resolvedPath;   // Fully resolved ROM path after search
    string bank;           // Optional: "aux"
    string target;         // Optional: "chargen"
};





////////////////////////////////////////////////////////////////////////////////
//
//  DeviceConfig
//
////////////////////////////////////////////////////////////////////////////////

struct DeviceConfig
{
    string type;
    Word        address;    // For point-mapped devices
    Word        start;      // For range-mapped devices
    Word        end;        // For range-mapped devices
    int         slot;       // 1-7 for slot-based devices
    bool        hasAddress;
    bool        hasRange;
    bool        hasSlot;

    DeviceConfig ()
        : address    (0),
          start      (0),
          end        (0),
          slot       (0),
          hasAddress (false),
          hasRange   (false),
          hasSlot    (false)
    {
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  VideoConfig
//
////////////////////////////////////////////////////////////////////////////////

struct VideoConfig
{
    vector<string> modes;
    int width;
    int height;

    VideoConfig ()
        : width  (560),
          height (384)
    {
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfig
//
////////////////////////////////////////////////////////////////////////////////

struct MachineConfig
{
    string                 name;
    string                 cpu;
    uint32_t                    clockSpeed;
    vector<MemoryRegion>   memoryRegions;
    vector<DeviceConfig>   devices;
    VideoConfig                 videoConfig;
    string                 keyboardType;

    MachineConfig ()
        : clockSpeed (1023000)
    {
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfigLoader
//
////////////////////////////////////////////////////////////////////////////////

class MachineConfigLoader
{
public:
    static HRESULT Load (
        const string & jsonText,
        const vector<fs::path> & searchPaths,
        MachineConfig & outConfig,
        string & outError);

private:
    static HRESULT ParseHexAddress (const string & str, Word & outAddr, string & outError);
};
