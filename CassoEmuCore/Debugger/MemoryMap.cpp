#include "Pch.h"

#include "Debugger/MemoryMap.h"
#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMap::Build
//
//  The //e's routing: ALTZP picks the bank for $0000-$01FF and the language
//  card's RAM; RAMRD and RAMWRT pick it for the rest of $0200-$BFFF, except
//  that 80STORE hands $0400-$07FF to PAGE2, and $2000-$3FFF too when HIRES is
//  on. INTCXROM puts the internal ROM over every slot's; SLOTC3ROM off puts
//  it over slot 3's alone, and INTC8ROM over $C800-$CFFF. With LCREAD off
//  $D000-$FFFF reads ROM, and with LCWRITE off writes there go nowhere.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<MemoryMapRange> MemoryMap::Build (const std::vector<SoftSwitch> & switches)
{
    std::vector<MemoryMapRange>  ranges;
    bool                         ramRd    = false;
    bool                         ramWrt   = false;
    bool                         altZp    = false;
    bool                         store80  = false;
    bool                         intCxRom = false;
    bool                         slotC3   = false;
    bool                         intC8Rom = false;
    bool                         page2    = false;
    bool                         hires    = false;
    bool                         lcRead   = false;
    bool                         lcWrite  = false;
    bool                         lcBank2  = false;
    bool                         hasMmu   = TryGetSwitch (switches, "RAMRD", ramRd);
    bool                         hasLc    = TryGetSwitch (switches, "LCREAD", lcRead);



    TryGetSwitch (switches, "RAMWRT",    ramWrt);
    TryGetSwitch (switches, "ALTZP",     altZp);
    TryGetSwitch (switches, "80STORE",   store80);
    TryGetSwitch (switches, "INTCXROM",  intCxRom);
    TryGetSwitch (switches, "SLOTC3ROM", slotC3);
    TryGetSwitch (switches, "INTC8ROM",  intC8Rom);
    TryGetSwitch (switches, "PAGE2",     page2);
    TryGetSwitch (switches, "HIRES",     hires);
    TryGetSwitch (switches, "LCWRITE",   lcWrite);
    TryGetSwitch (switches, "LCBANK2",   lcBank2);

    std::string  zeroPage = hasMmu && altZp  ? kAuxRam : kMainRam;
    std::string  reads    = hasMmu && ramRd  ? kAuxRam : kMainRam;
    std::string  writes   = hasMmu && ramWrt ? kAuxRam : kMainRam;
    std::string  page2Ram = page2 ? kAuxRam : kMainRam;
    bool         text80   = hasMmu && store80;
    bool         hires80  = text80 && hires;

    Add (ranges, 0x0000, 0x01FF, zeroPage, zeroPage);
    Add (ranges, 0x0200, 0x03FF, reads, writes);
    Add (ranges, 0x0400, 0x07FF, text80 ? page2Ram : reads, text80 ? page2Ram : writes);
    Add (ranges, 0x0800, 0x1FFF, reads, writes);
    Add (ranges, 0x2000, 0x3FFF, hires80 ? page2Ram : reads, hires80 ? page2Ram : writes);
    Add (ranges, 0x4000, 0xBFFF, reads, writes);
    Add (ranges, 0xC000, 0xC0FF, kIo, kIo);

    if (hasMmu)
    {
        std::string  slots = intCxRom ? kInternalRom : kSlotRom;
        std::string  slot3 = intCxRom || !slotC3 ? kInternalRom : kSlotRom;
        std::string  c8    = intCxRom || intC8Rom ? kInternalRom : kSlotRom;

        Add (ranges, 0xC100, 0xC2FF, slots, kNothing);
        Add (ranges, 0xC300, 0xC3FF, slot3, kNothing);
        Add (ranges, 0xC400, 0xC7FF, slots, kNothing);
        Add (ranges, 0xC800, 0xCFFF, c8,    kNothing);
    }
    else
    {
        Add (ranges, 0xC100, 0xCFFF, kSlotRom, kNothing);
    }

    if (hasLc)
    {
        std::string  bank   = std::string (hasMmu && altZp ? "aux" : "main") + " LC";
        std::string  banked = bank + (lcBank2 ? " bank 2" : " bank 1");
        std::string  upper  = bank + " RAM";

        Add (ranges, 0xD000, 0xDFFF, lcRead ? banked : kRom, lcWrite ? banked : kNothing);
        Add (ranges, 0xE000, 0xFFFF, lcRead ? upper  : kRom, lcWrite ? upper  : kNothing);
    }
    else
    {
        Add (ranges, 0xD000, 0xFFFF, kRom, kNothing);
    }

    return ranges;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMap::Format
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> MemoryMap::Format (const std::vector<MemoryMapRange> & ranges)
{
    std::vector<std::string>  lines;



    lines.push_back (std::format ("{:<11}  {:<16}  {}", "Range", "Read", "Write"));

    for (const MemoryMapRange & range : ranges)
    {
        lines.push_back (std::format ("${:04X}-${:04X}  {:<16}  {}", range.first, range.last, range.read, range.write));
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMap::TryGetSwitch
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryMap::TryGetSwitch (const std::vector<SoftSwitch> & switches, const char * name, bool & value)
{
    for (const SoftSwitch & entry : switches)
    {
        if (entry.name == name)
        {
            value = entry.value;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryMap::Add
//
//  A range with the same targets as the one before it extends that one.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryMap::Add (std::vector<MemoryMapRange> & ranges, Word first, Word last, const std::string & read, const std::string & write)
{
    if (!ranges.empty() && ranges.back().read == read && ranges.back().write == write && ranges.back().last + 1 == first)
    {
        ranges.back().last = last;
        return;
    }

    ranges.push_back ({ first, last, read, write });
}
