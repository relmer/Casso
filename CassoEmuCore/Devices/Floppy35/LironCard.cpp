#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"
#include "Devices/Floppy35/LironCard.h"




static constexpr Word      s_kSwitchMask = 0x0F;
static constexpr Word      s_kPageMask   = 0xFF;
static constexpr uint64_t  s_kNsPerSec   = 1'000'000'000;





////////////////////////////////////////////////////////////////////////////////
//
//  LironCard
//
//  The bus is the IWM's drive 1 and drive 2, since the phases are on the
//  connector whichever drive is selected.
//
////////////////////////////////////////////////////////////////////////////////

LironCard::LironCard (int slot, MemoryBus & bus) :
    m_slot     (slot),
    m_hostBus  (bus),
    m_slotPage (slot, *this)
{
    m_ioStart = (Word) (kSlotIoBase + slot * kSlotIoStride);
    m_ioEnd   = (Word) (m_ioStart + kSlotIoStride - 1);

    m_iwm.AttachDrive (0, &m_bus);
    m_iwm.AttachDrive (1, &m_bus);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~LironCard
//
////////////////////////////////////////////////////////////////////////////////

LironCard::~LironCard() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
//  ComponentRegistry factory. The card defaults to slot 5.
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<MemoryDevice> LironCard::Create (const DeviceConfig & config, MemoryBus & bus)
{
    constexpr int  kDefaultSlot = 5;
    int            slot         = config.hasSlot ? config.slot : kDefaultSlot;



    return std::make_unique<LironCard> (slot, bus);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte LironCard::Read (Word address)
{
    return Access (address, false, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void LironCard::Write (Word address, Byte value)
{
    Access (address, true, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
//  The Apple's power-on resets the IWM. The drives are separate devices on
//  the bus and continue to run. The card's time continues from where it was,
//  since the drives' clocks do.
//
////////////////////////////////////////////////////////////////////////////////

void LironCard::Reset()
{
    m_iwm.Reset();
    m_selected    = false;
    m_epochNs     = m_lastNs;
    m_epochCycles = (m_hostCycles != nullptr) ? *m_hostCycles : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSlotPageAccess
//
////////////////////////////////////////////////////////////////////////////////

Byte LironCard::OnSlotPageAccess (Word address, bool isWrite, Byte value)
{
    (void) isWrite;
    (void) value;

    m_selected = true;

    return m_rom.empty() ? (Byte) 0xFF : m_rom[(size_t) m_slot * 0x100 + (address & s_kPageMask)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadExpansionRom
//
////////////////////////////////////////////////////////////////////////////////

Byte LironCard::ReadExpansionRom (Word address)
{
    return m_rom.empty() ? (Byte) 0xFF : m_rom[address - kExpansion];
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFloppy35EventSink
//
////////////////////////////////////////////////////////////////////////////////

void LironCard::SetFloppy35EventSink (IFloppy35EventSink * sink)
{
    for (auto & drive : m_drives)
    {
        drive->SetFloppy35EventSink (sink);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFirmware
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LironCard::SetFirmware (std::vector<Byte> firmware)
{
    HRESULT  hr     = S_OK;
    size_t   length = firmware.size();



    m_rom.clear();
    CBREx (length == kRomSize, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH));

    m_rom = std::move (firmware);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetHostClock
//
////////////////////////////////////////////////////////////////////////////////

void LironCard::SetHostClock (const uint64_t * cycles, uint32_t clockHz)
{
    m_hostCycles  = cycles;
    m_hostHz      = clockHz;
    m_epochNs     = m_lastNs;
    m_epochCycles = (cycles != nullptr) ? *cycles : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddDrive
//
////////////////////////////////////////////////////////////////////////////////

void LironCard::AddDrive (std::unique_ptr<UniDisk35Drive> drive)
{
    m_bus.AddDevice (drive.get());
    m_drives.push_back (std::move (drive));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatchUp
//
////////////////////////////////////////////////////////////////////////////////

void LironCard::CatchUp()
{
    uint64_t  nowNs = GetNowNs();



    if (m_iwm.IsWriting())
    {
        m_iwm.AdvanceTo (nowNs);
    }

    m_bus.CatchUp (nowNs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Access
//
//  The drives run up to now, in the order that lets each side see the
//  other's bits: while the Apple writes, its IWM's cells go out first; while
//  it reads, the drives' cells come in first.
//
////////////////////////////////////////////////////////////////////////////////

Byte LironCard::Access (Word address, bool isWrite, Byte value)
{
    uint64_t  nowNs = GetNowNs();



    CatchUp();

    return m_iwm.Access (address & s_kSwitchMask, isWrite, value, nowNs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetNowNs
//
//  The Apple's time in nanoseconds, never moving back across a reset of the
//  Apple's cycle count.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t LironCard::GetNowNs()
{
    uint64_t  cycles  = (m_hostCycles != nullptr) ? *m_hostCycles : 0;
    uint64_t  elapsed = (cycles > m_epochCycles) ? cycles - m_epochCycles : 0;



    if (m_hostHz != 0)
    {
        m_lastNs = std::max (m_lastNs, m_epochNs + elapsed / m_hostHz * s_kNsPerSec + elapsed % m_hostHz * s_kNsPerSec / m_hostHz);
    }

    return m_lastNs;
}
