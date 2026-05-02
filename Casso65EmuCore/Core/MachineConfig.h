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
    Word   address    = 0;
    Word   start      = 0;
    Word   end        = 0;
    int    slot       = 0;
    bool   hasAddress = false;
    bool   hasRange   = false;
    bool   hasSlot    = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  VideoConfig
//
////////////////////////////////////////////////////////////////////////////////

struct VideoConfig
{
    vector<string> modes;
    int            width  = 560;
    int            height = 384;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfig
//
////////////////////////////////////////////////////////////////////////////////

// Apple II NTSC timing derived from the 14.31818 MHz crystal:
//   CPU clock  = 14,318,180 / 14        = 1,022,727 Hz
//   Cycles/frame = 65 cycles/line * 262 lines = 17,030
//   Frame rate = 1,022,727 / 17,030     = 60.05 Hz
static constexpr uint32_t kAppleCpuClock      = 1022727;
static constexpr uint32_t kAppleCyclesPerFrame = 17030;

struct MachineConfig
{
    string                 name;
    string                 cpu;
    uint32_t               clockSpeed     = kAppleCpuClock;
    uint32_t               cyclesPerFrame = kAppleCyclesPerFrame;
    vector<MemoryRegion>   memoryRegions;
    vector<DeviceConfig>   devices;
    VideoConfig            videoConfig;
    string                 keyboardType;
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
