#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Devices/BlockDevice/ProFileCard.h"




static constexpr Word  s_kDataOut      = 0x0;
static constexpr Word  s_kDataIn       = 0x1;
static constexpr Word  s_kStatus       = 0x2;
static constexpr Byte  s_kStatusIdle   = 0x80;
static constexpr Byte  s_kStatusAbsent = 0x01;
static constexpr Byte  s_kLineCount    = 8;
static constexpr Byte  s_kEndLine      = 0x01;
static constexpr Byte  s_kStartLine    = 0x04;
static constexpr Byte  s_kArmLine      = 0x08;





////////////////////////////////////////////////////////////////////////////////
//
//  ProFileCard
//
////////////////////////////////////////////////////////////////////////////////

ProFileCard::ProFileCard (int slot, MemoryBus & bus) :
    m_slot     (slot),
    m_bus      (bus),
    m_slotPage (slot, *this),
    m_drive    (std::make_unique<ProFileDrive> (slot, BlockDeviceCatalog::FindDrive (kpszProFile5Device)->maxBlocks))
{
    m_ioStart = (Word) (kSlotIoBase + slot * kSlotIoStride);
    m_ioEnd   = (Word) (m_ioStart + kSlotIoStride - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
//  ComponentRegistry factory. The card defaults to slot 5.
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<MemoryDevice> ProFileCard::Create (const DeviceConfig & config, MemoryBus & bus)
{
    constexpr int  kDefaultSlot = 5;
    int            slot         = config.hasSlot ? config.slot : kDefaultSlot;



    return std::make_unique<ProFileCard> (slot, bus);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte ProFileCard::Read (Word address)
{
    Word  offset = (Word) (address - m_ioStart);
    Byte  value  = m_bus.GetFloatingBusValue();



    if (offset == s_kDataIn)
    {
        value = m_drive->ReadData();
    }
    else if (offset == s_kStatus)
    {
        value = (Byte) ((m_drive->IsBusy() ? 0 : s_kStatusIdle) | (m_drive->IsPresent() ? 0 : s_kStatusAbsent));
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void ProFileCard::Write (Word address, Byte value)
{
    Word  offset = (Word) (address - m_ioStart);



    if (offset == s_kDataOut)
    {
        m_drive->WriteData (value);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void ProFileCard::Reset()
{
    SoftReset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  The Apple's RESET line clears the line latch and the drive's exchange.
//
////////////////////////////////////////////////////////////////////////////////

void ProFileCard::SoftReset()
{
    m_lines    = 0;
    m_selected = false;
    m_drive->Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSlotPageAccess
//
//  An access to $Cn00-$CnFF selects the expansion space and reads the ROM's
//  last page; one to $Cn00-$Cn07 also sets a handshake line.
//
////////////////////////////////////////////////////////////////////////////////

Byte ProFileCard::OnSlotPageAccess (Word address, bool isWrite, Byte value)
{
    constexpr Word  kPageMask = 0xFF;
    Byte            offset    = (Byte) (address & kPageMask);



    (void) isWrite;
    (void) value;

    m_selected = true;

    if (offset < s_kLineCount)
    {
        OnLineAccess (offset);
    }

    return m_rom.empty() ? m_bus.GetFloatingBusValue() : m_rom[kRomPageAt + offset];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadExpansionRom
//
////////////////////////////////////////////////////////////////////////////////

Byte ProFileCard::ReadExpansionRom (Word address)
{
    Word  offset = (Word) (address - kExpansionAt);



    return (m_rom.empty() || offset >= kRomPageAt) ? m_bus.GetFloatingBusValue() : m_rom[offset];
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFirmware
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProFileCard::SetFirmware (std::vector<Byte> firmware)
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
//  OnLineAccess
//
//  Sets the line the offset selects to the offset's low bit, then starts a
//  handshake on line 2's falling edge with line 3 set, or ends one on an
//  access to $Cn00.
//
////////////////////////////////////////////////////////////////////////////////

void ProFileCard::OnLineAccess (Byte offset)
{
    Byte  line    = (Byte) (1 << (offset >> 1));
    bool  isHigh  = (offset & 1) != 0;
    Byte  before  = m_lines;



    m_lines = isHigh ? (Byte) (m_lines | line) : (Byte) (m_lines & ~line);

    if (line == s_kStartLine && !isHigh && (before & s_kStartLine) != 0 && (before & s_kArmLine) != 0)
    {
        m_drive->StartHandshake();
    }
    else if (line == s_kEndLine && !isHigh)
    {
        m_drive->EndHandshake();
    }
}
