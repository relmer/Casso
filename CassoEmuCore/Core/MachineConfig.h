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
    Word   start = 0;
    Word   end   = 0;
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
//  VideoStandard
//
////////////////////////////////////////////////////////////////////////////////

enum class VideoStandard
{
    NTSC,
    PAL
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfig
//
////////////////////////////////////////////////////////////////////////////////

// Apple II NTSC timing — all values derived from the 14.31818 MHz crystal
static constexpr uint32_t kNtscCrystalHz       = 14318180;  // 4x NTSC color burst (3,579,545 Hz)
static constexpr uint32_t kCrystalDivisor      = 14;        // 7 pixels/char * 2 phases
static constexpr uint32_t kCyclesPerScanline   = 65;        // 40 visible + 25 blanking
static constexpr uint32_t kScanlinesPerFrame   = 262;       // 192 visible + 70 blanking

static constexpr uint32_t kNtscScanlines       = 262;
static constexpr uint32_t kPalScanlines        = 312;

static constexpr uint32_t kAppleCpuClock       = kNtscCrystalHz / kCrystalDivisor;
static constexpr uint32_t kAppleCyclesPerFrame = kCyclesPerScanline * kScanlinesPerFrame;





struct MachineConfig
{
    string                 name;
    string                 cpu;

    // Timing (parsed from "timing" section)
    VideoStandard          videoStandard     = VideoStandard::NTSC;
    uint32_t               clockSpeed        = kAppleCpuClock;
    uint32_t               cyclesPerScanline = kCyclesPerScanline;
    uint32_t               scanlinesPerFrame = kNtscScanlines;
    uint32_t               cyclesPerFrame    = kAppleCyclesPerFrame;

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
    static HRESULT Load (const string           & jsonText,
                         const vector<fs::path> & searchPaths,
                         MachineConfig          & outConfig,
                         string                 & outError);

private:
    template <typename T>
    struct Field
    {
        const char  * key;
        bool          fRequired;
        string T::  * strDest;
        Word   T::  * wDest;
        int    T::  * intDest;
    };

    template <typename T>
    static HRESULT GetValue (
        const JsonValue  & entry,
        const Field<T>   & f,
        T                & dest,
        string           & outError);



    static HRESULT ParseHexAddress    (const string & str, Word & outAddr, string & outError);

    static HRESULT LoadTiming        (const JsonValue & timing,
                                       MachineConfig   & outConfig,
                                       string          & outError);

    static HRESULT LoadMemoryRegions  (const JsonValue        & memArray,
                                       const vector<fs::path> & searchPaths,
                                       MachineConfig          & outConfig,
                                       string                 & outError);

    static HRESULT LoadDevices        (const JsonValue & devArray,
                                       MachineConfig   & outConfig,
                                       string          & outError);

    static void    LoadVideoConfig    (const JsonValue & video,    MachineConfig & outConfig);
    static void    LoadKeyboardConfig (const JsonValue & keyboard, MachineConfig & outConfig);
};
