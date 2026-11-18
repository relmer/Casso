#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/Floppy35/SmartPortHost.h"




static constexpr uint64_t  s_kNsPerSec = 1'000'000'000;





////////////////////////////////////////////////////////////////////////////////
//
//  SmartPortHost
//
////////////////////////////////////////////////////////////////////////////////

SmartPortHost::SmartPortHost()
{
    m_iwm.AttachDrive (0, &m_bus);
    m_iwm.AttachDrive (1, &m_bus);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Access
//
////////////////////////////////////////////////////////////////////////////////

Byte SmartPortHost::Access (int offset, bool isWrite, Byte value)
{
    uint64_t  nowNs = GetNowNs();



    CatchUp();

    return m_iwm.Access (offset, isWrite, value, nowNs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CatchUp
//
//  A reading IWM moves its cells after the drives catch up, as an access
//  moves them, so a turn-off delay ends without an access too.
//
////////////////////////////////////////////////////////////////////////////////

void SmartPortHost::CatchUp (CatchUpKind kind)
{
    uint64_t  nowNs = GetNowNs();



    if (m_iwm.IsWriting())
    {
        m_iwm.AdvanceTo (nowNs);
    }

    m_bus.CatchUp (nowNs, kind);

    m_iwm.AdvanceTo (nowNs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void SmartPortHost::Reset()
{
    m_iwm.Reset();
    m_epochNs     = m_lastNs;
    m_epochCycles = (m_hostCycles != nullptr) ? *m_hostCycles : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetHostClock
//
////////////////////////////////////////////////////////////////////////////////

void SmartPortHost::SetHostClock (const uint64_t * cycles, const uint64_t * stateCycles, uint32_t clockHz)
{
    m_hostCycles  = cycles;
    m_stateCycles = stateCycles;
    m_hostHz      = clockHz;
    m_epochNs     = m_lastNs;
    m_epochCycles = (cycles != nullptr) ? *cycles : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddDrive
//
////////////////////////////////////////////////////////////////////////////////

void SmartPortHost::AddDrive (std::unique_ptr<UniDisk35Drive> drive)
{
    m_bus.AddDevice (drive.get());
    m_drives.push_back (std::move (drive));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFloppy35EventSink
//
////////////////////////////////////////////////////////////////////////////////

void SmartPortHost::SetFloppy35EventSink (IFloppy35EventSink * sink)
{
    for (auto & drive : m_drives)
    {
        drive->SetFloppy35EventSink (sink);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetNowNs
//
//  The Apple's time in nanoseconds, never moving back across a reset of the
//  Apple's cycle count. A power cycle zeroes the count, so a count below the
//  last one seen starts a new epoch at the last time returned. Measured from
//  the old epoch, the time wouldn't advance until the count was back where
//  it was.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t SmartPortHost::GetNowNs()
{
    uint64_t  cycles = (m_hostCycles != nullptr) ? *m_hostCycles : 0;



    if (cycles < m_lastCycles || cycles < m_epochCycles)
    {
        m_epochCycles = cycles;
        m_epochNs     = m_lastNs;
    }

    m_lastCycles = cycles;
    m_lastNs     = std::max (m_lastNs, GetTimeAt (cycles));

    return m_lastNs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTimeAt
//
//  The Apple's time in nanoseconds at a cycle count in the current epoch.
//  With no clock rate, the time doesn't move past the epoch's start.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t SmartPortHost::GetTimeAt (uint64_t cycles) const
{
    uint64_t  elapsed = cycles - m_epochCycles;



    if (m_hostHz == 0)
    {
        return m_epochNs;
    }

    return m_epochNs + elapsed / m_hostHz * s_kNsPerSec + elapsed % m_hostHz * s_kNsPerSec / m_hostHz;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SmartPortHost::SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SmartPortHost::SaveState (StateWriter & writer) const
{
    HRESULT   hr       = S_OK;
    uint64_t  appleNow = (m_stateCycles != nullptr) ? *m_stateCycles : 0;



    writer.WriteUInt64 (m_epochCycles);
    writer.WriteUInt64 (m_epochNs);
    writer.WriteUInt64 (appleNow - m_lastCycles);
    writer.WriteUInt64 (m_lastNs - GetTimeAt (m_lastCycles));

    m_iwm.SaveState (writer, m_lastNs);
    m_bus.SaveState (writer);

    writer.WriteUInt32 (static_cast<uint32_t> (m_drives.size()));

    for (const std::unique_ptr<UniDisk35Drive> & drive : m_drives)
    {
        hr = drive->SaveState (writer, m_lastNs);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SmartPortHost::LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SmartPortHost::LoadState (StateReader & reader)
{
    HRESULT   hr          = S_OK;
    bool      isSameCount = false;
    uint32_t  driveCount  = 0;
    uint64_t  appleNow    = (m_stateCycles != nullptr) ? *m_stateCycles : 0;
    uint64_t  cyclesAgo   = 0;
    uint64_t  nsPastCount = 0;



    reader.ReadUInt64 (m_epochCycles);
    reader.ReadUInt64 (m_epochNs);
    reader.ReadUInt64 (cyclesAgo);
    reader.ReadUInt64 (nsPastCount);

    m_lastCycles = appleNow - cyclesAgo;
    m_lastNs     = GetTimeAt (m_lastCycles) + nsPastCount;

    hr = m_iwm.LoadState (reader, m_lastNs);
    CHR (hr);

    hr = m_bus.LoadState (reader);
    CHR (hr);

    reader.ReadUInt32 (driveCount);
    isSameCount = driveCount == m_drives.size();
    CBREx (isSameCount, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (std::unique_ptr<UniDisk35Drive> & drive : m_drives)
    {
        hr = drive->LoadState (reader, m_lastNs);
        CHR (hr);
    }

    hr = reader.GetResult();

Error:
    return hr;
}
