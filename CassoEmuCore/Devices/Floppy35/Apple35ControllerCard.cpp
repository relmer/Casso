#include "Pch.h"

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
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatchUp
//
//  The card's CPU runs the cycles that match the Apple's since the last
//  catch-up, at most four million at a time: a card left alone for longer
//  is waiting in its loop, and the difference is not visible.
//
////////////////////////////////////////////////////////////////////////////////

void Apple35ControllerCard::CatchUp()
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

    m_cpu.RunUntil (m_cpu.GetCycles() + cardRun);
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
