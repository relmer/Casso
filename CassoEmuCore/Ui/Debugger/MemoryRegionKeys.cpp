#include "Pch.h"

#include "Ui/Debugger/MemoryRegionKeys.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryRegionKeys::GetKey
//
//  A slot's ROM in $C100-$C7FF is that slot's, by its page; slot ROM above
//  $C7FF is the card's expansion ROM. $C080-$C0FF is the slots' I/O, sixteen
//  addresses for each, slot 0's the language card's.
//
////////////////////////////////////////////////////////////////////////////////

uint16_t MemoryRegionKeys::GetKey (MemoryRegion region, Word address)
{
    constexpr Word  kFirstSlotPage = 0xC1;
    constexpr Word  kLastSlotPage  = 0xC7;
    constexpr Word  kSlotMask      = 0x07;
    constexpr Word  kSlotIoFirst   = 0xC080;
    constexpr Word  kSlotIoLast    = 0xC0FF;
    constexpr int   kSlotIoShift   = 4;
    Word            page           = (Word) (address >> 8);



    switch (region)
    {
    case MemoryRegion::Rom:     return kRegionRom;
    case MemoryRegion::LcBank1: return kRegionLcBank1;
    case MemoryRegion::LcBank2: return kRegionLcBank2;
    case MemoryRegion::AuxRam:  return kRegionAux;
    case MemoryRegion::MainRam: return 0;
    case MemoryRegion::Io:
        return (address >= kSlotIoFirst && address <= kSlotIoLast) ? (uint16_t) (kRegionSlotIo + ((address - kSlotIoFirst) >> kSlotIoShift)) : kRegionIo;
    case MemoryRegion::SlotRom:
        return (page >= kFirstSlotPage && page <= kLastSlotPage) ? (uint16_t) (kRegionSlotRom + (page & kSlotMask)) : kRegionExpansionRom;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryRegionKeys::GetLabel
//
//  A slot's ranges by the card in it, "Slot 2: Super Serial Card", or
//  "Slot 3: empty" for a slot with no card. Slot 0's I/O is the language
//  card's switches, which bank its RAM in at $D000-$FFFF. The //c's ranges
//  are its built-in ports'.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryRegionKeys::GetLabel (uint16_t key, const SlotCards & cards, const std::string & machineId)
{
    int  slot = 0;



    if (key > kRegionSlotRom && key < kRegionSlotRom + kSlotCount)
    {
        slot = key - kRegionSlotRom;

        if (!GetBuiltInRom (machineId, slot).empty())
        {
            return L"ROM: " + GetBuiltInRom (machineId, slot);
        }

        return cards[(size_t) slot].empty() ? std::format (L"Slot {}: empty", slot)
                                            : std::format (L"Slot {}: {}", slot, GetCardText (cards[(size_t) slot]));
    }

    if (key >= kRegionSlotIo && key < kRegionSlotIo + kSlotCount)
    {
        slot = key - kRegionSlotIo;

        if (slot == 0)
        {
            return L"Slot 0: language card switches";
        }

        if (!GetBuiltInIo (machineId, slot).empty())
        {
            return GetBuiltInIo (machineId, slot);
        }

        return cards[(size_t) slot].empty() ? std::format (L"Slot {}: empty", slot)
                                            : std::format (L"Slot {}: {}", slot, GetCardText (cards[(size_t) slot]));
    }

    if (IsRomPart (key))
    {
        return std::wstring (L"ROM: ") + kRomParts[(size_t) (key - kRegionRomPart)];
    }

    switch (key)
    {
    case kRegionRom:          return L"ROM";
    case kRegionIo:           return L"I/O";
    case kRegionLcBank1:      return L"LC bank 1";
    case kRegionLcBank2:      return L"LC bank 2";
    case kRegionAux:          return L"Aux RAM";
    case kRegionExpansionRom: return L"Expansion ROM";
    default:                  break;
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryRegionKeys::GetRomArea
//
//  The Monitor is $F800-$FFFF on every model, under Integer BASIC from
//  $E000 on the original Apple ][ and Applesoft from $D000 on the rest. The
//  //e's internal $C100-$CFFF has the 80-column firmware at $C300 and in the
//  expansion space, and the //c's has its built-in ports' firmware in the
//  slot pages; the rest of either is the machine's internal firmware.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryRegionKeys::GetRomArea (const std::string & machineId, Word address)
{
    constexpr Word  kMonitor      = 0xF800;
    constexpr Word  kIntegerBasic = 0xE000;
    constexpr Word  kApplesoft    = 0xD000;
    constexpr Word  kFirmware     = 0xC100;
    constexpr Word  kSlot3Page    = 0xC3;
    constexpr Word  kExpansion    = 0xC800;
    bool            isOriginal    = machineId == "Apple2";
    bool            isIie         = machineId.starts_with ("Apple2e");



    if (address >= kMonitor)
    {
        return L"Monitor";
    }

    if (isOriginal)
    {
        return (address >= kIntegerBasic) ? L"Integer BASIC" : std::wstring();
    }

    if (address >= kApplesoft)
    {
        return L"Applesoft BASIC";
    }

    if (address < kFirmware)
    {
        return {};
    }

    if (isIie && ((address >> 8) == kSlot3Page || address >= kExpansion))
    {
        return L"80-column firmware";
    }

    if (address < kExpansion && !GetBuiltInRom (machineId, (address >> 8) & 0x07).empty())
    {
        return GetBuiltInRom (machineId, (address >> 8) & 0x07);
    }

    return L"internal firmware";
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryRegionKeys::GetBuiltInIo
//
//  In the //c's firmware (ROM 3 and 4), serial port 1 is slot 1, serial
//  port 2 slot 2, the memory expansion slot 4 and the disk port slot 6;
//  slots 3, 5 and 7 have no I/O.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryRegionKeys::GetBuiltInIo (const std::string & machineId, int slot)
{
    static constexpr std::array<const wchar_t *, kSlotCount>  kIo =
    {
        L"", L"Serial port 1 (printer)", L"Serial port 2 (modem)", L"No device",
        L"Memory expansion", L"No device", L"Disk port (IWM)", L"No device",
    };



    if (!machineId.starts_with ("Apple2c") || slot < 0 || slot >= kSlotCount)
    {
        return {};
    }

    return kIo[(size_t) slot];
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryRegionKeys::GetBuiltInRom
//
//  The //c's slot pages: each port's firmware, the 80-column firmware at
//  $C300, the SmartPort's for 3.5" drives at $C500, and the mouse's at
//  $C700.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryRegionKeys::GetBuiltInRom (const std::string & machineId, int slot)
{
    static constexpr std::array<const wchar_t *, kSlotCount>  kRom =
    {
        L"", L"serial port 1 firmware", L"serial port 2 firmware", L"80-column firmware",
        L"memory expansion firmware", L"SmartPort firmware", L"disk port firmware", L"mouse firmware",
    };



    if (!machineId.starts_with ("Apple2c") || slot < 1 || slot >= kSlotCount)
    {
        return {};
    }

    return kRom[(size_t) slot];
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryRegionKeys::GetRomPartKey
//
////////////////////////////////////////////////////////////////////////////////

uint16_t MemoryRegionKeys::GetRomPartKey (const std::string & machineId, Word address)
{
    std::wstring  area = GetRomArea (machineId, address);



    for (size_t i = 0; i < kRomParts.size(); i++)
    {
        if (area == kRomParts[i])
        {
            return (uint16_t) (kRegionRomPart + i);
        }
    }

    return kRegionRom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryRegionKeys::GetRomLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryRegionKeys::GetRomLabel (uint16_t key, Word address, const SlotCards & cards, const std::string & machineId)
{
    return GetLabel ((key == kRegionRom) ? GetRomPartKey (machineId, address) : key, cards, machineId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryRegionKeys::GetRangeLabel
//
//  The system ROM's parts in the range, each once, in order; they change
//  only at page boundaries, so a page apart is close enough.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryRegionKeys::GetRangeLabel (uint16_t key, Word first, int count, const SlotCards & cards, const std::string & machineId)
{
    constexpr uint32_t         kPage = 0x100;
    std::vector<std::wstring>  areas;
    std::wstring               label = L"ROM";
    uint32_t                   end   = (uint32_t) first + (uint32_t) std::max (1, count);



    if (key != kRegionRom)
    {
        return GetRomLabel (key, first, cards, machineId);
    }

    for (uint32_t address = first; address < end; address += kPage)
    {
        std::wstring  area = GetRomArea (machineId, (Word) address);



        if (!area.empty() && std::ranges::find (areas, area) == areas.end())
        {
            areas.push_back (area);
        }
    }

    for (size_t i = 0; i < areas.size(); i++)
    {
        label += ((i == 0) ? L": " : L", ") + areas[i];
    }

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryRegionKeys::GetCardText
//
//  A card's display name after a label's colon, in sentence case: a common
//  noun is lowercased ("Parallel printer" is "parallel printer"), and a
//  name whose second word is capitalized ("Super Serial Card", "Mockingboard
//  C") is left as it is.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryRegionKeys::GetCardText (const std::string & card)
{
    std::wstring  text  = TextEncoding::NarrowToWide (card);
    size_t        space = text.find (L' ');



    if (space != std::wstring::npos && space + 1 < text.size() && iswlower (text[space + 1]) && iswupper (text[0]))
    {
        text[0] = (wchar_t) towlower (text[0]);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryRegionKeys::GetColor
//
//  A slot's ROM and the expansion ROM in ROM's color and a slot's I/O in
//  I/O's, as their bytes are drawn.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t MemoryRegionKeys::GetColor (uint16_t key, const Colors & colors)
{
    if (IsIo (key))
    {
        return colors.io;
    }

    switch (key)
    {
    case kRegionLcBank1: return colors.lcBank1;
    case kRegionLcBank2: return colors.lcBank2;
    case kRegionAux:     return colors.aux;
    case 0:              return 0;
    default:             break;
    }

    return colors.rom;
}
