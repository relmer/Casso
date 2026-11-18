#pragma once

#include "Debugger/BeamBreakMonitor.h"
#include "Debugger/Reply.h"

class BasicStopMonitor;
class BreakpointTable;
class DiskBreakpointMonitor;
class IDebugExpressionContext;
class IDebugTarget;
class WatchpointTable;
struct DebugHookFilter;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor
//
//  A debug session's stop conditions, tested at each instruction boundary:
//  the breakpoints, the disk and Applesoft monitors, the beam breaks, the
//  return a Monitor `G` pushed, the before-mode watchpoints and read before
//  written. A stop's cause is stored where it is found and added to the
//  stop event when the run reports the stop.
//
////////////////////////////////////////////////////////////////////////////////

class DebugStopMonitor
{
public:
    DebugStopMonitor (IDebugTarget             & target,
                      IDebugExpressionContext  & context,
                      BreakpointTable          & breakpoints,
                      WatchpointTable          & watchpoints,
                      DiskBreakpointMonitor    & diskMonitor,
                      BasicStopMonitor         & basicMonitor);

    bool   ShouldStopBefore         (Word pc);
    bool   ShouldStopAtRunStart     (Word pc);
    bool   ShouldStopAfterInterrupt (Word pc);
    bool   HasPendingStop           () const;
    void   NoteCpuOwnReads          (Word pc);
    void   GetCpuOwnReads           (Word pc, Word & length, std::optional<Word> & storeTarget) const;

    BeamBreakMonitor        & GetBeamBreaks     ()       { return m_beamBreaks; }
    const BeamBreakMonitor  & GetBeamBreaks     () const { return m_beamBreaks; }

    void   SetUnwrittenBreak        (bool isOn);
    bool   HasUnwrittenBreak        () const { return m_isUnwrittenBreak; }

    //  The stop at the return a Monitor `G` pushed.
    void                 ArmMonitorReturn     (Word address) { m_monitorReturn = address; }
    void                 DisarmMonitorReturn  ()             { m_monitorReturn.reset(); }

    //  Any enabled stop condition exists, so the hook is installed.
    bool   HasStopConditions        () const;

    //  The instructions the hook is called on, given whether every one is
    //  called on for another reason.
    void   BuildHookFilter          (DebugHookFilter & filter, bool isEvery);

    //  A stop's cause, reason and reports, added to the event the run
    //  reports; then the causes are cleared for the next run.
    void   ReportStop               (StopEvent & event);
    void   OnStopped                ();
    void   ClearTemporary           (const StopEvent & stop);

    //  A run is starting: a hit raised by IN or OUT while the machine was
    //  paused is not part of any run, and is dropped.
    void   OnRunStarting            ();

    void   OnMachineChanged         (bool isKeepingStops);
    void   OnReset                  ();
    void   ClearBreaks              ();

private:
    //  A read before written's stop is reported under this id, which no
    //  breakpoint or watchpoint has.
    static constexpr int  kUnwrittenHitId = -1;

    std::optional<WatchHit>  GetUnwrittenHit          () const;
    bool                     TryMatchBeforeWatchpoint (Word pc);
    void                     AttachHit                (StopEvent & event);
    void                     AttachDiskReport         (StopEvent & event);
    void                     AttachDiskCallReport     (StopEvent & event);
    void                     AttachBasicReport        (StopEvent & event);
    void                     AttachCondition          (StopEvent & event) const;

    IDebugTarget                & m_target;
    IDebugExpressionContext     & m_context;
    BreakpointTable             & m_breakpoints;
    WatchpointTable             & m_watchpoints;
    DiskBreakpointMonitor       & m_diskMonitor;
    BasicStopMonitor            & m_basicMonitor;
    BeamBreakMonitor              m_beamBreaks;
    std::optional<int>            m_lastBreakpointId;
    std::optional<WatchHit>       m_beforeHit;
    std::optional<Word>           m_monitorReturn;
    bool                          m_monitorReturned  = false;
    bool                          m_isDiskStop       = false;     // ShouldStopBefore stopped for a disk breakpoint
    bool                          m_isBasicStop      = false;     // and for an Applesoft breakpoint, step or error
    bool                          m_isUnwrittenBreak = false;     // BRKUNINIT is on
};
