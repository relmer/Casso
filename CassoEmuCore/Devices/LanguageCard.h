#pragma once

#include "Pch.h"
#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"

class IMmu;





////////////////////////////////////////////////////////////////////////////////
//
//  LanguageCard
//
//  Bank-switching state machine for $D000-$FFFF via soft switches $C080-$C08F.
//
//  Per audit §3 / Sather UTAIIe §5-23:
//    - Pre-write arms on ANY odd-address read in $C080-$C08F (not same
//      address twice). A second odd-address read while armed enables
//      WRITERAM. A write to any LC region or any even $C08x clears the
//      arm.
//    - Power-on default is BANK2 | WRITERAM (per Sather; SHIM-WRITE is
//      pre-armed).
//    - On //e soft reset, flag state resets to power-on default; all four
//      RAM banks (main bank1, main bank2, main high, plus aux variants
//      when ALTZP is set) are PRESERVED.
//
//  Aux-LC integration: when the //e MMU's ALTZP flag is set, the
//  $D000-$FFFF window backs to a separate 12 KiB-of-aux storage. The LC
//  owns both main and aux RAM banks; the MMU pointer is queried per
//  access to pick the side. ][/][+ machines pass m_mmu == nullptr and
//  use only the main banks (zero behavioral drift).
//
////////////////////////////////////////////////////////////////////////////////

enum LcFlag : Word
{
    kLcFlagBank2    = 0x01,
    kLcFlagReadRam  = 0x02,
    kLcFlagWriteRam = 0x04,
};

constexpr Word kLcFlagsPowerOn = static_cast<Word> (kLcFlagBank2 | kLcFlagWriteRam);



class LanguageCard : public MemoryDevice
{
public:
    LanguageCard (MemoryBus & bus);

    Byte Read     (Word address) override;
    void Write    (Word address, Byte value) override;
    Word GetStart () const override { return 0xC080; }
    Word GetEnd   () const override { return 0xC08F; }
    void Reset    () override;

    Byte ReadRam  (Word address);
    void WriteRam (Word address, Byte value);

    bool IsReadRam    () const { return (m_flags & kLcFlagReadRam)  != 0; }
    bool IsWriteRam   () const { return (m_flags & kLcFlagWriteRam) != 0; }
    bool IsBank2      () const { return (m_flags & kLcFlagBank2)    != 0; }
    int  GetPreWriteCount () const { return m_preWriteCount; }

    void SetMmu       (IMmu * mmu) { m_mmu = mmu; }

    void SetRomData (const vector<Byte> & rom) { m_romData = rom; }
    Byte ReadRom    (Word address) const;

    void SoftReset    ();
    void PowerCycle   (Prng & prng) override;

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    void ApplySwitch  (Byte switchAddr, bool isWrite);
    Byte * SelectBank4K (Word address);
    Byte * SelectMainHigh (Word address);

    MemoryBus &    m_bus;

    vector<Byte>   m_ramBank1Main;
    vector<Byte>   m_ramBank2Main;
    vector<Byte>   m_ramMainHigh;

    vector<Byte>   m_ramBank1Aux;
    vector<Byte>   m_ramBank2Aux;
    vector<Byte>   m_ramAuxHigh;

    vector<Byte>   m_romData;

    IMmu *         m_mmu            = nullptr;
    Word           m_flags          = kLcFlagsPowerOn;
    int            m_preWriteCount  = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  LanguageCardBank
//
//  Memory intercept device at $D000-$FFFF that routes reads/writes through
//  the LanguageCard. When IsReadRam() is true, reads come from LC RAM
//  (main or aux per ALTZP). When false, reads fall through to ROM.
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
