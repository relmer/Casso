#pragma once

struct SoftSwitch;





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMapRange
//
//  One address range and where its reads and writes go.
//
////////////////////////////////////////////////////////////////////////////////

struct MemoryMapRange
{
    Word         first = 0;
    Word         last  = 0;
    std::string  read;
    std::string  write;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMap
//
//  The resolved memory map MAP shows, worked out from the soft switches the
//  target reports: the MMU's (RAMRD, RAMWRT, ALTZP, 80STORE, INTCXROM,
//  SLOTC3ROM, INTC8ROM), the language card's (LCREAD, LCWRITE, LCBANK2) and
//  the video switches 80STORE consults (PAGE2, HIRES). A machine without an
//  MMU reports none of the MMU's switches, and one without a language card
//  none of its own, so each part of the map appears only where the machine
//  has the hardware. Adjacent ranges with the same targets are merged.
//
//  With the slot whose expansion ROM is selected known, 0 for none,
//  $C800-$CEFF is that slot's ROM and $CF00-$CFFF reads nothing.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryMap
{
public:
    static std::vector<MemoryMapRange>  Build  (const std::vector<SoftSwitch> & switches, std::optional<int> expansionSlot = std::nullopt);
    static std::vector<std::string>     Format (const std::vector<MemoryMapRange> & ranges);

    static constexpr const char *  kMainRam     = "main RAM";
    static constexpr const char *  kAuxRam      = "aux RAM";
    static constexpr const char *  kRom         = "ROM";
    static constexpr const char *  kInternalRom = "internal ROM";
    static constexpr const char *  kSlotRom     = "slot ROM";
    static constexpr const char *  kIo          = "I/O";
    static constexpr const char *  kNothing     = "nothing";

private:
    static bool  TryGetSwitch (const std::vector<SoftSwitch> & switches, const char * name, bool & value);
    static void  Add          (std::vector<MemoryMapRange> & ranges, Word first, Word last, const std::string & read, const std::string & write);
    static void  AddExpansion (std::vector<MemoryMapRange> & ranges, std::optional<int> expansionSlot, const std::string & read);
};
