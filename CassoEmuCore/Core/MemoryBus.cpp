#include "Pch.h"

#include "MemoryBus.h"
#include "Prng.h"
#include "RamPages.h"
#include "StateReader.h"
#include "StateWriter.h"





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

    m_writeFlag.assign (0x100, &m_flagSink);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~MemoryBus
//
//  Tells the owners of RAM still registered that the bus is gone, so they do
//  not unregister from it later.
//
////////////////////////////////////////////////////////////////////////////////

MemoryBus::~MemoryBus()
{
    for (const RamRegion & region : m_ramRegions)
    {
        if (region.owner != nullptr)
        {
            region.owner->OnBusDestroyed();
        }
    }
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
//  Unmapped I/O returns the FLOATING BUS. On an Apple II that is the byte the
//  video scanner fetched from RAM on the same cycle, which a machine supplies
//  through SetFloatingBusSource; software genuinely depends on it (vapor lock,
//  the classic video-sync detection, polls an undriven $C0xx for a byte it
//  placed in screen memory). With no source the lines hold the last value
//  any device drove. Outside that window an unmapped address reads as zero
//  instead, since there is no bus to float.
//
////////////////////////////////////////////////////////////////////////////////

Byte MemoryBus::ReadByte (Word address)
{
    // Fast path: page-table lookup. RAM ($0000-$BFFF) and, once the language
    // card wires them, ROM/LC RAM ($D000-$FFFF) have a mapped page; I/O
    // ($C000-$C0FF) and the //e's slot ROM space ($C100-$CFFF) stay null and
    // fall through to device dispatch so their read side effects run. On the
    // //c, the internal ROM pages $C1-$CF are mapped for reads, except $C3
    // and $CF, which keep their INTC8ROM side effects. This is the hottest
    // read in the emulator, so the mapped case stays one branch deep.
    //
    // The device dispatch is written out here rather than calling
    // ReadFromDevice, which holds the same code for ReadWatchedPage. Every ROM
    // fetch comes this way, and the call cost about 5% of emulation speed in
    // Release x64 (50M //e cycles: 288 ms with the call, 275 ms inline, 271 ms
    // before the watch mask existed).
    Byte *          page   = m_readPage[address >> 8];
    MemoryDevice *  device = nullptr;
    Byte            value  = 0;



    if (page != nullptr)
    {
        value = page[address & 0xFF];
    }
    else if (m_pathWatched[address >> 8])
    {
        value = ReadWatchedPage (address);
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
            value = ReadFloatingBus();
        }
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadFromDevice
//
//  The device-dispatch half of ReadByte, for an address with no mapped page.
//
////////////////////////////////////////////////////////////////////////////////

Byte MemoryBus::ReadFromDevice (Word address)
{
    MemoryDevice *  device = FindDevice (address);
    Byte            value  = 0;



    if (device != nullptr)
    {
        value              = device->Read (address);
        m_floatingBusValue = value;
    }
    else if (address >= 0xC000 && address <= 0xCFFF)
    {
        // Unmapped I/O reads the floating bus. Outside that window an
        // unmapped address reads as 0 instead -- there is no bus to float.
        value = ReadFloatingBus();
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadWatchedPage
//
//  A read of a watched page: served from the shadow page when the MMU mapped
//  one, otherwise from the device exactly as an unwatched read would be, and
//  then reported to the watch sink.
//
////////////////////////////////////////////////////////////////////////////////

Byte MemoryBus::ReadWatchedPage (Word address)
{
    Byte *  page  = m_shadowReadPage[address >> 8];
    Byte    value = 0;



    value = (page != nullptr) ? page[address & 0xFF] : ReadFromDevice (address);

    ReportAccess (address, value, BusAccess::Read, std::nullopt);

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

        // Written out rather than calling StoreToPage, for the reason ReadByte
        // gives: this is the hottest write, and the call is measurable.
        if (page != nullptr)
        {
            Byte * cell = &page[address & 0xFF];

            if (m_videoWatched[address >> 8]                           &&
                (address & s_kScreenBlockMask) < s_kFirstScreenHoleByte &&
                *cell != value)
            {
                m_videoDirty = true;
            }

            *cell = value;
            *m_writeFlag[address >> 8] = 1;
            return;
        }

        // No page mapping -- fall through to device-based write (e.g., for ROM areas)
    }

    if (m_pathWatched[address >> 8])
    {
        WriteWatchedPage (address, value);
        return;
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
//  StoreToPage
//
//  A write landing in a mapped page.
//
//  Video-dirty raise: only a write that actually CHANGES a *displayed* byte in
//  a watched page marks the frame for re-render. The watched check
//  short-circuits the common non-video write; the screen-hole check drops
//  undisplayed scratch writes; and the value compare drops same-value
//  re-stores -- so an idle screen whose firmware polls through the screen
//  holes stops re-rendering.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::StoreToPage (Byte * page, Word address, Byte value)
{
    Byte * cell = &page[address & 0xFF];



    if (m_videoWatched[address >> 8]                           &&
        (address & s_kScreenBlockMask) < s_kFirstScreenHoleByte &&
        *cell != value)
    {
        m_videoDirty = true;
    }

    *cell = value;
    *m_writeFlag[address >> 8] = 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteWatchedPage
//
//  A write to a watched page: stored through the shadow page when the MMU
//  mapped one, with the same video-dirty rule as the fast path, otherwise
//  dispatched to the device once; then reported to the watch sink. The byte a
//  memory write replaces is read before the store and reported with it; a
//  device write reports none, since reading a device back has side effects.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::WriteWatchedPage (Word address, Byte value)
{
    Byte *               page     = m_shadowWritePage[address >> 8];
    MemoryDevice *       device   = nullptr;
    std::optional<Byte>  previous;



    if (page != nullptr)
    {
        previous = page[address & 0xFF];
        StoreToPage (page, address, value);
    }
    else
    {
        device = FindDevice (address);

        if (device != nullptr)
        {
            device->Write (address, value);
        }

        m_floatingBusValue = value;
    }

    ReportAccess (address, value, BusAccess::Write, previous);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReportAccess
//
//  An access on the watched path goes to the trace sink, and to the watch
//  sink when a watchpoint's page holds it; while the trace publishes every
//  page, the rest are the trace's alone.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::ReportAccess (Word address, Byte value, BusAccess access, std::optional<Byte> previous)
{
    if (m_traceSink != nullptr)
    {
        m_traceSink->OnWatchedAccess (address, value, access, previous);
    }

    if (m_watchSink != nullptr && m_debugWatched[address >> 8])
    {
        m_watchSink->OnWatchedAccess (address, value, access, previous);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetReadPage / SetWritePage
//
//  The shadow table always takes the pointer; the published table takes it
//  unless the page is watched.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::SetReadPage (int pageIndex, Byte * page)
{
    if (pageIndex >= 0 && pageIndex < 0x100)
    {
        m_shadowReadPage[pageIndex] = page;
        m_readPage[pageIndex]       = m_pathWatched[pageIndex] ? nullptr : page;
    }
}

void MemoryBus::SetWritePage (int pageIndex, Byte * page)
{
    if (pageIndex >= 0 && pageIndex < 0x100)
    {
        m_shadowWritePage[pageIndex] = page;
        m_writePage[pageIndex]       = m_pathWatched[pageIndex] ? nullptr : page;
        m_writeFlag[pageIndex]       = FindPageFlag (page);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterRamPages
//
//  A RAM buffer the page table may map, with one written flag per 256-byte
//  page of it. Every store the bus makes into a page of the buffer sets that
//  page's flag. Pages mapped before the buffer registered are resolved again
//  now. The owner unregisters it before the buffer goes; a bus that goes
//  first tells every owner still registered, so neither outliving the other
//  leaves a dangling pointer.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::RegisterRamPages (
    RamPages    * owner,
    const Byte  * base,
    size_t        size,
    Byte        * pageFlags)
{
    UnregisterRamPages (base);

    m_ramRegions.push_back (RamRegion { owner, base, size, pageFlags });

    ResolvePageFlags();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UnregisterRamPages
//
//  The pages mapped into the buffer mark nothing from here on.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::UnregisterRamPages (const Byte * base)
{
    std::erase_if (m_ramRegions, [base] (const RamRegion & region) { return region.base == base; });

    ResolvePageFlags();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPageFlag
//
//  The written flag of the registered page holding page, or a sink for a
//  page no registered buffer holds, so the write paths store a flag without
//  testing for one. Also what MarkPointerWritten sets, for a store made
//  straight into a registered buffer.
//
////////////////////////////////////////////////////////////////////////////////

Byte * MemoryBus::FindPageFlag (const Byte * page)
{
    static constexpr size_t  kPageShift = 8;
    Byte                   * flag       = &m_flagSink;
    const RamRegion        * region     = nullptr;
    size_t                   count      = m_ramRegions.size();
    size_t                   tried      = 0;
    size_t                   index      = 0;



    // The region that held the last page first: a banking change maps a run
    // of pages from one buffer.
    for (tried = 0; tried < count && page != nullptr; tried++)
    {
        index  = (m_lastRegion + tried) % count;
        region = &m_ramRegions[index];

        if (page >= region->base && page < region->base + region->size)
        {
            flag         = &region->flags[static_cast<size_t> (page - region->base) >> kPageShift];
            m_lastRegion = index;
            break;
        }
    }

    return flag;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolvePageFlags
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::ResolvePageFlags()
{
    for (int pageIndex = 0; pageIndex < 0x100; pageIndex++)
    {
        m_writeFlag[pageIndex] = FindPageFlag (m_shadowWritePage[pageIndex]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetWatchedPage
//
//  Watching a page unpublishes it; unwatching republishes whatever the MMU set
//  in the meantime.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::SetWatchedPage (int pageIndex, bool watched)
{
    if (pageIndex >= 0 && pageIndex < 0x100)
    {
        m_debugWatched[pageIndex] = watched;
        PublishPage (pageIndex);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTraceAllPages
//
//  On, every page takes the watched path, so the trace sink sees every
//  access; off, each page goes back to what the watch mask says.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::SetTraceAllPages (bool on)
{
    m_traceAllPages = on;

    for (int pageIndex = 0; pageIndex < 0x100; pageIndex++)
    {
        PublishPage (pageIndex);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetAllPagesWatched
//
//  For the heat map, which the CPU reports its accesses to: on, every page
//  takes the watched path, so no read or write is served from a page table
//  the CPU reads inline; off, each page goes back to what the watch mask and
//  the trace say.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::SetAllPagesWatched (bool on)
{
    m_allPagesWatched = on;

    for (int pageIndex = 0; pageIndex < 0x100; pageIndex++)
    {
        PublishPage (pageIndex);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PublishPage
//
//  A page on the watched path is published as null in both tables; any other
//  gets what the MMU set in the meantime.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryBus::PublishPage (int pageIndex)
{
    bool  watched = m_debugWatched[pageIndex] || m_traceAllPages || m_allPagesWatched;



    m_pathWatched[pageIndex] = watched;
    m_readPage[pageIndex]    = watched ? nullptr : m_shadowReadPage[pageIndex];
    m_writePage[pageIndex]   = watched ? nullptr : m_shadowWritePage[pageIndex];
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetWatchedPageCount
//
////////////////////////////////////////////////////////////////////////////////

int MemoryBus::GetWatchedPageCount() const
{
    return (int) std::count (std::begin (m_pathWatched), std::end (m_pathWatched), true);
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





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MemoryBus::SaveState (StateWriter & writer) const
{
    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteByte (m_floatingBusValue);

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MemoryBus::LoadState (StateReader & reader)
{
    HRESULT   hr      = S_OK;
    uint16_t  version = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadByte (m_floatingBusValue);

    hr = reader.EndSection();
    CHR (hr);

    m_videoDirty = true;

Error:
    return hr;
}