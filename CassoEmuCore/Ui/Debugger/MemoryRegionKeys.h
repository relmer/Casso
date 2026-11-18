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
    static constexpr uint16_t  kRegionRomPart      = 32;    // plus the part's index in kRomParts

    //  The parts of the system ROM GetRomArea finds, which a memory window
    //  outlines apart.
    static constexpr std::array<const wchar_t *, 11>  kRomParts =
    {
        L"Monitor", L"Integer BASIC", L"Applesoft BASIC", L"80-column firmware", L"internal firmware",
        L"serial port 1 firmware", L"serial port 2 firmware", L"memory expansion firmware",
        L"SmartPort firmware", L"disk port firmware", L"mouse firmware",
    };

    //  The //c's built-in ports at their slot numbers in its firmware:
    //  each slot's I/O and its ROM page's part. Empty on any other machine.
    static std::wstring  GetBuiltInIo  (const std::string & machineId, int slot);
    static std::wstring  GetBuiltInRom (const std::string & machineId, int slot);

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

    //  What part of the system ROM an address is in, by the machine's model
    //  directory ("Apple2eEnhanced"):
    //  "Monitor", "Applesoft BASIC", "Integer BASIC", "80-column firmware"
    //  or "internal firmware"; empty where no part is known.
    static std::wstring  GetRomArea   (const std::string & machineId, Word address);

    //  A range's label with the part of the system ROM at an address in it,
    //  for a cell's tip: "ROM: Applesoft BASIC", "Slot 6: Disk ][".
    static std::wstring  GetRomLabel  (uint16_t key, Word address, const SlotCards & cards, const std::string & machineId);

    //  The same for a range of `count` addresses from `first`, which can
    //  span several parts of the system ROM: "ROM: Applesoft BASIC, Monitor".
    static std::wstring  GetRangeLabel (uint16_t key, Word first, int count, const SlotCards & cards, const std::string & machineId);

    //  ROM's key with the part of the system ROM at an address, kRegionRom
    //  where no part is known.
    static uint16_t      GetRomPartKey (const std::string & machineId, Word address);

    static uint16_t      GetKey       (MemoryRegion region, Word address);
    static std::wstring  GetLabel     (uint16_t key, const SlotCards & cards, const std::string & machineId = {});
    static std::wstring  GetCardText  (const std::string & card);
    static uint32_t      GetColor     (uint16_t key, const Colors & colors);

    //  ROM's and I/O's from the bytes' colors, the rest from the memory map's.
    static Colors        GetColors    (const DebuggerTextColors::Set & set) { return Colors { set.rom, set.io, set.mapLcBank1, set.mapLcBank2, set.mapAux }; }
    static bool          IsIo         (uint16_t key) { return key == kRegionIo || (key >= kRegionSlotIo && key < kRegionSlotIo + kSlotCount); }
    static bool          IsRomPart    (uint16_t key) { return key >= kRegionRomPart && key < kRegionRomPart + kRomParts.size(); }
    static bool          IsRom        (uint16_t key) { return key == kRegionRom || key == kRegionExpansionRom || IsRomPart (key) || (key > kRegionSlotRom && key < kRegionSlotRom + kSlotCount); }
};
