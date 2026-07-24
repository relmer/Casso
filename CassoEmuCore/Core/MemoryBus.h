#pragma once

#include "Pch.h"
#include "MemoryDevice.h"

class Prng;





////////////////////////////////////////////////////////////////////////////////
//
//  BusEntry
//
////////////////////////////////////////////////////////////////////////////////

struct BusEntry
{
    Word            start;
    Word            end;
    MemoryDevice *  device;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBus
//
////////////////////////////////////////////////////////////////////////////////

class MemoryBus
{
public:
    MemoryBus ();

    Byte ReadByte     (Word address);
    void WriteByte    (Word address, Byte value);

    void AddDevice    (MemoryDevice * device);
    void RemoveDevice (MemoryDevice * device);

    HRESULT Validate () const;

    void Reset ();

    // Phase 4 split-reset fan-out. SoftResetAll calls SoftReset on every
    // attached device; PowerCycleAll calls PowerCycle, threading the
    // shared Prng so every DRAM-owning device sees the same deterministic
    // pattern (FR-034, FR-035, audit §10).
    void SoftResetAll  ();
    void PowerCycleAll (Prng & prng);

    const vector<BusEntry> & GetEntries () const { return m_entries; }

    // Page table for fast $0000-$BFFF access. Each page (256 bytes) maps to
    // a host buffer; null = read-only / writes ignored. Only used for RAM
    // pages -- the I/O range ($C000+) always goes through the device list.
    Byte * GetReadPage  (Word address)        { return m_readPage[address >> 8]; }
    Byte * GetWritePage (Word address)        { return m_writePage[address >> 8]; }

    // Set up page mapping (called by EmulatorShell after RAM is created)
    void SetReadPage  (int pageIndex, Byte * page);
    void SetWritePage (int pageIndex, Byte * page);

    // The 256-entry read-page table itself, for a CPU that wants to serve
    // RAM/ROM reads inline (bypassing the virtual read dispatch). Entries are
    // updated in place on banking changes, so the returned pointer stays valid.
    Byte * const * GetReadPageTable () const { return m_readPage; }

    // Video-dirty tracking. Pages the renderer reads (text/hi-res, main +
    // aux, since aux is re-pointed at the same page index) are marked
    // "watched"; a write into any of them, or any banking change, raises
    // m_videoDirty. The render loop consults VideoDirty() to skip
    // re-rasterizing an unchanged screen, then ClearVideoDirty() after a
    // render. Starts dirty so the very first frame paints.
    void SetVideoWatchPage (int pageIndex, bool watched)
    {
        if (pageIndex >= 0 && pageIndex < 0x100)
        {
            m_videoWatched[pageIndex] = watched;
        }
    }
    bool VideoDirty      () const { return m_videoDirty; }
    void MarkVideoDirty  ()       { m_videoDirty = true; }
    void ClearVideoDirty ()       { m_videoDirty = false; }

    // Banking-change callback (invoked by soft switches when banking state changes)
    using BankingChangedFn = function<void()>;
    void SetBankingChangedCallback (BankingChangedFn fn) { m_bankingChanged = move (fn); }
    void NotifyBankingChanged ()
    {
        // A banking change can swap which buffer (main vs aux) the renderer
        // reads for the display region without any write landing, so force
        // a repaint alongside the callback.
        m_videoDirty = true;

        if (m_bankingChanged)
        {
            m_bankingChanged ();
        }
    }

private:
    MemoryDevice * FindDevice (Word address) const;

    vector<BusEntry>        m_entries;
    Byte                    m_floatingBusValue = 0xFF;

    // Per-page redirection for $0000-$BFFF. Index is high byte of address.
    // Only entries 0x00-0xBF are meaningful; $C0+ stays device-routed.
    Byte *                  m_readPage [0x100] = {};
    Byte *                  m_writePage[0x100] = {};

    // Video-dirty tracking (see SetVideoWatchPage). m_videoWatched flags the
    // display pages; m_videoDirty is raised on a write to one of them or a
    // banking change, and starts true so the first frame renders.
    bool                    m_videoWatched[0x100] = {};
    bool                    m_videoDirty          = true;

    BankingChangedFn        m_bankingChanged;
};
