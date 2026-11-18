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
static constexpr int   s_kInternal     = 0;
static constexpr int   s_kExternalLine = 1;

static constexpr uint16_t  s_kFirstWithExternals = 2;
static constexpr uint32_t  s_kMaxEngineBytes      = 0x10000;





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
//  PowerOn
//
//  The Disk II logic's power cycle points both drives at their empty disks;
//  the caller binds the mounted disks again afterward. The external drives'
//  heads and rotation are cleared.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3DiskController::PowerOn (Prng & prng)
{
    m_diskII->PowerCycle (prng);

    for (External & external : m_externals)
    {
        external.quarterTrack = 0;
        external.engineState.clear();
    }

    m_connected          = 0;
    m_isInternalSelected = false;
    m_isExternal1        = false;
    m_isExternal2        = false;

    m_diskII->GetSignals().SetReportedDrive (s_kExternalLine, 1 + m_connected);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void Apple3DiskController::Reset()
{
    m_diskII->SoftReset();
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

    if (GetSelectedExternal() >= s_kDriveD2)
    {
        ConnectExternal (GetSelectedExternal() - s_kDriveD2);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetExternalDriveCount
//
////////////////////////////////////////////////////////////////////////////////

void Apple3DiskController::SetExternalDriveCount (int count)
{
    m_externalCount = std::clamp (count, 0, kMaxExternal);

    m_diskII->GetDrive (s_kExternalLine).SetExternalDisk ((m_connected < m_externalCount) ? m_externals[(size_t) m_connected].disk : nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDisk
//
//  An external drive's disk goes to the Disk II logic only while that drive
//  is connected.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3DiskController::SetDisk (int drive, DiskImage * disk)
{
    int  index = drive - 1;



    if (drive == s_kInternal)
    {
        m_diskII->GetDrive (s_kInternal).SetExternalDisk (disk);
    }
    else if (index >= 0 && index < kMaxExternal)
    {
        m_externals[(size_t) index].disk = disk;

        if (index == m_connected && index < m_externalCount)
        {
            m_diskII->GetDrive (s_kExternalLine).SetExternalDisk (disk);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConnectExternal
//
//  Stores the connected drive's head and rotation, and puts the chosen
//  drive's back. A drive never connected starts at track 0 with the
//  rotation reset. The motor stays as the lines have it.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3DiskController::ConnectExternal (int index)
{
    Disk2Drive   & drive  = m_diskII->GetDrive (s_kExternalLine);
    External     & before = m_externals[(size_t) m_connected];
    External     & after  = m_externals[(size_t) index];
    StateWriter    writer;



    if (index == m_connected)
    {
        return;
    }

    before.quarterTrack = drive.GetQuarterTrack();
    before.readNibbles  = drive.GetEngine().GetReadNibbles();
    before.writeNibbles = drive.GetEngine().GetWriteNibbles();
    drive.GetEngine().SaveState (writer);
    before.engineState = writer.GetBytes();

    drive.SetExternalDisk (index < m_externalCount ? after.disk : nullptr);
    drive.SetQuarterTrack (after.quarterTrack);

    if (after.engineState.empty())
    {
        drive.GetEngine().Reset();
        drive.GetEngine().SetDiskImage (drive.GetDisk());
    }
    else
    {
        StateReader  reader (after.engineState);
        HRESULT      hr = drive.GetEngine().LoadState (reader);



        IGNORE_RETURN_VALUE (hr, S_OK);
        drive.GetEngine().SetDiskImage (drive.GetDisk());
    }

    drive.GetEngine().SetMotorOn (m_diskII->GetLines().IsMotorOn());

    m_connected = index;

    // The debugger's drive is the Apple /// drive: .D2 to .D4 are drives 1 to 3.
    m_diskII->GetSignals().SetReportedDrive (s_kExternalLine, 1 + index);

    if (m_diskII->GetLines().GetActiveDrive() == s_kExternalLine)
    {
        m_diskII->GetSignals().OnDriveSelect (s_kExternalLine);
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
//  GetDisk
//
////////////////////////////////////////////////////////////////////////////////

DiskImage * Apple3DiskController::GetDisk (int drive) const
{
    DiskImage  * disk = nullptr;



    if (drive == 0)
    {
        disk = m_diskII->GetDrive (0).GetDisk();
    }
    else if (drive > 0 && drive <= m_externalCount)
    {
        disk = m_externals[(size_t) (drive - 1)].disk;
    }

    return disk;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDriveActivity
//
////////////////////////////////////////////////////////////////////////////////

Apple3DiskController::DriveActivity Apple3DiskController::GetDriveActivity (int drive) const
{
    DriveActivity  activity;
    int            external = drive - 1;



    if (drive == 0 || external == m_connected)
    {
        const Disk2NibbleEngine  & engine = m_diskII->GetDrive ((drive == 0) ? 0 : s_kExternalLine).GetEngine();

        activity.isMotorOn    = engine.IsMotorOn();
        activity.quarterTrack = engine.GetCurrentTrack();
        activity.readNibbles  = engine.GetReadNibbles();
        activity.writeNibbles = engine.GetWriteNibbles();
    }
    else if (external >= 0 && external < kMaxExternal)
    {
        activity.quarterTrack = m_externals[(size_t) external].quarterTrack;
        activity.readNibbles  = m_externals[(size_t) external].readNibbles;
        activity.writeNibbles = m_externals[(size_t) external].writeNibbles;
    }

    return activity;
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
    writer.WriteByte   ((Byte) m_connected);

    for (const External & external : m_externals)
    {
        writer.WriteUInt32 ((uint32_t) external.quarterTrack);
        writer.WriteUInt32 ((uint32_t) external.engineState.size());
        writer.WriteBytes  (external.engineState.data(), external.engineState.size());
    }

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
    HRESULT   hr           = S_OK;
    uint16_t  version      = 0;
    Byte      connected    = 0;
    uint32_t  quarterTrack = 0;
    uint32_t  engineBytes  = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadBool   (m_isInternalSelected);
    reader.ReadBool   (m_isExternal1);
    reader.ReadBool   (m_isExternal2);
    reader.ReadUInt64 (m_diskCycles);
    reader.ReadUInt64 (m_lastMasterClocks);

    if (version >= s_kFirstWithExternals)
    {
        reader.ReadByte (connected);
        CBREx (connected < kMaxExternal, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
        m_connected = connected;
        m_diskII->GetSignals().SetReportedDrive (s_kExternalLine, 1 + m_connected);

        for (External & external : m_externals)
        {
            reader.ReadUInt32 (quarterTrack);
            reader.ReadUInt32 (engineBytes);
            CBREx (engineBytes <= s_kMaxEngineBytes, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

            external.quarterTrack = (int) quarterTrack;
            external.engineState.resize (engineBytes);
            reader.ReadBytes (external.engineState.data(), engineBytes);
        }
    }

    hr = m_diskII->LoadState (reader);
    CHR (hr);

    hr = reader.EndSection();
    CHR (hr);

Error:
    return hr;
}
