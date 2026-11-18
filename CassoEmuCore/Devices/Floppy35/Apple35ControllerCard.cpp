#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"
#include "Devices/Floppy35/Apple35ControllerCard.h"




static constexpr Byte  s_kBankMask = 0x0F;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple35ControllerCard
//
////////////////////////////////////////////////////////////////////////////////

Apple35ControllerCard::Apple35ControllerCard (int slot, MemoryBus & bus) :
    m_slot     (slot),
    m_bus      (bus),
    m_slotPage (slot, *this)
{
    HRESULT  hr = S_OK;



    m_ioStart = (Word) (kSlotIoBase + slot * kSlotIoStride);
    m_ioEnd   = (Word) (m_ioStart + kSlotIoStride - 1);

    m_memory.SetCycleSource (m_cpu.GetCyclePtr());

    hr = m_cpu.Initialize (m_memory);
    IGNORE_RETURN_VALUE (hr, S_OK);

    MapCardPages();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~Apple35ControllerCard
//
////////////////////////////////////////////////////////////////////////////////

Apple35ControllerCard::~Apple35ControllerCard() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
//  ComponentRegistry factory. The card defaults to slot 5.
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<MemoryDevice> Apple35ControllerCard::Create (const DeviceConfig & config, MemoryBus & bus)
{
    constexpr int  kDefaultSlot = 5;
    int            slot         = config.hasSlot ? config.slot : kDefaultSlot;



    return std::make_unique<Apple35ControllerCard> (slot, bus);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  Any access to the card's I/O window latches the bank.
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple35ControllerCard::Read (Word address)
{
    CatchUp();
    m_bank = (Byte) (address & s_kBankMask);

    return m_bus.GetFloatingBusValue();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPeekIo
//
//  The window returns the floating bus. The bank latch a read sets is left
//  alone.
//
////////////////////////////////////////////////////////////////////////////////

bool Apple35ControllerCard::TryPeekIo (Word address, Byte & value)
{
    (void) address;

    value = m_bus.GetFloatingBusValue();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void Apple35ControllerCard::Write (Word address, Byte value)
{
    (void) value;

    CatchUp();
    m_bank = (Byte) (address & s_kBankMask);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void Apple35ControllerCard::Reset()
{
    SoftReset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  The Apple's RESET line resets the card's CPU, which starts its firmware
//  again, and clears the bank.
//
////////////////////////////////////////////////////////////////////////////////

void Apple35ControllerCard::SoftReset()
{
    m_bank     = 0;
    m_selected = false;
    m_memory.Reset();
    m_cpu.Reset();
    m_hostSynced = (m_hostCycles != nullptr) ? *m_hostCycles : 0;
    m_cardTarget = m_cpu.GetCycles();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSlotPageAccess
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple35ControllerCard::OnSlotPageAccess (Word address, bool isWrite, Byte value)
{
    constexpr Word  kPageMask = 0xFF;



    (void) isWrite;
    (void) value;

    CatchUp();
    m_selected = true;

    return m_memory.GetRam()[kPageRam + (address & kPageMask)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPeekSlotPage
//
//  The card's RAM page as of the card's last catch-up: the card's CPU
//  doesn't run, and the expansion ROM's selection is left alone.
//
////////////////////////////////////////////////////////////////////////////////

bool Apple35ControllerCard::TryPeekSlotPage (Word address, Byte & value) const
{
    constexpr Word  kPageMask = 0xFF;



    value = m_memory.GetRam()[kPageRam + (address & kPageMask)];

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadExpansionRom
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple35ControllerCard::ReadExpansionRom (Word address)
{
    CatchUp();

    return m_memory.GetRam()[GetRamAddress (address)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPeekExpansionRom
//
//  The card's RAM as of the card's last catch-up, without running the
//  card's CPU.
//
////////////////////////////////////////////////////////////////////////////////

bool Apple35ControllerCard::TryPeekExpansionRom (Word address, Byte & value) const
{
    value = m_memory.GetRam()[GetRamAddress (address)];

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteExpansionRom
//
////////////////////////////////////////////////////////////////////////////////

void Apple35ControllerCard::WriteExpansionRom (Word address, Byte value)
{
    CatchUp();

    m_memory.GetRam()[GetRamAddress (address)] = value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFloppy35EventSink
//
////////////////////////////////////////////////////////////////////////////////

void Apple35ControllerCard::SetFloppy35EventSink (IFloppy35EventSink * sink)
{
    for (int i = 0; i < Apple35CardMemory::kDrives; i++)
    {
        m_memory.GetDrive (i).SetFloppy35EventSink (sink);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFirmware
//
//  The card's CPU starts its firmware once the firmware is in.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple35ControllerCard::SetFirmware (std::vector<Byte> firmware)
{
    HRESULT  hr = S_OK;



    hr = m_memory.SetFirmware (std::move (firmware));
    CHR (hr);

    MapCardPages();
    m_cpu.Reset();

    if (IsIdleLoopInRom())
    {
        m_cpu.SetIdleLoop (kIdleLoop, kIdlePassCycles, [this] (uint64_t cyclesLeft) { return SkipIdleLoop (cyclesLeft); });
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetHostClock
//
////////////////////////////////////////////////////////////////////////////////

void Apple35ControllerCard::SetHostClock (const uint64_t * cycles, uint32_t clockHz)
{
    m_hostCycles = cycles;
    m_hostHz     = clockHz;
    m_hostSynced = (cycles != nullptr) ? *cycles : 0;
    m_cardTarget = m_cpu.GetCycles();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatchUp
//
//  The card's CPU runs the cycles that match the Apple's since the last
//  catch-up, at most four million at a time: a card left alone for longer
//  is in its idle loop, and the difference is not visible. The target
//  counts from the last target, not from where the CPU stopped, so a CPU
//  that ran past it or stopped short in its idle loop makes up the
//  difference.
//
////////////////////////////////////////////////////////////////////////////////

void Apple35ControllerCard::CatchUp (CatchUpKind kind)
{
    uint64_t  now      = 0;
    uint64_t  elapsed  = 0;
    uint64_t  cardRun  = 0;



    if (m_hostCycles == nullptr || m_hostHz == 0 || !m_memory.HasFirmware())
    {
        return;
    }

    now          = *m_hostCycles;
    elapsed      = now > m_hostSynced ? now - m_hostSynced : 0;
    m_hostSynced = now;
    cardRun      = std::min (elapsed * kCardClockHz / m_hostHz, kMaxCatchUp);

    m_cardTarget += cardRun;

    m_cpu.RunUntil (m_cardTarget, kind);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SkipIdleLoop
//
//  The firmware's loop at $D1A6 polls for a command from the Apple. A pass
//  decrements the counter at $AD, and when that is zero, the counter at
//  $AE; when that is zero, it decrements X, and when X is zero it calls
//  $D8DD. It then reads the mailbox at $00 and loops while its
//  bit 7 is set. A pass is 14 cycles, 21 with $AE decremented and 25 with X
//  decremented too, with no I/O. The Apple writes the mailbox only between
//  catch-ups. The passes before the one that calls $D8DD are skipped, and
//  that one runs.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t Apple35ControllerCard::SkipIdleLoop (uint64_t cyclesLeft)
{
    constexpr Word       kMailbox     = 0x00;
    constexpr Word       kCounterLow  = 0xAD;
    constexpr Word       kCounterHigh = 0xAE;
    constexpr Byte       kNegative    = 0x80;
    constexpr Byte       kZero        = 0x02;
    constexpr uint64_t   kHighPass    = 21;
    constexpr uint64_t   kXPass       = 25;
    std::vector<Byte>  & ram          = m_memory.GetRam();
    Cpu6502Registers     regs         = m_cpu.GetRegisters();
    bool                 waiting      = (ram[kMailbox] & kNegative) != 0 && regs.a == ram[kMailbox] && (regs.p & (kNegative | kZero)) == kNegative;
    uint64_t             skipped      = 0;
    bool                 done         = !waiting;



    while (!done)
    {
        Byte      low    = ram[kCounterLow];
        uint64_t  quick  = (low == 0) ? 0xFF : (uint64_t) low - 1;
        uint64_t  fits   = (cyclesLeft - skipped) / kIdlePassCycles;
        Byte      high   = (Byte) (ram[kCounterHigh] - 1);
        uint64_t  wrap   = (high != 0) ? kHighPass : kXPass;



        //  The passes that decrement only $AD.
        ram[kCounterLow] = (Byte) (low - std::min (quick, fits));
        skipped         += std::min (quick, fits) * kIdlePassCycles;

        //  The pass that takes $AD to zero, unless it is the one that calls
        //  $D8DD or doesn't fit.
        done = fits < quick || (high == 0 && regs.x == 1) || cyclesLeft - skipped < wrap;

        if (!done)
        {
            ram[kCounterLow]  = 0;
            ram[kCounterHigh] = high;
            regs.x            = (high == 0) ? (Byte) (regs.x - 1) : regs.x;
            skipped          += wrap;
        }
    }

    if (skipped > 0)
    {
        m_cpu.SetRegisters (regs);
    }

    return skipped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsIdleLoopInRom
//
//  Whether the ROM's bytes at the idle loop are the ones SkipIdleLoop is
//  written for.
//
////////////////////////////////////////////////////////////////////////////////

bool Apple35ControllerCard::IsIdleLoopInRom()
{
    constexpr Word             kRomStart = 0x8000;
    static const Byte          s_kLoop[] = { 0xC6, 0xAD, 0xD0, 0x0C, 0xC6, 0xAE, 0xD0, 0x08, 0xCA, 0xD0, 0x05, 0x20, 0xDD, 0xD8, 0xA2, 0x03, 0xA5, 0x00, 0x30, 0xEC };
    const std::vector<Byte>  & rom       = m_memory.GetRom();
    size_t                     offset    = (size_t) (kIdleLoop - kRomStart);



    return rom.size() >= offset + std::size (s_kLoop) && std::equal (std::begin (s_kLoop), std::end (s_kLoop), rom.begin() + (ptrdiff_t) offset);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MapCardPages
//
//  The card CPU reads and writes RAM and reads ROM straight from the card's
//  memory. The I/O page and writes to ROM go to the card memory device.
//
////////////////////////////////////////////////////////////////////////////////

void Apple35ControllerCard::MapCardPages()
{
    constexpr int     kIoPage    = 0x0A;
    constexpr int     kRamPages  = 0x80;
    constexpr int     kRomPages  = 0x80;
    constexpr int     kAfterIo   = kIoPage + 1;
    constexpr Word    kPageBytes = 0x100;
    Byte            * ram        = m_memory.GetRam().data();
    Byte            * rom        = m_memory.HasFirmware() ? m_memory.GetRom().data() : nullptr;



    m_cpu.MapPages (0,         kIoPage,              ram,                          ram);
    m_cpu.MapPages (kIoPage,   1,                    nullptr,                      nullptr);
    m_cpu.MapPages (kAfterIo,  kRamPages - kAfterIo, ram + kAfterIo * kPageBytes,  ram + kAfterIo * kPageBytes);
    m_cpu.MapPages (kRamPages, kRomPages,            rom,                          nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRamAddress
//
////////////////////////////////////////////////////////////////////////////////

Word Apple35ControllerCard::GetRamAddress (Word address) const
{
    if (address < kFixedWindow)
    {
        return (Word) (m_bank * kBankBytes + (address - kBankWindow));
    }

    return (Word) (kFixedRam + (address - kFixedWindow));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Apple35ControllerCard::GetReplayMedia
//
//  Each drive the card has.
//
////////////////////////////////////////////////////////////////////////////////

void Apple35ControllerCard::GetReplayMedia (std::vector<IReplayMedia *> & outDrives)
{
    int  drive = 0;



    for (drive = 0; drive < Apple35CardMemory::kDrives; drive++)
    {
        if (m_memory.HasDrive (drive))
        {
            outDrives.push_back (&m_memory.GetDrive (drive));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Apple35ControllerCard::SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple35ControllerCard::SaveState (StateWriter & writer) const
{
    HRESULT  hr = S_OK;



    writer.BeginSection (kStateTag, kStateVersion);

    hr = m_memory.SaveState (writer);
    CHR (hr);

    hr = m_cpu.SaveState (writer);
    CHR (hr);

    writer.WriteUInt64 (m_hostSynced);
    writer.WriteUInt64 (m_cardTarget);
    writer.WriteByte   (m_bank);
    writer.WriteBool   (m_selected);

    hr = writer.EndSection();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Apple35ControllerCard::LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple35ControllerCard::LoadState (StateReader & reader)
{
    HRESULT   hr      = S_OK;
    uint16_t  version = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    hr = m_memory.LoadState (reader);
    CHR (hr);

    hr = m_cpu.LoadState (reader);
    CHR (hr);

    reader.ReadUInt64 (m_hostSynced);
    reader.ReadUInt64 (m_cardTarget);
    reader.ReadByte   (m_bank);
    reader.ReadBool   (m_selected);

    hr = reader.EndSection();

Error:
    return hr;
}
