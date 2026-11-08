#pragma once

#include "Debugger/Reply.h"
#include "Ui/Debugger/DebuggerTextColors.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryRegionKeys
//
//  The labeled ranges a memory window and the heat map outline, one key for
//  each: ROM, I/O, the language card's banks, aux RAM, the expansion ROM,
//  each slot's ROM page and each slot's sixteen I/O addresses. Main RAM is
//  in none. A slot's ranges are labeled with the card in the slot.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryRegionKeys
{
public:
    static constexpr int       kSlotCount          = 8;     // slot 0, the language card's, to slot 7

    static constexpr uint16_t  kRegionRom          = 1;
    static constexpr uint16_t  kRegionIo           = 2;
    static constexpr uint16_t  kRegionLcBank1      = 3;
    static constexpr uint16_t  kRegionLcBank2      = 4;
    static constexpr uint16_t  kRegionAux          = 5;
    static constexpr uint16_t  kRegionExpansionRom = 6;
    static constexpr uint16_t  kRegionSlotRom      = 8;     // plus the slot, 1 to 7
    static constexpr uint16_t  kRegionSlotIo       = 16;    // plus the slot, 0 to 7

    //  The outline colors: ROM's and I/O's are their bytes' colors, and the
    //  language card's and aux RAM's the memory map's. Until a view has the
    //  theme's, the dark themes' defaults.
    struct Colors
    {
        uint32_t  rom     = 0xFF7FB2E5;
        uint32_t  io      = 0xFF909090;
        uint32_t  lcBank1 = 0xFF5BB36A;
        uint32_t  lcBank2 = 0xFF2E8B57;
        uint32_t  aux     = 0xFFD98A4A;

        bool operator== (const Colors & other) const = default;
    };

    //  The card in each slot by its display name, empty for an empty slot.
    using SlotCards = std::array<std::string, kSlotCount>;

    static uint16_t      GetKey       (MemoryRegion region, Word address);
    static std::wstring  GetLabel     (uint16_t key, const SlotCards & cards);
    static std::wstring  GetCardText  (const std::string & card);
    static uint32_t      GetColor     (uint16_t key, const Colors & colors);

    //  ROM's and I/O's from the bytes' colors, the rest from the memory map's.
    static Colors        GetColors    (const DebuggerTextColors::Set & set) { return Colors { set.rom, set.io, set.mapLcBank1, set.mapLcBank2, set.mapAux }; }
    static bool          IsIo         (uint16_t key) { return key == kRegionIo || (key >= kRegionSlotIo && key < kRegionSlotIo + kSlotCount); }
    static bool          IsRom        (uint16_t key) { return key == kRegionRom || key == kRegionExpansionRom || (key > kRegionSlotRom && key < kRegionSlotRom + kSlotCount); }
};
