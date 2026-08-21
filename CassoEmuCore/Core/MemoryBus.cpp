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
    // No devices yet: an all-null map correctly resolves every I/O address to
    // "unmapped" until AddDevice rebuilds it.
    m_ioDeviceMap.assign (kIoMapSize, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadByte
//
//  The hottest read in the emulator: a page-table lookup, with device dispatch
//  as the fallback.
//
//  The page table maps each of the 256 pages to a raw pointer, or null. RAM
//  ($0000-$BFFF) and -- once the language card wires them -- ROM and card RAM
//  ($D000-$FFFF) get a mapped page, so a normal read is one branch and one
//  indexed load. I/O ($C000-$CFFF) is deliberately left NULL so it falls
//  through to device dispatch and its read side effects actually run; a soft
//  switch that is merely read from a table does nothing.
//
//  That is also why the page table is the right shape for MMU banking. Aux
//  memory, language-card banks, and 80STORE all re-point PAGES rather than
//  re-registering devices, so the fast path never has to know they exist.
//
//  Unmapped I/O returns the FLOATING BUS -- the last value any device drove --
//  because that is what real hardware does: nothing pulls the lines, so they
//  hold their last state. Software genuinely depends on it (the classic
//  video-sync detection reads undriven $C0xx). Outside that window an unmapped
//  address reads as zero instead, since there is no bus to float.
//
////////////////////////////////////////////////////////////////////////////////

Byte MemoryBus::ReadByte (Word address)
{
    // Fast path: page-table lookup. RAM ($0000-$BFFF) and, once the language
    // card wires them, ROM/LC RAM ($D000-$FFFF) have a mapped page; I/O
    // ($C000-$CFFF) stays null and falls through to device dispatch so its
    // read side effects run. This is the hottest read in the emulator, so the
    // mapped case stays one branch deep.
    Byte *          page   = m_readPage[address >> 8];
    MemoryDevice *  device = nullptr;
    Byte            value  = 0;



    if (page != nullptr)
    {
        value = page[address & 0xFF];
    }
    else
    {
        device = FindDevice (address);

        if (device != nullptr)
        {
            value              = device->Read (address);
            m_floatingBusValue = value;
        }
        else if (address >= 0xC000 && address <= 0xCFFF)
        {
            // Unmapped I/O: the bus holds whatever the last device drove.
            // Outside that window an unmapped address reads as 0 instead --
            // there is no bus to float.
            value = m_floatingBusValue;
        }
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteByte
//
//  The write counterpart: page-table fast path below $C000, device dispatch
//  above it or wherever no page is mapped.
//
//  The write table is separate from the read table because the two genuinely
//  differ -- language-card RAM can be read-only while its underlying RAM is
//  still writable, and ROM maps for reads with no write page at all.
//
//  Riding along on the fast path is the video-dirty flag that drives the
//  render-skip gate, and its three conditions are ordered cheapest-first and
//  each drops a distinct class of write:
//
//    watched page  short-circuits the overwhelmingly common non-video write
//    screen hole   drops writes to the undisplayed bytes inside the text and
//                  hi-res pages, which firmware freely uses as scratch
//    value compare drops a re-store of the same byte
//
//  Together they mean an idle screen whose firmware is polling through the
//  screen holes stops re-rendering entirely -- which is where the render-skip
//  gate's savings actually come from.
//
//  The floating-bus value is updated on every write, including ones that
//  reached no device, because the value was driven onto the bus regardless of
//  whether anything latched it.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::WriteByte (Word address, Byte value)
{
    MemoryDevice * device = nullptr;



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

    device = FindDevice (address);

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
//  Registers a device's address range, keeping the entry list sorted by start
//  address.
//
//  The bus does NOT own the device -- it stores a bare pointer, and the caller
//  (MachineManager's owned-device list) holds the lifetime. This is a routing
//  table, not a container.
//
//  Overlapping ranges are ALLOWED and are not diagnosed here. Dispatch is
//  documented as first-match-wins, several unit tests register overlaps
//  deliberately to verify exactly that, and a warning would be pure noise
//  during those runs. A real misregistration in the product surfaces as a
//  wrong-dispatch test failure, which names the actual problem.
//
//  Sorted insertion keeps the linear fallback scan in FindDevice ordered, so
//  first-match-wins means lowest-start-wins rather than
//  whoever-registered-first.
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

    BuildIoDeviceMap();
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

    BuildIoDeviceMap();
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

void MemoryBus::SoftResetAll()
{
    for (auto & entry : m_entries)
    {
        entry.device->SoftReset();
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
//  Resolves an address to its device, by two different mechanisms.
//
//  At or above $C000 -- the I/O and ROM window, where every fast-path miss
//  lands -- lookup is a DIRECT INDEX into a precomputed map. That range is
//  where soft switches live and is read constantly, so it cannot afford a
//  scan; BuildIoDeviceMap paints each device's footprint into that map
//  whenever the device set changes.
//
//  Below $C000 the linear scan is genuinely cold. Production maps all of
//  $0000-$BFFF through the page table, so this path is only reached on the
//  partial buses that unit tests construct, where a scan over a handful of
//  entries is entirely adequate.
//
//  The scan keeps the FIRST match rather than the last, honoring the
//  first-match-wins contract that AddDevice's sorted insertion establishes.
//
////////////////////////////////////////////////////////////////////////////////

MemoryDevice * MemoryBus::FindDevice (Word address) const
{
    MemoryDevice *  device = nullptr;



    if (address >= kIoMapBase)
    {
        // I/O is a direct-indexed map; the scan below is never reached for it.
        device = m_ioDeviceMap[address - kIoMapBase];
    }
    else
    {
        // Rare: a read/write to an unmapped low page. Production maps all of
        // $0000-$BFFF through the page table, so this only happens on partial
        // test buses; a linear scan of the handful of entries is fine here.
        for (const auto & entry : m_entries)
        {
            if (device == nullptr && address >= entry.start && address <= entry.end)
            {
                device = entry.device;
            }
        }
    }

    return device;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildIoDeviceMap
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::BuildIoDeviceMap()
{
    fill (m_ioDeviceMap.begin(), m_ioDeviceMap.end(), nullptr);

    // Paint each device's footprint in $C000-$FFFF into the map. Walking the
    // entries from highest start address down to lowest lets a lower-start
    // device overwrite any overlap, so a lookup returns exactly what the
    // linear "first match wins" scan would (m_entries is sorted ascending by
    // start). Ranges below $C000 (main RAM) contribute nothing to the I/O map.
    for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it)
    {
        int lo = max<int> (it->start, kIoMapBase);
        int hi = it->end;

        for (int address = lo; address <= hi; address++)
        {
            m_ioDeviceMap[address - kIoMapBase] = it->device;
        }
    }
}
