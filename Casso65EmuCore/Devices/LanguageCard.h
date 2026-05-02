#pragma once

#include "Pch.h"
#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LanguageCard
//
//  Bank-switching state machine for $D000-$FFFF via soft switches $C080-$C08F.
//
////////////////////////////////////////////////////////////////////////////////

class LanguageCard : public MemoryDevice
{
public:
    LanguageCard (MemoryBus & bus);

    Byte Read     (Word address) override;
    void Write    (Word address, Byte value) override;
    Word GetStart () const override { return 0xC080; }
    Word GetEnd   () const override { return 0xC08F; }
    void Reset    () override;

    // RAM access for the $D000-$FFFF region
    Byte ReadRam  (Word address);
    void WriteRam (Word address, Byte value);

    bool IsReadRam    () const { return m_readRam; }
    bool IsWriteRam   () const { return m_writeRam; }
    bool IsBank2      () const { return m_bank2Select; }

    // Store ROM data so LanguageCardBank can serve ROM reads
    void SetRomData (const vector<Byte> & rom) { m_romData = rom; }
    Byte ReadRom    (Word address) const;

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    void UpdateState (Byte switchAddr);

    MemoryBus &         m_bus;

    vector<Byte>   m_ramBank1;     // 4KB bank 1 at $D000-$DFFF
    vector<Byte>   m_ramBank2;     // 4KB bank 2 at $D000-$DFFF
    vector<Byte>   m_ramMain;      // 8KB at $E000-$FFFF
    vector<Byte>   m_romData;      // Original ROM data for fallback reads

    bool    m_readRam     = false;
    bool    m_writeRam    = false;
    bool    m_preWrite    = false;
    bool    m_bank2Select = true;
    Byte    m_lastSwitch  = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  LanguageCardBank
//
//  Memory intercept device at $D000-$FFFF that routes reads/writes through the
//  LanguageCard. When readRam is true, reads come from LC RAM. When false,
//  reads fall through to stored ROM data.
//
////////////////////////////////////////////////////////////////////////////////

class LanguageCardBank : public MemoryDevice
{
public:
    explicit LanguageCardBank (LanguageCard & lc);

    Byte Read     (Word address) override;
    void Write    (Word address, Byte value) override;
    Word GetStart () const override { return 0xD000; }
    Word GetEnd   () const override { return 0xFFFF; }
    void Reset    () override;

private:
    LanguageCard & m_lc;
};