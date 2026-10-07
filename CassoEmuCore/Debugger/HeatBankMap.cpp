#include "Pch.h"

#include "Debugger/HeatBankMap.h"
#include "Core/MemoryBus.h"
#include "Machines/Apple2/Common/LanguageCard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatBankMap::Clear
//
////////////////////////////////////////////////////////////////////////////////

void HeatBankMap::Clear()
{
    m_bus  = nullptr;
    m_card = nullptr;
    m_last = 0;

    m_regions.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatBankMap::AddRegion
//
//  A buffer already added, as the same buffer can be reached two ways, is
//  not added again.
//
////////////////////////////////////////////////////////////////////////////////

void HeatBankMap::AddRegion (
    const Byte  * base,
    size_t        size,
    HeatSpace     space,
    Word          firstIndex)
{
    bool  isKnown = std::ranges::any_of (m_regions, [base] (const Region & region) { return region.base == base; });



    if (base == nullptr || size == 0 || isKnown)
    {
        return;
    }

    m_regions.push_back (Region { base, size, space, firstIndex });
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatBankMap::HasSpace
//
////////////////////////////////////////////////////////////////////////////////

bool HeatBankMap::HasSpace (HeatSpace space) const
{
    if (space == HeatSpace::Cpu || space == HeatSpace::Rom)
    {
        return m_bus != nullptr;
    }

    return std::ranges::any_of (m_regions, [space] (const Region & region) { return region.space == space; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatBankMap::TryResolve
//
//  The byte the page tables point the access at, or, for a write the
//  language card takes, the byte the card stores it in; then the buffer that
//  byte is in. A byte in none of them, or no byte at all, from $C100 up is
//  ROM.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatBankMap::TryResolve (
    Word            address,
    bool            isWrite,
    HeatLocation  & outLocation) const
{
    const Byte  * page = nullptr;
    const Byte  * cell = nullptr;



    if (m_bus == nullptr || (address >= kIoFirst && address < kRomFirst))
    {
        return false;
    }

    page = isWrite ? m_bus->GetShadowWritePage (address) : m_bus->GetShadowReadPage (address);

    if (page != nullptr)
    {
        cell = page + (address & 0xFF);
    }
    else if (isWrite && m_card != nullptr && address >= kLcFirst)
    {
        cell = m_card->GetWriteTarget (address);
    }

    if (cell != nullptr && TryFindRegion (cell, outLocation))
    {
        return true;
    }

    if (address < kRomFirst)
    {
        return false;
    }

    outLocation = HeatLocation { HeatSpace::Rom, address };
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatBankMap::TryFindRegion
//
//  The region the last lookup found is tried first: an instruction's bytes
//  and most of its data share one.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatBankMap::TryFindRegion (
    const Byte    * cell,
    HeatLocation  & outLocation) const
{
    size_t          count  = m_regions.size();
    size_t          index  = 0;
    const Region  * region = nullptr;



    for (size_t tried = 0; tried < count; tried++)
    {
        index  = (m_last + tried) % count;
        region = &m_regions[index];

        if (cell >= region->base && cell < region->base + region->size)
        {
            outLocation = HeatLocation { region->space, (Word) (region->firstIndex + (size_t) (cell - region->base)) };
            m_last      = index;
            return true;
        }
    }

    return false;
}





