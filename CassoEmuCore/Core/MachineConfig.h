#pragma once

#include "Pch.h"
#include "JsonParser.h"
#include "PathResolver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RamRegion
//
////////////////////////////////////////////////////////////////////////////////

struct RamRegion
{
    Word    address = 0;
    Word    size    = 0;       // 1..0x10000 (full 64K)
    string  bank;              // Optional: "aux"
};





////////////////////////////////////////////////////////////////////////////////
//
//  RomReference
//
//  Describes a ROM chip mapped on the CPU bus (system ROM, slot ROM).
//  The end address is implicit from the file size.
//
////////////////////////////////////////////////////////////////////////////////

struct RomReference
{
    Word    address      = 0;
    string  file;
    string  resolvedPath;
    size_t  fileSize     = 0;  // Populated after resolution
};





////////////////////////////////////////////////////////////////////////////////
//
//  CharacterRomReference
//
//  Character generator ROM is not mapped on the CPU bus -- it's read by the
//  video circuitry. Only the file matters.
//
////////////////////////////////////////////////////////////////////////////////

struct CharacterRomReference
{
    string  file;
    string  resolvedPath;
    size_t  fileSize     = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  InternalDevice
//
//  Motherboard device with hardcoded address mapping. Just a type name.
//
////////////////////////////////////////////////////////////////////////////////

struct InternalDevice
{
    string  type;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DeviceConfig
//
//  Lightweight container passed to ComponentRegistry factory functions.
//  Used to communicate per-instance configuration like slot number.
//
////////////////////////////////////////////////////////////////////////////////

struct DeviceConfig
{
    string  type;
    int     slot      = 0;
    bool    hasSlot   = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SlotConfig
//
//  A slot 1-7 with an optional device implementation and an optional slot ROM.
//  At least one of `device` or `rom` must be specified.
//
////////////////////////////////////////////////////////////////////////////////

struct SlotConfig
{
    int     slot         = 0;     // 1..7
    string  device;               // Optional: registered device type
    string  rom;                  // Optional: slot ROM filename
    string  resolvedRomPath;
    size_t  romSize      = 0;
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
    string                      name;
    string                      cpu;

    // Timing (parsed from "timing" section)
    VideoStandard               videoStandard     = VideoStandard::NTSC;
    uint32_t                    clockSpeed        = kAppleCpuClock;
    uint32_t                    cyclesPerScanline = kCyclesPerScanline;
    uint32_t                    scanlinesPerFrame = kNtscScanlines;
    uint32_t                    cyclesPerFrame    = kAppleCyclesPerFrame;

    vector<RamRegion>           ram;
    RomReference                systemRom;
    CharacterRomReference       characterRom;
    vector<InternalDevice>      internalDevices;
    vector<SlotConfig>          slots;
    VideoConfig                 videoConfig;
    string                      keyboardType;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfigLoader
//
////////////////////////////////////////////////////////////////////////////////

class MachineConfigLoader
{
public:
    // Callable that resolves a relative path given search directories.
    // Returns the resolved path, or empty path if not found.
    using FileResolver = function<fs::path (const vector<fs::path> &,
                                            const fs::path &)>;

    // `machineName` selects the per-machine ROM subdir
    // (Machines/<machineName>/<file>) used to resolve systemRom and
    // characterRom files. Slot ROMs whose device is "disk-ii" resolve
    // under Devices/DiskII/<file>. Pass the bare machine identifier
    // (e.g., "Apple2e"), not a localized display name.
    static HRESULT Load            (const string           & jsonText,
                                    const string           & machineName,
                                    const vector<fs::path> & searchPaths,
                                    MachineConfig          & outConfig,
                                    string                 & outError);

    static HRESULT Load            (const string           & jsonText,
                                    const string           & machineName,
                                    const vector<fs::path> & searchPaths,
                                    const FileResolver     & resolver,
                                    MachineConfig          & outConfig,
                                    string                 & outError);

    static HRESULT CollectRomFiles (const string           & jsonText,
                                    vector<string>         & outFiles,
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
    static HRESULT ParseHexSize       (const string & str, uint32_t & outSize, string & outError);

    static HRESULT LoadTiming         (const JsonValue & timing,
                                       MachineConfig   & outConfig,
                                       string          & outError);

    static HRESULT LoadRam            (const JsonValue & ramArray,
                                       MachineConfig   & outConfig,
                                       string          & outError);

    static HRESULT LoadSystemRom      (const JsonValue        & sysRomObj,
                                       const string           & machineName,
                                       const vector<fs::path> & searchPaths,
                                       const FileResolver     & resolver,
                                       MachineConfig          & outConfig,
                                       string                 & outError);

    static HRESULT LoadCharacterRom   (const JsonValue        & charRomObj,
                                       const string           & machineName,
                                       const vector<fs::path> & searchPaths,
                                       const FileResolver     & resolver,
                                       MachineConfig          & outConfig,
                                       string                 & outError);

    static HRESULT LoadInternalDevices (const JsonValue & devArray,
                                        MachineConfig   & outConfig,
                                        string          & outError);

    static HRESULT LoadSlots          (const JsonValue        & slotsArray,
                                       const string           & machineName,
                                       const vector<fs::path> & searchPaths,
                                       const FileResolver     & resolver,
                                       MachineConfig          & outConfig,
                                       string                 & outError);

    static void    LoadVideoConfig    (const JsonValue & video,    MachineConfig & outConfig);
    static void    LoadKeyboardConfig (const JsonValue & keyboard, MachineConfig & outConfig);
};
