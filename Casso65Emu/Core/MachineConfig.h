#pragma once

#include "Pch.h"
#include "JsonParser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryRegion
//
////////////////////////////////////////////////////////////////////////////////

struct MemoryRegion
{
    std::string type;       // "ram" or "rom"
    Word        start;
    Word        end;
    std::string file;       // Required for ROM
    std::string bank;       // Optional: "aux"
    std::string target;     // Optional: "chargen"
};





////////////////////////////////////////////////////////////////////////////////
//
//  DeviceConfig
//
////////////////////////////////////////////////////////////////////////////////

struct DeviceConfig
{
    std::string type;
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
    std::vector<std::string> modes;
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
    std::string                 name;
    std::string                 cpu;
    uint32_t                    clockSpeed;
    std::vector<MemoryRegion>   memoryRegions;
    std::vector<DeviceConfig>   devices;
    VideoConfig                 videoConfig;
    std::string                 keyboardType;

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
        const std::string & jsonText,
        const std::string & basePath,
        MachineConfig & outConfig,
        std::string & outError);

private:
    static HRESULT ParseHexAddress (const std::string & str, Word & outAddr, std::string & outError);
};
