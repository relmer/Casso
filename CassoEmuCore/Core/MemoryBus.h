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

    // Banking-change callback (invoked by soft switches when banking state changes)
    using BankingChangedFn = function<void()>;
    void SetBankingChangedCallback (BankingChangedFn fn) { m_bankingChanged = move (fn); }
    void NotifyBankingChanged ()
    {
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

    BankingChangedFn        m_bankingChanged;
};
