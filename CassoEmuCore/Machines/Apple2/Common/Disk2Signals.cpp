#include "Pch.h"

#include "Machines/Apple2/Common/Disk2Signals.h"
#include "Audio/IDriveAudioSink.h"
#include "Machines/Apple2/Common/IDisk2EventSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Signals::OnMotorEngaged
//
//  The motor started from rest. A motor-on inside the spin-down is not a
//  start, so the motor sound does not restart.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Signals::OnMotorEngaged()
{
    if (m_audioSink != nullptr)
    {
        m_audioSink->OnMotorEngaged();
    }

    m_eventSinks.ForEach ([] (IDisk2EventSink & sink) { sink.OnMotorEngaged(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Signals::OnMotorDisengaged
//
//  The motor stopped. The flush callback fires after the sinks, so a
//  debugger sink has the stop first.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Signals::OnMotorDisengaged()
{
    if (m_audioSink != nullptr)
    {
        m_audioSink->OnMotorDisengaged();
    }

    m_eventSinks.ForEach ([] (IDisk2EventSink & sink) { sink.OnMotorDisengaged(); });

    if (m_motorOffFlushCallback)
    {
        m_motorOffFlushCallback();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Signals::OnMotorCommand
//
//  Every access to a motor switch, including one that changes nothing.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Signals::OnMotorCommand (bool isOn)
{
    if (isOn)
    {
        m_eventSinks.ForEach ([] (IDisk2EventSink & sink) { sink.OnMotorCommandOn(); });
    }
    else
    {
        m_eventSinks.ForEach ([] (IDisk2EventSink & sink) { sink.OnMotorCommandOff(); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Signals::OnHeadMoved
//
//  A bump is a pull past a travel stop: the stepper is energized and the
//  head cannot move, which makes the thunk. A move is a step or a bump,
//  never both. The step sound is for a move that ends on a half-track
//  position, so a DOS seek's two moves sound as the one step they make; the
//  event sinks get every quarter-track move.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Signals::OnHeadMoved (const Disk2HeadMove & move)
{
    if (m_audioSink != nullptr)
    {
        if (move.isBumped)
        {
            m_audioSink->OnHeadBump();
        }
        else if (move.to % Disk2Drive::kQuarterTracksPerHalfTrack == 0)
        {
            m_audioSink->OnHeadStep (move.to);
        }
    }

    if (move.isBumped)
    {
        m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnHeadBump (move.to); });
    }
    else
    {
        m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnHeadStep (move.from, move.to); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Signals::OnDriveSelect
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Signals::OnDriveSelect (int drive)
{
    int  reported = GetReportedDrive (drive);



    m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnDriveSelect (reported); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Signals::OnWriteMode
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Signals::OnWriteMode (int drive, bool isWriting)
{
    int  reported = GetReportedDrive (drive);



    m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnWriteMode (reported, isWriting); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Signals::OnWriteProtected
//
//  Writing turned on with a write-protected disk in the drive: the nibbles
//  go nowhere.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Signals::OnWriteProtected()
{
    m_eventSinks.ForEach ([] (IDisk2EventSink & sink) { sink.OnWriteProtected(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Signals::OnDiskInserted
//
//  Also fired by the shell for a disk it attaches through the disk store,
//  so every insert goes to the debugger. An out-of-range drive is ignored.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Signals::OnDiskInserted (int drive)
{
    if (drive < 0 || drive >= Disk2Drive::kPerController)
    {
        return;
    }

    m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnDiskInserted (drive); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Signals::OnDiskEjected
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Signals::OnDiskEjected (int drive)
{
    if (drive < 0 || drive >= Disk2Drive::kPerController)
    {
        return;
    }

    m_eventSinks.ForEach ([&] (IDisk2EventSink & sink) { sink.OnDiskEjected (drive); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Signals::PumpIdle
//
//  Calls the idle callback at a quiet moment on the CPU thread.
//
//  THE SPINDOWN HOOK CANNOT SERVE THIS. It fires on a motor-on to motor-off
//  transition, so it never fires for a guest that never touched the drive,
//  such as a machine at the BASIC prompt at the end of a build loop.
//
//  "NO OPERATION IN FLIGHT" IS SPIN-UP AND NOTHING ELSE (isBusy). The
//  controller has no notion of a transfer in progress; what it has is a
//  window during which reads are suppressed because the drive has not come up
//  to speed, and that window is precisely the moment when swapping the bytes
//  under the guest would be seen.
//
//  RATE-LIMITED, because this is pumped per instruction and the condition is
//  nearly always true. The busy flag is accumulated across the whole window
//  rather than sampled at its end: spin-up lasts a few hundred cycles and the
//  window is a frame, so a sample at the boundary would nearly always find
//  the drive quiet again.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Signals::PumpIdle (uint32_t cpuCycles, bool isBusy)
{
    bool  isQuiet = false;



    if (isBusy)
    {
        m_busySinceIdleCallback = true;
    }

    m_cyclesSinceIdleCallback += cpuCycles;

    if (m_cyclesSinceIdleCallback < kIdleCallbackCycles)
    {
        return;
    }

    isQuiet = !m_busySinceIdleCallback;

    m_cyclesSinceIdleCallback = 0;
    m_busySinceIdleCallback   = false;

    if (isQuiet && m_idleCallback)
    {
        m_idleCallback();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Signals::SetIdleWindow
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Signals::SetIdleWindow (uint32_t cycles, bool isBusy)
{
    m_cyclesSinceIdleCallback = cycles;
    m_busySinceIdleCallback   = isBusy;
}
