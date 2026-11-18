#include "Pch.h"

#include "Apple3ProFileCard.h"
#include "Core/MemoryBus.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"





static constexpr Word  s_kSlotIoBase   = 0xC080;
static constexpr Word  s_kSlotIoStride = 0x10;
static constexpr Word  s_kDataOut      = 0x0;
static constexpr Word  s_kDataIn       = 0x1;
static constexpr Word  s_kStatus       = 0x2;
static constexpr Word  s_kPageMask     = 0xFF;
static constexpr Byte  s_kLineOffsets  = 0x10;
static constexpr Byte  s_kLineSelect   = 0x03;
static constexpr Byte  s_kLevelBit     = 0x04;
static constexpr Byte  s_kResetBit     = 0x08;
static constexpr Byte  s_kCmdLine      = 0x01;
static constexpr Byte  s_kResetLine    = 0x10;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3ProFileCard
//
//  The card starts with a 5 MB drive.
//
////////////////////////////////////////////////////////////////////////////////

Apple3ProFileCard::Apple3ProFileCard (int slot, MemoryBus & bus) :
    m_slot  (slot),
    m_bus   (bus),
    m_drive (std::make_unique<ProFileDrive> (slot, BlockDeviceCatalog::FindDrive (kpszProFile5Device)->maxBlocks))
{
    m_ioStart = (Word) (s_kSlotIoBase + slot * s_kSlotIoStride);
    m_ioEnd   = (Word) (m_ioStart + s_kSlotIoStride - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSlotPage
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3ProFileCard::IsSlotPage (Word address) const
{
    return (address >> 8) == (Word) (0xC0 + m_slot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  A read of the data register takes its byte off the drive, and a read of
//  $Cn00-$Cn0F sets a line.
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple3ProFileCard::Read (Word address)
{
    Byte  value = PeekIo (address);



    if (IsSlotPage (address))
    {
        if ((address & s_kPageMask) < s_kLineOffsets)
        {
            OnLineAccess ((Byte) (address & s_kPageMask));
        }
    }
    else if (address - m_ioStart == s_kDataIn)
    {
        m_drive->TakeData();
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PeekIo
//
//  The byte a read returns, without the read's change. The card has no
//  memory space, so its page and unused registers read the floating bus.
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple3ProFileCard::PeekIo (Word address) const
{
    Byte  value  = m_bus.GetFloatingBusValue();
    Word  offset = (Word) (address - m_ioStart);



    if (!IsSlotPage (address) && offset == s_kDataIn)
    {
        value = m_drive->PeekData();
    }
    else if (!IsSlotPage (address) && offset == s_kStatus)
    {
        value = m_drive->IsBusy() ? 0 : kStatusIdle;
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPeekIo
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3ProFileCard::TryPeekIo (Word address, Byte & value)
{
    value = PeekIo (address);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
//  A write of the data register puts its byte on the drive's bus. A write
//  to the slot's page sets a line as a read does.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3ProFileCard::Write (Word address, Byte value)
{
    if (IsSlotPage (address))
    {
        if ((address & s_kPageMask) < s_kLineOffsets)
        {
            OnLineAccess ((Byte) (address & s_kPageMask));
        }
    }
    else if (address - m_ioStart == s_kDataOut)
    {
        m_drive->WriteData (value);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
//  The Apple's reset line clears the line latch and the drive's exchange.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3ProFileCard::Reset()
{
    m_lines = 0;
    m_drive->Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLineAccess
//
//  Sets the line the offset selects to the offset's level. CMD rising
//  starts a handshake and CMD falling ends one; the reset line rising
//  resets the drive.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3ProFileCard::OnLineAccess (Byte offset)
{
    bool  isReset = (offset & s_kResetBit) != 0;
    Byte  line    = isReset ? s_kResetLine : (Byte) (1 << (offset & s_kLineSelect));
    bool  isHigh  = (offset & s_kLevelBit) != 0;
    bool  wasHigh = (m_lines & line) != 0;



    m_lines = isHigh ? (Byte) (m_lines | line) : (Byte) (m_lines & ~line);

    if (line == s_kCmdLine && isHigh && !wasHigh)
    {
        m_drive->StartHandshake();
    }
    else if (line == s_kCmdLine && !isHigh && wasHigh)
    {
        m_drive->EndHandshake();
    }
    else if (line == s_kResetLine && isHigh && !wasHigh)
    {
        m_drive->Reset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3ProFileCard::SaveState (StateWriter & writer) const
{
    HRESULT  hr = S_OK;



    writer.BeginSection (kStateTag, kStateVersion);
    writer.WriteByte (m_lines);

    hr = m_drive->SaveState (writer);
    CHR (hr);

    hr = writer.EndSection();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3ProFileCard::LoadState (StateReader & reader)
{
    HRESULT   hr      = S_OK;
    uint16_t  version = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadByte (m_lines);

    hr = m_drive->LoadState (reader);
    CHR (hr);

    hr = reader.EndSection();
    CHR (hr);

Error:
    return hr;
}
