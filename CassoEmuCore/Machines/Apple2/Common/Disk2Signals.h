#pragma once

#include "Pch.h"

#include "Core/ObserverSlots.h"
#include "Machines/Apple2/Common/Disk2Drive.h"

class IDriveAudioSink;
class IDisk2EventSink;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Signals
//
//  What a Disk II controller reports outward, from either front end: the
//  drive sound, the debugger's event sinks, the motor-off flush and the idle
//  callback. The sinks and callbacks are the caller's; a null one is skipped.
//
////////////////////////////////////////////////////////////////////////////////

class Disk2Signals
{
public:
    // How often the idle callback may fire, in CPU cycles. One emulated frame
    // at 1.0205 MHz: 60 chances a second to notice a change, which is far
    // more prompt than a build loop requires and far less work than a
    // per-instruction dispatch.
    static constexpr uint32_t  kIdleCallbackCycles = 17030;

    // One sink covers both drives; per-drive routing happens at the mixer.
    void               SetAudioSink (IDriveAudioSink * sink) { m_audioSink = sink; }
    IDriveAudioSink  * GetAudioSink () const                 { return m_audioSink; }

    void               SetEventSink (ObserverSlot slot, IDisk2EventSink * sink) noexcept { m_eventSinks.Set (slot, sink); }
    IDisk2EventSink  * GetEventSink (ObserverSlot slot) const noexcept                   { return m_eventSinks.Get (slot); }

    // Fired on the CPU thread when the motor stops, a debounced moment to
    // write dirty images back.
    void  SetMotorOffFlushCallback (std::function<void ()> cb) { m_motorOffFlushCallback = std::move (cb); }

    // Fired on the CPU thread at most once per emulated frame while no disk
    // operation is in flight (PumpIdle).
    void  SetIdleCallback (std::function<void ()> cb) { m_idleCallback = std::move (cb); }

    //  The drive number the select and write-mode events report for each of
    //  the logic's two drive lines: the line itself, or on the Apple ///
    //  the external drive connected to the second line.
    void  SetReportedDrive  (int line, int drive) { if (line >= 0 && line < (int) m_reportedDrive.size()) { m_reportedDrive[(size_t) line] = drive; } }
    int   GetReportedDrive  (int line) const      { return (line >= 0 && line < (int) m_reportedDrive.size()) ? m_reportedDrive[(size_t) line] : line; }

    void  OnMotorEngaged    ();
    void  OnMotorDisengaged ();
    void  OnMotorCommand    (bool isOn);
    void  OnHeadMoved       (const Disk2HeadMove & move);
    void  OnDriveSelect     (int drive);
    void  OnWriteMode       (int drive, bool isWriting);
    void  OnWriteProtected  ();
    void  OnDiskInserted    (int drive);
    void  OnDiskEjected     (int drive);
    void  PumpIdle          (uint32_t cpuCycles, bool isBusy);

    //  The idle callback's window, for the controller's state.
    uint32_t  GetCyclesSinceIdle () const { return m_cyclesSinceIdleCallback; }
    bool      IsBusySinceIdle    () const { return m_busySinceIdleCallback; }
    void      SetIdleWindow      (uint32_t cycles, bool isBusy);

private:
    IDriveAudioSink                 * m_audioSink               = nullptr;
    std::array<int, 2>                m_reportedDrive           = { 0, 1 };
    ObserverSlots<IDisk2EventSink>    m_eventSinks;
    std::function<void ()>            m_motorOffFlushCallback;
    std::function<void ()>            m_idleCallback;

    // Cycles since the idle callback last fired, which rate-limits it, and
    // whether anything was in flight at any point since.
    uint32_t                          m_cyclesSinceIdleCallback = 0;
    bool                              m_busySinceIdleCallback   = false;
};
