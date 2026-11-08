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
//  A slot's ranges by the card in it, "Super Serial Card in slot 2"; an
//  empty slot's by its number. Slot 0's I/O is the language card's.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MemoryRegionKeys::GetLabel (uint16_t key, const SlotCards & cards)
{
    int  slot = 0;



    if (key > kRegionSlotRom && key < kRegionSlotRom + kSlotCount)
    {
        slot = key - kRegionSlotRom;

        return cards[(size_t) slot].empty() ? std::format (L"Slot {} ROM", slot)
                                            : std::format (L"{} in slot {}", TextEncoding::NarrowToWide (cards[(size_t) slot]), slot);
    }

    if (key >= kRegionSlotIo && key < kRegionSlotIo + kSlotCount)
    {
        slot = key - kRegionSlotIo;

        if (slot == 0)
        {
            return L"Language card";
        }

        return cards[(size_t) slot].empty() ? std::format (L"Slot {} I/O", slot)
                                            : std::format (L"{} in slot {}", TextEncoding::NarrowToWide (cards[(size_t) slot]), slot);
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
