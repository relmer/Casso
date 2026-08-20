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

    // Bank-switched system ROM (Apple //c): the file holds N banks of
    // romBankSize bytes, each mapped at `address`; a write to the
    // romBankSelect soft switch toggles the visible bank. Zero when the ROM
    // is a single flat image (the //e and earlier).
    Word    romBankSize   = 0;
    Word    romBankSelect = 0;
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
//  CapabilityFlag
//
//  Per FR-015 (007-ui-overhaul): describes whether a given internal
//  device or slot card may be disabled by the user.
//
//    optional        — user may toggle on/off freely.
//    required        — present by default; UI disables the checkbox.
//    platform-locked — physically present on this hardware; UI shows
//                      a tooltip with lockReason explaining why.
//
//  Default per FR-015: internal devices → required, slot entries → optional.
//
////////////////////////////////////////////////////////////////////////////////

enum class CapabilityFlag
{
    Optional,
    Required,
    PlatformLocked
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
    string          type;
    CapabilityFlag  capabilityFlag = CapabilityFlag::Required;
    string          lockReason;            // Optional: shown as tooltip when PlatformLocked.
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

//
//  One connector and whatever is plugged into it.
//
//  A PORT IS NOT A DRIVE, which is the whole reason this exists instead of a
//  drive count. The Disk II Interface has two ports taking one drive each; a
//  DuoDisk is ONE device on ONE port providing two drive units; a ProFile is a
//  different card in its own slot entirely. A count describes none of that.
//
//  `device` empty means the connector is present and nothing is on it -- which
//  is a real and different state from the connector not existing.
//
//  `name` identifies a connector that has an identity of its own: a //c's back
//  panel has a disk port, two serial ports and a joystick port, and they are
//  not interchangeable. A card's connectors are numbered rather than named, so
//  they leave it empty and are read positionally.
//
struct PortConfig
{
    string  name;      // "disk", "serial1", "joystick"; empty for numbered ports
    string  device;    // what is attached; empty for an unoccupied port
};


struct SlotConfig
{
    int             slot            = 0;     // 1..7
    string          device;                  // Optional: registered device type
    string          rom;                     // Optional: slot ROM filename
    string          resolvedRomPath;
    size_t          romSize         = 0;
    CapabilityFlag  capabilityFlag  = CapabilityFlag::Optional;
    string          lockReason;              // Optional: shown as tooltip when PlatformLocked.
    bool            enabled         = true;   // false => user disabled this slot (Settings > Hardware); skip install.

    // What is plugged into the CARD's connectors, in port order. Absent (an
    // empty vector) means this machine's own default, which is what every
    // config written before the key existed relies on.
    vector<PortConfig>  ports;
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
//  The parsed description of one machine: timing, memory map, ROMs, devices,
//  and slots.
//
//  Every timing constant here is DERIVED from the 14.31818 MHz crystal rather
//  than written as a rounded figure, because that one oscillator is where all
//  Apple II timing comes from. The CPU clock is the crystal divided by 14 (7
//  pixels per character times 2 phases), and cycles per frame is the product
//  of the scanline and frame counts. Writing 1023000 Hz directly would be
//  close enough to look right and wrong enough to drift the video phase.
//
//  Fields default to the NTSC Apple II values, so a config that omits a timing
//  key yields a working machine rather than a zeroed one.
//
//  HasEnabledSlotDevice is the question the UI actually asks -- "can this
//  machine print?" -- and it deliberately ignores a slot the user disabled in
//  Settings > Hardware, since a disabled card is not present as far as the
//  guest or the UI is concerned.
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

    // The machine's OWN connectors, for machines whose hardware is built in
    // rather than carded. A //c has no slots at all -- it has a back panel:
    // a disk port, two serial ports, a joystick port. Those are as real as a
    // card's connectors and belong in the config for the same reason, which
    // is that what is plugged into them is the owner's choice and not ours.
    //
    // The built-in hardware BEHIND a port is not described here and does not
    // need to be: a //c's internal drive is soldered in, not attached, so it
    // is not a port and never appears in this list.
    vector<PortConfig>          ports;
    VideoConfig                 videoConfig;
    string                      keyboardType;

    // True when an enabled slot hosts the given device type -- e.g. a query for
    // "parallel-printer" tells the UI whether this machine can print. A slot the
    // user disabled in Settings > Hardware does not count.
    bool  HasEnabledSlotDevice (const string & device) const
    {
        for (const SlotConfig & entry : slots)
        {
            if (entry.enabled && entry.device == device)
            {
                return true;
            }
        }

        return false;
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfigLoader
//
//  Parses machine JSON into a MachineConfig, resolving ROM references through
//  a caller-supplied resolver.
//
//  The FileResolver seam is what makes this whole loader testable: a test
//  supplies a resolver backed by a table instead of the disk, so config
//  parsing and validation are covered without shipping ROM files into the test
//  tree. Production passes one that walks the real search paths.
//
//  Every entry point takes the machine name separately from the JSON, because
//  it selects the per-machine ROM subdirectory -- two machines may reference
//  files of the same name.
//
//  Errors come back as a message string alongside the HRESULT, since the
//  reader is hand-editing JSON and a result code alone cannot say which field
//  is wrong.
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

    // Maps the JSON string ("optional"|"required"|"platform-locked") to
    // the CapabilityFlag enum. If the input is empty the supplied
    // `defaultFlag` is returned. Unknown strings produce E_INVALIDARG.
    static HRESULT ParseCapabilityFlag (const string  & str,
                                        CapabilityFlag  defaultFlag,
                                        CapabilityFlag & outFlag,
                                        string         & outError);

    // Reads an optional `ports` array off `owner` -- a slot entry or the
    // machine root -- into `outPorts`. An entry may be a bare string naming
    // the attached device, or an object with `name` and/or `device`, so a
    // named connector and a numbered one use the same key. Anything else
    // becomes an empty port rather than an error: an unoccupied connector is
    // a legitimate state, so there is nothing here worth failing a load over.
    //
    // A MISSING `ports` KEY LEAVES `outPorts` EMPTY, and empty means "this
    // machine's default" -- never "no connectors". Every config written
    // before the key existed depends on that reading.
    static void    ParsePorts         (const JsonValue    & owner,
                                       vector<PortConfig> & outPorts);

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
