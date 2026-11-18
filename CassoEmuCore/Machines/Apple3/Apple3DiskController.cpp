#include "Pch.h"

#include "Apple3DiskController.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"





static constexpr Word  s_kExternal2Off = 0xC0D0;
static constexpr Word  s_kExternal2On  = 0xC0D1;
static constexpr Word  s_kExternal1Off = 0xC0D2;
static constexpr Word  s_kExternal1On  = 0xC0D3;
static constexpr Word  s_kInternalOn   = 0xC0D4;
static constexpr Word  s_kInternalOff  = 0xC0D5;

static constexpr int   s_kDriveD2      = 2;
static constexpr int   s_kDriveD3      = 3;
static constexpr int   s_kDriveD4      = 4;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3DiskController
//
//  The Disk II logic catches up to the disk cycle count at each access.
//
////////////////////////////////////////////////////////////////////////////////

Apple3DiskController::Apple3DiskController()
    : m_diskII (std::make_unique<Disk2Controller> (kDiskIiSlot))
{
    m_diskII->GetClock().SetSource (&m_diskCycles);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsControllerAddress
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3DiskController::IsControllerAddress (Word address) const
{
    bool  isSelect   = address >= kFirstSelect   && address <= kLastSelect;
    bool  isRegister = address >= kFirstRegister && address <= kLastRegister;



    return isSelect || isRegister;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  A motor select latch changes on any access and reads the floating bus,
//  which the caller supplies; it reads 0 here.
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple3DiskController::Read (Word address)
{
    Byte  value = 0;



    if (address >= kFirstSelect && address <= kLastSelect)
    {
        HandleSelect (address);
    }
    else if (address >= kFirstRegister && address <= kLastRegister)
    {
        value = m_diskII->Read (address);
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void Apple3DiskController::Write (Word address, Byte value)
{
    if (address >= kFirstSelect && address <= kLastSelect)
    {
        HandleSelect (address);
    }
    else if (address >= kFirstRegister && address <= kLastRegister)
    {
        m_diskII->Write (address, value);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleSelect
//
////////////////////////////////////////////////////////////////////////////////

void Apple3DiskController::HandleSelect (Word address)
{
    switch (address)
    {
    case s_kExternal2Off: m_isExternal2        = false; break;
    case s_kExternal2On:  m_isExternal2        = true;  break;
    case s_kExternal1Off: m_isExternal1        = false; break;
    case s_kExternal1On:  m_isExternal1        = true;  break;
    case s_kInternalOn:   m_isInternalSelected = true;  break;
    case s_kInternalOff:  m_isInternalSelected = false; break;
    default:                                            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSelectedExternal
//
////////////////////////////////////////////////////////////////////////////////

int Apple3DiskController::GetSelectedExternal() const
{
    int  drive = 0;



    if (m_isExternal1 && m_isExternal2)
    {
        drive = s_kDriveD4;
    }
    else if (m_isExternal1)
    {
        drive = s_kDriveD3;
    }
    else if (m_isExternal2)
    {
        drive = s_kDriveD2;
    }

    return drive;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncToMasterClocks
//
//  Converts the master clocks since the last call to disk cycles, keeping
//  the remainder, and runs the motor timers by them.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3DiskController::SyncToMasterClocks (uint64_t masterClocks)
{
    uint64_t  before = m_diskCycles;



    if (masterClocks < m_lastMasterClocks)
    {
        m_lastMasterClocks = masterClocks;
        return;
    }

    m_diskCycles      += (masterClocks - m_lastMasterClocks) / kMasterPerCycle;
    m_lastMasterClocks = masterClocks - (masterClocks - m_lastMasterClocks) % kMasterPerCycle;

    m_diskII->Tick ((uint32_t) (m_diskCycles - before));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
//  The latches and the clock counts, then the Disk II logic's section.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3DiskController::SaveState (StateWriter & writer) const
{
    HRESULT  hr = S_OK;



    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteBool   (m_isInternalSelected);
    writer.WriteBool   (m_isExternal1);
    writer.WriteBool   (m_isExternal2);
    writer.WriteUInt64 (m_diskCycles);
    writer.WriteUInt64 (m_lastMasterClocks);

    hr = m_diskII->SaveState (writer);
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

HRESULT Apple3DiskController::LoadState (StateReader & reader)
{
    HRESULT   hr      = S_OK;
    uint16_t  version = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadBool   (m_isInternalSelected);
    reader.ReadBool   (m_isExternal1);
    reader.ReadBool   (m_isExternal2);
    reader.ReadUInt64 (m_diskCycles);
    reader.ReadUInt64 (m_lastMasterClocks);

    hr = m_diskII->LoadState (reader);
    CHR (hr);

    hr = reader.EndSection();
    CHR (hr);

Error:
    return hr;
}
