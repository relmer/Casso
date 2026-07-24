#include "Pch.h"

#include "MemoryBus.h"
#include "Prng.h"




////////////////////////////////////////////////////////////////////////////////
//
//  Constants
//
////////////////////////////////////////////////////////////////////////////////

// Apple II display pages are addressed in 128-byte blocks whose last 8 bytes
// ($78-$7F within the block) are "screen holes" -- undisplayed scratch RAM the
// slot firmware and DOS hammer in poll loops. The pattern is identical across
// text, lo-res, and hi-res pages (same low-7-bit video addressing), so a write
// is displayed iff its block offset is below $78. Screen-hole writes must not
// dirty the frame or an idle DOS prompt re-rasterizes needlessly.
static constexpr Word  s_kScreenBlockMask     = 0x7F;
static constexpr Word  s_kFirstScreenHoleByte = 0x78;





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBus
//
////////////////////////////////////////////////////////////////////////////////

MemoryBus::MemoryBus()
{
    InvalidateDispatchCache ();
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
            Byte * cell = &page[address & 0xFF];

            // Video-dirty raise: only a write that actually CHANGES a
            // *displayed* byte in a watched page marks the frame for re-render.
            // The watched check short-circuits the common non-video write; the
            // screen-hole check drops undisplayed scratch writes; and the
            // value compare drops same-value re-stores -- so an idle screen
            // whose firmware polls through the screen holes stops re-rendering.
            if (m_videoWatched[address >> 8]                       &&
                (address & s_kScreenBlockMask) < s_kFirstScreenHoleByte &&
                *cell != value)
            {
                m_videoDirty = true;
            }

            *cell = value;
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

    // Check for overlaps with existing devices. Overlap is documented as
    // a "first match wins" contract in MemoryBus dispatch, and several
    // unit tests register overlapping ranges intentionally to verify
    // that contract. Logging the overlap here would just produce noise
    // during those tests; real-product misregistrations surface via
    // wrong-dispatch test failures, not via this warning.

    // Insert sorted by start address
    auto it = lower_bound (m_entries.begin(),
                           m_entries.end(),
                           entry,
                           [] (const BusEntry & a, const BusEntry & b)
                           {
                               return a.start < b.start;
                           });

    m_entries.insert (it, entry);

    InvalidateDispatchCache ();
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

    InvalidateDispatchCache ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Validate
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MemoryBus::Validate() const
{
    // Overlap is allowed by contract -- "first match wins" -- so this
    // method intentionally does not flag overlaps. Kept as a hook for
    // future invariants that don't conflict with the dispatch contract.
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
    m_videoDirty       = true;
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
    const int slot = address & (kDispatchSlots - 1);

    if (m_dispatchTag[slot] == address)
    {
        return m_dispatchDev[slot];
    }

    MemoryDevice * device = nullptr;

    for (const auto & entry : m_entries)
    {
        if (address >= entry.start && address <= entry.end)
        {
            device = entry.device;
            break;
        }
    }

    m_dispatchTag[slot] = address;
    m_dispatchDev[slot] = device;

    return device;
}




////////////////////////////////////////////////////////////////////////////////
//
//  InvalidateDispatchCache
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::InvalidateDispatchCache ()
{
    for (int i = 0; i < kDispatchSlots; i++)
    {
        m_dispatchTag[i] = kDispatchInvalid;
    }
}
