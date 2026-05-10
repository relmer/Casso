#include "Pch.h"

#include "MemoryBus.h"
#include "Prng.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBus
//
////////////////////////////////////////////////////////////////////////////////

MemoryBus::MemoryBus()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadByte
//
////////////////////////////////////////////////////////////////////////////////

Byte MemoryBus::ReadByte (Word address)
{
    // Fast path: page table lookup for $0000-$BFFF
    if (address < 0xC000)
    {
        Byte * page = m_readPage[address >> 8];

        if (page != nullptr)
        {
            return page[address & 0xFF];
        }
    }

    MemoryDevice * device = FindDevice (address);

    if (device != nullptr)
    {
        Byte value = device->Read (address);
        m_floatingBusValue = value;
        return value;
    }

    // Unmapped I/O in $C000-$CFFF returns floating bus value
    if (address >= 0xC000 && address <= 0xCFFF)
    {
        return m_floatingBusValue;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteByte
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::WriteByte (Word address, Byte value)
{
    // Fast path: page table lookup for $0000-$BFFF
    if (address < 0xC000)
    {
        Byte * page = m_writePage[address >> 8];

        if (page != nullptr)
        {
            page[address & 0xFF] = value;
            return;
        }

        // No page mapping -- fall through to device-based write (e.g., for ROM areas)
    }

    MemoryDevice * device = FindDevice (address);

    if (device != nullptr)
    {
        device->Write (address, value);
    }

    m_floatingBusValue = value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetReadPage / SetWritePage
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::SetReadPage (int pageIndex, Byte * page)
{
    if (pageIndex >= 0 && pageIndex < 0x100)
    {
        m_readPage[pageIndex] = page;
    }
}

void MemoryBus::SetWritePage (int pageIndex, Byte * page)
{
    if (pageIndex >= 0 && pageIndex < 0x100)
    {
        m_writePage[pageIndex] = page;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddDevice
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::AddDevice (MemoryDevice * device)
{
    BusEntry entry;
    Word     newStart = device->GetStart();
    Word     newEnd   = device->GetEnd();



    entry.start  = newStart;
    entry.end    = newEnd;
    entry.device = device;

    // Check for overlaps with existing devices
    for (const auto & existing : m_entries)
    {
        if (newStart <= existing.end && existing.start <= newEnd)
        {
            DEBUGMSG (L"Bus conflict: new device $%04X-$%04X overlaps existing $%04X-$%04X\n",
                      newStart, newEnd, existing.start, existing.end);
        }
    }

    // Insert sorted by start address
    auto it = lower_bound (m_entries.begin(),
                           m_entries.end(),
                           entry,
                           [] (const BusEntry & a, const BusEntry & b)
                           {
                               return a.start < b.start;
                           });

    m_entries.insert (it, entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RemoveDevice
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::RemoveDevice (MemoryDevice * device)
{
    auto it = remove_if (
        m_entries.begin(),
        m_entries.end(),
        [device] (const BusEntry & entry)
        {
            return entry.device == device;
        });

    m_entries.erase (it, m_entries.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  Validate
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MemoryBus::Validate() const
{
    for (size_t i = 0; i + 1 < m_entries.size(); i++)
    {
        for (size_t j = i + 1; j < m_entries.size(); j++)
        {
            const BusEntry & a = m_entries[i];
            const BusEntry & b = m_entries[j];

            if (a.start <= b.end && b.start <= a.end)
            {
                DEBUGMSG (L"Bus overlap (first match wins): $%04X-$%04X and $%04X-$%04X\n",
                          a.start, a.end, b.start, b.end);
            }
        }
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::Reset()
{
    for (auto & entry : m_entries)
    {
        entry.device->Reset();
    }

    m_floatingBusValue = 0xFF;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftResetAll
//
//  Phase 4 split-reset (FR-034). Fans out SoftReset to every attached
//  device. RAM-owning devices are no-ops here so user RAM survives soft
//  reset on the //e (audit §10 [CRITICAL]).
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::SoftResetAll ()
{
    for (auto & entry : m_entries)
    {
        entry.device->SoftReset ();
    }

    m_floatingBusValue = 0xFF;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycleAll
//
//  Phase 4 split-reset (FR-035). Fans out PowerCycle so every DRAM-owning
//  device re-seeds from the shared Prng. Real //e DRAM is undefined at
//  power-on; the deterministic Prng stand-in is common emulator practice
//  for repeatable test runs (audit §10).
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::PowerCycleAll (Prng & prng)
{
    for (auto & entry : m_entries)
    {
        entry.device->PowerCycle (prng);
    }

    m_floatingBusValue = 0xFF;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindDevice
//
////////////////////////////////////////////////////////////////////////////////

MemoryDevice * MemoryBus::FindDevice (Word address) const
{
    for (const auto & entry : m_entries)
    {
        if (address >= entry.start && address <= entry.end)
        {
            return entry.device;
        }
    }

    return nullptr;
}
