#include "Pch.h"

#include "LanguageCard.h"
#include "Devices/IMmu.h"
#include "Core/Prng.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Memory map constants
//
////////////////////////////////////////////////////////////////////////////////

static constexpr Word  kLc4KSize           = 0x1000;
static constexpr Word  kLcHighSize         = 0x2000;
static constexpr Word  kLcRomDataSize      = 0x3000;
static constexpr Word  kLcWindowStart      = 0xD000;
static constexpr Word  kLcBank2Last        = 0xDFFF;
static constexpr Word  kLcHighStart        = 0xE000;
static constexpr Word  kLcWindowLast       = 0xFFFF;

static constexpr Byte  kSwitchOddMask      = 0x01;
static constexpr Byte  kSwitchBankMask     = 0x08;
static constexpr Byte  kSwitchReadBitsMask = 0x03;
static constexpr Byte  kSwitchReadRamA     = 0x00;
static constexpr Byte  kSwitchReadRamB     = 0x03;
static constexpr int   kPreWriteArmTarget  = 2;





////////////////////////////////////////////////////////////////////////////////
//
//  LanguageCard
//
//  The six 4 KiB / 8 KiB buffers below hold all LC RAM. The four-bank
//  layout (main bank1 / main bank2 / main high; plus
//  aux variants for the //e). Buffers are heap-allocated so they survive
//  Reset() — destroying RAM on reset is audit C7 (CRITICAL) and is the bug
//  this rewrite fixes.
//
////////////////////////////////////////////////////////////////////////////////

LanguageCard::LanguageCard (MemoryBus & bus)
    : m_bus           (bus),
      m_ramBank1Main  (kLc4KSize,   0),
      m_ramBank2Main  (kLc4KSize,   0),
      m_ramMainHigh   (kLcHighSize, 0),
      m_ramBank1Aux   (kLc4KSize,   0),
      m_ramBank2Aux   (kLc4KSize,   0),
      m_ramAuxHigh    (kLcHighSize, 0)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  Reads of $C080-$C08F apply the soft-switch decode AND advance the
//  pre-write arm counter on odd-addressed accesses. Returns 0 on the bus
//  (the //e LC ignores the data byte for these IO reads; floating-bus
//  fidelity isn't required since callers only care about the side
//  effects).
//
////////////////////////////////////////////////////////////////////////////////

Byte LanguageCard::Read (Word address)
{
    Byte  switchAddr = static_cast<Byte> (address & 0x0F);

    ApplySwitch (switchAddr, false);

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
//  Writes apply the bank/read decode but NEVER advance the pre-write arm.
//  A write that targets an odd-addressed switch RESETS the arm counter
//  (audit M7 / Sather UTAIIe §5-23).
//
////////////////////////////////////////////////////////////////////////////////

void LanguageCard::Write (Word address, Byte value)
{
    UNREFERENCED_PARAMETER (value);

    Byte  switchAddr = static_cast<Byte> (address & 0x0F);

    ApplySwitch (switchAddr, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplySwitch
//
//  Reference state machine (per audit §3 / Sather UTAIIe §5-23):
//
//    - Bank: bit 3 inverted -> bank2 when bit3==0, bank1 when bit3==1.
//    - Read source: low 2 bits decide; pattern (b1==b0) -> READRAM else
//      ROM (the ((x&2)>>1) == (x&1) trick yields x in {00,11}).
//    - Pre-write arm: any odd-addressed READ increments the arm counter
//      (cap at kPreWriteArmTarget). When the counter reaches the target
//      AND the current access is an odd-addressed read, WRITERAM latches.
//    - Any even-addressed access (read or write) latches WRITERAM off
//      and clears the arm counter.
//    - A write to an odd-addressed switch clears the arm counter (so
//      the pending odd-read sequence is canceled) but does NOT clear an
//      already-latched WRITERAM (audit M7 / Sather UTAIIe §5-23).
//
////////////////////////////////////////////////////////////////////////////////

void LanguageCard::ApplySwitch (Byte switchAddr, bool isWrite)
{
    Byte  readBits = static_cast<Byte> (switchAddr & kSwitchReadBitsMask);
    bool  isOdd    = (switchAddr & kSwitchOddMask) != 0;
    bool  bank2    = (switchAddr & kSwitchBankMask) == 0;
    bool  readRam  = (readBits == kSwitchReadRamA) || (readBits == kSwitchReadRamB);



    m_flags &= static_cast<Word> (~(kLcFlagBank2 | kLcFlagReadRam));

    if (bank2)
    {
        m_flags |= kLcFlagBank2;
    }

    if (readRam)
    {
        m_flags |= kLcFlagReadRam;
    }

    if (!isOdd)
    {
        m_preWriteCount = 0;
        m_flags &= static_cast<Word> (~kLcFlagWriteRam);
        return;
    }

    if (isWrite)
    {
        m_preWriteCount = 0;
        return;
    }

    if (m_preWriteCount < kPreWriteArmTarget)
    {
        m_preWriteCount++;
    }

    if (m_preWriteCount >= kPreWriteArmTarget)
    {
        m_flags |= kLcFlagWriteRam;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectBank4K
//
//  Returns the active 4 KiB buffer covering $D000-$DFFF, factoring in
//  ALTZP (aux side) and BANK2.
//
////////////////////////////////////////////////////////////////////////////////

Byte * LanguageCard::SelectBank4K (Word address)
{
    bool  altZp = m_mmu != nullptr && m_mmu->GetAltZp ();
    bool  bank2 = (m_flags & kLcFlagBank2) != 0;



    UNREFERENCED_PARAMETER (address);

    if (altZp)
    {
        return bank2 ? m_ramBank2Aux.data () : m_ramBank1Aux.data ();
    }

    return bank2 ? m_ramBank2Main.data () : m_ramBank1Main.data ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectMainHigh
//
//  Returns the active 8 KiB buffer covering $E000-$FFFF (no banking;
//  main vs aux only).
//
////////////////////////////////////////////////////////////////////////////////

Byte * LanguageCard::SelectMainHigh (Word address)
{
    bool  altZp = m_mmu != nullptr && m_mmu->GetAltZp ();



    UNREFERENCED_PARAMETER (address);

    return altZp ? m_ramAuxHigh.data () : m_ramMainHigh.data ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadRam
//
////////////////////////////////////////////////////////////////////////////////

Byte LanguageCard::ReadRam (Word address)
{
    if (address >= kLcWindowStart && address <= kLcBank2Last)
    {
        Byte *  bank = SelectBank4K (address);
        return bank[address - kLcWindowStart];
    }

    if (address >= kLcHighStart && address <= kLcWindowLast)
    {
        Byte *  high = SelectMainHigh (address);
        return high[address - kLcHighStart];
    }

    return 0xFF;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteRam
//
////////////////////////////////////////////////////////////////////////////////

void LanguageCard::WriteRam (Word address, Byte value)
{
    if ((m_flags & kLcFlagWriteRam) == 0)
    {
        return;
    }

    if (address >= kLcWindowStart && address <= kLcBank2Last)
    {
        Byte *  bank = SelectBank4K (address);
        bank[address - kLcWindowStart] = value;
        return;
    }

    if (address >= kLcHighStart && address <= kLcWindowLast)
    {
        Byte *  high = SelectMainHigh (address);
        high[address - kLcHighStart] = value;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset / SoftReset
//
//  Reset() forwards to SoftReset() to preserve LC RAM contents (audit C7).
//  Power-cycle RAM seeding lands in Phase 4 (T047+).
//
////////////////////////////////////////////////////////////////////////////////

void LanguageCard::Reset ()
{
    SoftReset ();
}



void LanguageCard::SoftReset ()
{
    m_flags         = kLcFlagsPowerOn;
    m_preWriteCount = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
//  FR-035 / audit §10: re-seed all six LC RAM banks (main bank1/bank2/
//  high + aux bank1/bank2/high) from the shared Prng. SoftReset semantics
//  are applied to the flag state.
//
////////////////////////////////////////////////////////////////////////////////

void LanguageCard::PowerCycle (Prng & prng)
{
    SoftReset ();

    prng.Fill (m_ramBank1Main.data (), m_ramBank1Main.size ());
    prng.Fill (m_ramBank2Main.data (), m_ramBank2Main.size ());
    prng.Fill (m_ramMainHigh.data  (), m_ramMainHigh.size  ());
    prng.Fill (m_ramBank1Aux.data  (), m_ramBank1Aux.size  ());
    prng.Fill (m_ramBank2Aux.data  (), m_ramBank2Aux.size  ());
    prng.Fill (m_ramAuxHigh.data   (), m_ramAuxHigh.size   ());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadRom
//
////////////////////////////////////////////////////////////////////////////////

Byte LanguageCard::ReadRom (Word address) const
{
    if (m_romData.empty () || address < kLcWindowStart)
    {
        return 0xFF;
    }

    size_t  offset = static_cast<size_t> (address - kLcWindowStart);

    if (offset < m_romData.size ())
    {
        return m_romData[offset];
    }

    return 0xFF;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> LanguageCard::Create (const DeviceConfig & config, MemoryBus & bus)
{
    UNREFERENCED_PARAMETER (config);

    return make_unique<LanguageCard> (bus);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LanguageCardBank
//
////////////////////////////////////////////////////////////////////////////////

LanguageCardBank::LanguageCardBank (LanguageCard & lc)
    : m_lc (lc)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  LanguageCardBank::Read
//
////////////////////////////////////////////////////////////////////////////////

Byte LanguageCardBank::Read (Word address)
{
    if (m_lc.IsReadRam ())
    {
        return m_lc.ReadRam (address);
    }

    return m_lc.ReadRom (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LanguageCardBank::Write
//
////////////////////////////////////////////////////////////////////////////////

void LanguageCardBank::Write (Word address, Byte value)
{
    m_lc.WriteRam (address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LanguageCardBank::Reset
//
////////////////////////////////////////////////////////////////////////////////

void LanguageCardBank::Reset ()
{
}
