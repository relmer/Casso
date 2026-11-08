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
//  A slot's ranges by the card in it, "Slot 2: Super Serial Card", and an
//  empty slot's ROM page or I/O by the slot alone. Slot 0's I/O is the
//  language card's.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryRegionKeys::GetLabel (uint16_t key, const SlotCards & cards)
{
    int  slot = 0;



    if (key > kRegionSlotRom && key < kRegionSlotRom + kSlotCount)
    {
        slot = key - kRegionSlotRom;

        return cards[(size_t) slot].empty() ? std::format (L"Slot {} ROM", slot)
                                            : std::format (L"Slot {}: {}", slot, GetCardText (cards[(size_t) slot]));
    }

    if (key >= kRegionSlotIo && key < kRegionSlotIo + kSlotCount)
    {
        slot = key - kRegionSlotIo;

        if (slot == 0)
        {
            return L"Slot 0: language card";
        }

        return cards[(size_t) slot].empty() ? std::format (L"Slot {} I/O", slot)
                                            : std::format (L"Slot {}: {}", slot, GetCardText (cards[(size_t) slot]));
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
//  expansion space; the rest of it, and all of the //c's, is the machine's
//  internal firmware.
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

    return L"internal firmware";
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryRegionKeys::GetRomLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryRegionKeys::GetRomLabel (uint16_t key, Word address, const SlotCards & cards, const RomFiles & roms)
{
    std::wstring  label = GetLabel (key, cards);
    std::wstring  area;
    std::string   file;



    if (key == kRegionRom)
    {
        area  = GetRomArea (roms.machineId, address);
        label = area.empty() ? L"ROM" : L"ROM: " + area;
        file  = roms.system;
    }
    else if (key > kRegionSlotRom && key < kRegionSlotRom + kSlotCount)
    {
        file = roms.slots[(size_t) (key - kRegionSlotRom)];
    }

    if (!file.empty())
    {
        label += L" (" + TextEncoding::NarrowToWide (file) + L")";
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
