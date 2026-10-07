#pragma once

#include "Pch.h"

#include "Debugger/HeatTypes.h"

class LanguageCard;
class MemoryBus;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatBankMap
//
//  Resolves an access the CPU made to where it landed. The bus's page tables
//  say which buffer a read or a write of an address reaches, and each RAM
//  buffer the machine has is added here with the space and index its first
//  byte stands at. A write into the language card's window, which the card
//  takes itself, lands where the card says. A byte in no buffer added is
//  ROM from $C100 up, and nothing below that: I/O is the CPU's alone.
//
////////////////////////////////////////////////////////////////////////////////

class HeatBankMap
{
public:
    static constexpr Word  kIoFirst  = 0xC000;
    static constexpr Word  kRomFirst = 0xC100;
    static constexpr Word  kLcFirst  = 0xD000;

    void   Clear            ();
    void   SetBus           (const MemoryBus * bus)        { m_bus = bus; }
    void   SetLanguageCard  (const LanguageCard * card)    { m_card = card; }
    void   AddRegion        (const Byte * base, size_t size, HeatSpace space, Word firstIndex);

    //  Whether the machine has memory in the space: a buffer added for it,
    //  or, for Rom, a bus.
    bool   HasSpace         (HeatSpace space) const;
    bool   HasLanguageCard  () const { return m_card != nullptr; }

    //  Where a read or a write of address lands now; false for I/O, and for
    //  everything without a bus.
    bool   TryResolve       (Word address, bool isWrite, HeatLocation & outLocation) const;

    //  The byte a read or a write of address reaches now, which a write is
    //  about to replace; null for I/O and for a write no memory takes.
    const Byte * GetCell    (Word address, bool isWrite) const;

private:
    struct Region
    {
        const Byte  * base       = nullptr;
        size_t        size       = 0;
        HeatSpace     space      = HeatSpace::Cpu;
        Word          firstIndex = 0;
    };

    bool   TryFindRegion    (const Byte * cell, HeatLocation & outLocation) const;

    const MemoryBus      * m_bus  = nullptr;
    const LanguageCard   * m_card = nullptr;
    std::vector<Region>    m_regions;
    mutable size_t         m_last = 0;                // the region the last lookup found
};
