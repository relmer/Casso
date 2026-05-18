#include "Pch.h"

#include "DiskIIShellSinkBuffer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PublishToRing
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIShellSinkBuffer::PublishToRing (const DiskIIEvent & e) noexcept
{
    DiskIIEvent  stamped = e;

    if (m_cycleCounter != nullptr)
    {
        stamped.cycle = *m_cycleCounter;
    }

    // Best-effort: if the ring is full the oldest pre-open event
    // silently drops. There is no dedicated overflow marker because
    // the dialog wasn't open to render it; consumers that care about
    // pre-open completeness should open the dialog earlier. Once a
    // live sink is attached we bypass the ring entirely so this push
    // only runs during the pre-open window.
    (void) m_ring.TryPush (stamped);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushControllerEvent
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIShellSinkBuffer::PushControllerEvent (DiskIIEventType type) noexcept
{
    DiskIIEvent  e = {};

    e.category = EventCategory::Controller;
    e.type     = type;
    e.drive    = static_cast<int8_t> (m_currentDrive);

    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushHeadStepEvent
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIShellSinkBuffer::PushHeadStepEvent (int prevQt, int newQt) noexcept
{
    DiskIIEvent  e = {};

    e.category            = EventCategory::Controller;
    e.type                = DiskIIEventType::HeadStep;
    e.drive               = static_cast<int8_t> (m_currentDrive);
    e.payload.step.prevQt = prevQt;
    e.payload.step.newQt  = newQt;

    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushHeadBumpEvent
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIShellSinkBuffer::PushHeadBumpEvent (int atQt) noexcept
{
    DiskIIEvent  e = {};

    e.category          = EventCategory::Controller;
    e.type              = DiskIIEventType::HeadBump;
    e.drive             = static_cast<int8_t> (m_currentDrive);
    e.payload.bump.atQt = atQt;

    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushAddrMarkEvent
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIShellSinkBuffer::PushAddrMarkEvent (int track, int sector, int volume) noexcept
{
    DiskIIEvent  e = {};

    e.category                = EventCategory::Controller;
    e.type                    = DiskIIEventType::AddrMark;
    e.drive                   = static_cast<int8_t> (m_currentDrive);
    e.payload.addrMark.track  = track;
    e.payload.addrMark.sector = sector;
    e.payload.addrMark.volume = volume;

    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushDataMarkEvent
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIShellSinkBuffer::PushDataMarkEvent (DiskIIEventType type, int track, int sector, int volume, int byteCount) noexcept
{
    DiskIIEvent  e = {};

    e.category                   = EventCategory::Controller;
    e.type                       = type;
    e.drive                      = static_cast<int8_t> (m_currentDrive);
    e.payload.dataMark.track     = track;
    e.payload.dataMark.sector    = sector;
    e.payload.dataMark.volume    = volume;
    e.payload.dataMark.byteCount = byteCount;

    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushDriveEvent
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIShellSinkBuffer::PushDriveEvent (DiskIIEventType type, int drive) noexcept
{
    DiskIIEvent  e = {};

    e.category            = EventCategory::Controller;
    e.type                = type;
    e.drive               = static_cast<int8_t> (drive);
    e.payload.drive.drive = drive;

    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushAudioEvent
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIShellSinkBuffer::PushAudioEvent (
    DiskIIEventType  type,
    SoundKind        kind,
    int              drive,
    SilentReason     reason) noexcept
{
    DiskIIEvent  e = {};

    e.category             = EventCategory::Audio;
    e.type                 = type;
    e.drive                = static_cast<int8_t> (drive);
    e.payload.audio.kind   = kind;
    e.payload.audio.reason = reason;
    e.payload.audio.drive  = drive;

    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrainInto
//
//  Drains everything currently in the ring into dest in FIFO order,
//  then trims dest from the front if its size exceeds maxRows.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIShellSinkBuffer::DrainInto (std::deque<DiskIIEvent> & dest, size_t maxRows) noexcept
{
    constexpr uint32_t  kBatch                = 256;
    DiskIIEvent         batch[kBatch]         = {};
    uint32_t            drained               = 0;
    uint32_t            i                     = 0;

    do
    {
        drained = m_ring.Drain (batch, kBatch);

        for (i = 0; i < drained; i++)
        {
            dest.push_back (batch[i]);
        }
    }
    while (drained == kBatch);

    while (dest.size () > maxRows)
    {
        dest.pop_front ();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IDiskIIEventSink overrides
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIShellSinkBuffer::OnMotorCommandOn ()
{
    if (m_liveSink != nullptr) { m_liveSink->OnMotorCommandOn (); return; }
    PushControllerEvent (DiskIIEventType::MotorCommandOn);
}

void DiskIIShellSinkBuffer::OnMotorEngaged ()
{
    if (m_liveSink != nullptr) { m_liveSink->OnMotorEngaged (); return; }
    PushControllerEvent (DiskIIEventType::MotorEngaged);
}

void DiskIIShellSinkBuffer::OnMotorCommandOff ()
{
    if (m_liveSink != nullptr) { m_liveSink->OnMotorCommandOff (); return; }
    PushControllerEvent (DiskIIEventType::MotorCommandOff);
}

void DiskIIShellSinkBuffer::OnMotorDisengaged ()
{
    if (m_liveSink != nullptr) { m_liveSink->OnMotorDisengaged (); return; }
    PushControllerEvent (DiskIIEventType::MotorDisengaged);
}

void DiskIIShellSinkBuffer::OnHeadStep (int prevQt, int newQt)
{
    if (m_liveSink != nullptr) { m_liveSink->OnHeadStep (prevQt, newQt); return; }
    PushHeadStepEvent (prevQt, newQt);
}

void DiskIIShellSinkBuffer::OnHeadBump (int atQt)
{
    if (m_liveSink != nullptr) { m_liveSink->OnHeadBump (atQt); return; }
    PushHeadBumpEvent (atQt);
}

void DiskIIShellSinkBuffer::OnAddressMark (int track, int sector, int volume)
{
    if (m_liveSink != nullptr) { m_liveSink->OnAddressMark (track, sector, volume); return; }
    PushAddrMarkEvent (track, sector, volume);
}

void DiskIIShellSinkBuffer::OnDataMarkRead (int track, int sector, int volume, int byteCount)
{
    if (m_liveSink != nullptr) { m_liveSink->OnDataMarkRead (track, sector, volume, byteCount); return; }
    PushDataMarkEvent (DiskIIEventType::DataRead, track, sector, volume, byteCount);
}

void DiskIIShellSinkBuffer::OnDataMarkWrite (int track, int sector, int volume, int byteCount)
{
    if (m_liveSink != nullptr) { m_liveSink->OnDataMarkWrite (track, sector, volume, byteCount); return; }
    PushDataMarkEvent (DiskIIEventType::DataWrite, track, sector, volume, byteCount);
}

void DiskIIShellSinkBuffer::OnDriveSelect (int drive)
{
    m_currentDrive = drive;

    if (m_liveSink != nullptr) { m_liveSink->OnDriveSelect (drive); return; }
    PushDriveEvent (DiskIIEventType::DriveSelect, drive);
}

void DiskIIShellSinkBuffer::OnDiskInserted (int drive)
{
    if (m_liveSink != nullptr) { m_liveSink->OnDiskInserted (drive); return; }
    PushDriveEvent (DiskIIEventType::DiskInserted, drive);
}

void DiskIIShellSinkBuffer::OnDiskEjected (int drive)
{
    if (m_liveSink != nullptr) { m_liveSink->OnDiskEjected (drive); return; }
    PushDriveEvent (DiskIIEventType::DiskEjected, drive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IDriveAudioEventSink overrides
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIShellSinkBuffer::OnAudioStarted (SoundKind kind, int drive)
{
    if (m_liveAudioSink != nullptr) { m_liveAudioSink->OnAudioStarted (kind, drive); return; }
    PushAudioEvent (DiskIIEventType::AudioStarted, kind, drive, SilentReason::DriveAudioDisabled);
}

void DiskIIShellSinkBuffer::OnAudioRestarted (SoundKind kind, int drive)
{
    if (m_liveAudioSink != nullptr) { m_liveAudioSink->OnAudioRestarted (kind, drive); return; }
    PushAudioEvent (DiskIIEventType::AudioRestarted, kind, drive, SilentReason::DriveAudioDisabled);
}

void DiskIIShellSinkBuffer::OnAudioContinued (SoundKind kind, int drive)
{
    if (m_liveAudioSink != nullptr) { m_liveAudioSink->OnAudioContinued (kind, drive); return; }
    PushAudioEvent (DiskIIEventType::AudioContinued, kind, drive, SilentReason::DriveAudioDisabled);
}

void DiskIIShellSinkBuffer::OnAudioSilent (SoundKind kind, int drive, SilentReason reason)
{
    if (m_liveAudioSink != nullptr) { m_liveAudioSink->OnAudioSilent (kind, drive, reason); return; }
    PushAudioEvent (DiskIIEventType::AudioSilent, kind, drive, reason);
}

void DiskIIShellSinkBuffer::OnAudioLoopStarted (SoundKind kind, int drive)
{
    if (m_liveAudioSink != nullptr) { m_liveAudioSink->OnAudioLoopStarted (kind, drive); return; }
    PushAudioEvent (DiskIIEventType::AudioLoopStarted, kind, drive, SilentReason::DriveAudioDisabled);
}

void DiskIIShellSinkBuffer::OnAudioLoopStopped (SoundKind kind, int drive)
{
    if (m_liveAudioSink != nullptr) { m_liveAudioSink->OnAudioLoopStopped (kind, drive); return; }
    PushAudioEvent (DiskIIEventType::AudioLoopStopped, kind, drive, SilentReason::DriveAudioDisabled);
}
