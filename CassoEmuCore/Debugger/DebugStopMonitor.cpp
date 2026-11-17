#include "Pch.h"

#include "Microcode.h"
#include "OpcodeTable.h"
#include "Debugger/Basic/BasicStopMonitor.h"
#include "Debugger/BreakpointTable.h"
#include "Debugger/DebugHook.h"
#include "Debugger/DebugStopMonitor.h"
#include "Debugger/DiskBreakpointMonitor.h"
#include "Debugger/DiskCallVector.h"
#include "Debugger/EffectiveAddress.h"
#include "Debugger/IDebugExpressionContext.h"
#include "Debugger/IDebugTarget.h"
#include "Debugger/WatchpointTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::DebugStopMonitor
//
////////////////////////////////////////////////////////////////////////////////

DebugStopMonitor::DebugStopMonitor (
    IDebugTarget             & target,
    IDebugExpressionContext  & context,
    BreakpointTable          & breakpoints,
    WatchpointTable          & watchpoints,
    DiskBreakpointMonitor    & diskMonitor,
    BasicStopMonitor         & basicMonitor) :
    m_target       (target),
    m_context      (context),
    m_breakpoints  (breakpoints),
    m_watchpoints  (watchpoints),
    m_diskMonitor  (diskMonitor),
    m_basicMonitor (basicMonitor),
    m_beamBreaks   (target)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::ShouldStopBefore
//
//  Records the instruction address for watchpoint hits during the
//  instruction, then checks the breakpoints, then the before-mode
//  watchpoints against what the instruction would touch.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugStopMonitor::ShouldStopBefore (Word pc)
{
    Byte  opcode     = 0;
    int   hitId      = -1;
    bool  isHit      = false;
    bool  isDiskHit  = false;
    bool  isBasicHit = false;
    bool  isPeeked   = false;



    m_watchpoints.SetAccessPc (pc);
    isPeeked = m_target.TryPeek (pc, opcode);

    //  A disk event the last instruction made stops here, after it, as a
    //  watchpoint hit does; with an address breakpoint here as well, the stop
    //  has the breakpoint's reason and the disk report beside it. Applesoft's
    //  boundary is polled here too, so its variable entries take every
    //  boundary's values.
    isDiskHit  = m_diskMonitor.Poll (m_context);
    isBasicHit = m_basicMonitor.Poll (pc, m_context);
    isHit      = m_breakpoints.TryMatchBeforeInstruction (pc, isPeeked ? std::optional<Byte> (opcode) : std::nullopt, m_context, hitId);

    if (isHit)
    {
        m_lastBreakpointId = hitId;
        return true;
    }

    if (isDiskHit)
    {
        m_isDiskStop = true;
        return true;
    }

    if (isBasicHit)
    {
        m_isBasicStop = true;
        return true;
    }

    if (m_beamBreaks.Poll())
    {
        return true;
    }

    //  A Monitor `G` returning through its pushed address. It takes no id and
    //  is absent from BPL, because it is not the reader's breakpoint: it is
    //  how the Monitor gets control back from a program that ends in RTS.
    if (m_monitorReturn.has_value() && pc == *m_monitorReturn)
    {
        m_monitorReturn.reset();
        m_monitorReturned = true;
        return true;
    }

    return m_watchpoints.HasEnabledBefore() && TryMatchBeforeWatchpoint (pc);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::ShouldStopAtRunStart
//
//  The instruction a run begins on stops only for a before-mode watchpoint,
//  and not when that watchpoint's own stop left the PC here.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugStopMonitor::ShouldStopAtRunStart (Word pc)
{
    m_watchpoints.SetAccessPc (pc);

    if (m_watchpoints.IsSuppressingAfterStopAt (pc))
    {
        return false;
    }

    return TryMatchBeforeWatchpoint (pc);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::ShouldStopAfterInterrupt
//
//  pc is the first instruction of the handler of an interrupt just taken.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugStopMonitor::ShouldStopAfterInterrupt (Word pc)
{
    int  hitId = -1;



    m_watchpoints.SetAccessPc (pc);

    if (!m_breakpoints.TryMatchInterrupt (m_context, hitId))
    {
        return false;
    }

    m_lastBreakpointId = hitId;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::HasPendingStop
//
////////////////////////////////////////////////////////////////////////////////

bool DebugStopMonitor::HasPendingStop() const
{
    HeatUnwrittenRead  read;



    return m_watchpoints.HasPendingStop() || (m_isUnwrittenBreak && m_target.TryGetUnwrittenStop (read));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::NoteCpuOwnReads
//
//  The reads the instruction at pc makes that are not the program reading
//  memory: the fetch of its own bytes, and the read an indexed store makes of
//  its target before writing it. A read watchpoint passes over them.
//
////////////////////////////////////////////////////////////////////////////////

void DebugStopMonitor::NoteCpuOwnReads (Word pc)
{
    Word                 length      = 0;
    std::optional<Word>  storeTarget;



    GetCpuOwnReads (pc, length, storeTarget);
    m_watchpoints.SetCpuOwnReads (pc, length, storeTarget);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::GetCpuOwnReads
//
//  The instruction at pc's own bytes, as a length, and the target an indexed
//  store reads before writing it; a length of zero where the instruction
//  cannot be read.
//
////////////////////////////////////////////////////////////////////////////////

void DebugStopMonitor::GetCpuOwnReads (
    Word                  pc,
    Word                & length,
    std::optional<Word> & storeTarget) const
{
    const Microcode      * set         = m_target.GetInstructionSet();
    const Microcode      * microcode   = nullptr;
    Byte                   opcode      = 0;
    AccessPrediction       prediction;
    HRESULT                hr          = S_OK;



    length = 0;
    storeTarget.reset();

    if (set == nullptr || !m_target.TryPeek (pc, opcode))
    {
        return;
    }

    microcode = &set[opcode];
    length    = microcode->isLegal ? (Word) (1 + OpcodeTable::GetOperandSize (microcode->globalAddressingMode)) : (Word) 1;

    if (microcode->isLegal && EffectiveAddress::ClassifyOperand (*microcode) == PredictedAccess::Write)
    {
        hr = EffectiveAddress::Predict (set, pc, m_target.GetRegisters(), m_context, prediction);

        if (SUCCEEDED (hr) && !prediction.touches.empty() && prediction.touches.back().access == PredictedAccess::Write)
        {
            storeTarget = prediction.touches.back().address;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::SetUnwrittenBreak
//
//  The heat map is what sees such a read, so the target turns it on with the
//  break armed; a read held from before the break went off is dropped.
//
////////////////////////////////////////////////////////////////////////////////

void DebugStopMonitor::SetUnwrittenBreak (bool isOn)
{
    m_isUnwrittenBreak = isOn;
    m_target.SetUnwrittenBreak (isOn);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::HasStopConditions
//
////////////////////////////////////////////////////////////////////////////////

bool DebugStopMonitor::HasStopConditions() const
{
    return m_breakpoints.HasEnabledStopCondition() || m_watchpoints.HasEnabled() || m_beamBreaks.IsArmed() || m_monitorReturn.has_value() ||
           m_isUnwrittenBreak;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::BuildHookFilter
//
//  Every instruction while a watchpoint, a value breakpoint, a video break
//  or a breakpoint on anything but an address or an opcode is armed: those
//  are tested before or during any instruction, and a watchpoint hit reports
//  the address of the instruction that made it. Otherwise only the pages
//  with an address breakpoint or the Monitor's return, and the opcodes a
//  breakpoint specifies.
//
////////////////////////////////////////////////////////////////////////////////

void DebugStopMonitor::BuildHookFilter (DebugHookFilter & filter, bool isEvery)
{
    isEvery = isEvery || m_watchpoints.HasEnabled() || m_beamBreaks.IsArmed() || m_isUnwrittenBreak;

    filter       = DebugHookFilter();
    filter.pages = {};

    for (const Breakpoint & entry : m_breakpoints.GetAll())
    {
        if (!entry.enabled)
        {
            continue;
        }

        switch (entry.kind)
        {
        case BreakpointKind::Address:
            for (int page = entry.first >> 8; page <= (entry.last >> 8); ++page)
            {
                filter.pages[page] = true;
            }

            break;

        case BreakpointKind::Opcode:
        case BreakpointKind::Brk:
            filter.opcodes[entry.opcode] = true;
            filter.opcodesStop           = true;
            break;

        //  A disk event is raised during an instruction and reported at the
        //  next boundary, as a watchpoint hit is; an RWTS or driver call stops
        //  before its routine, whose page the filter marks once it is resolved.
        case BreakpointKind::Disk:
            isEvery = isEvery || !entry.IsDiskCall();
            filter.pages[entry.resolved.value_or (0) >> 8] = filter.pages[entry.resolved.value_or (0) >> 8] || entry.resolved.has_value();
            break;

        //  Applesoft's entries are tested where the interpreter passes, on
        //  the pages the monitor marks below.
        case BreakpointKind::BasicLine:
        case BreakpointKind::BasicVariable:
        case BreakpointKind::BasicError:
            break;

        default:
            isEvery = true;
            break;
        }
    }

    if (m_monitorReturn.has_value())
    {
        filter.pages[*m_monitorReturn >> 8] = true;
    }

    m_basicMonitor.ForgetDisabledValues();
    m_basicMonitor.MarkPages (filter.pages);

    //  With only opcode breakpoints armed, the host asks about an instruction
    //  only when its opcode is one of theirs. An address breakpoint beside
    //  them is on no such list, so every instruction is asked about instead.
    if (filter.opcodesStop && std::ranges::any_of (filter.pages, [] (bool marked) { return marked; }))
    {
        filter.opcodesStop = false;
        isEvery            = true;
    }

    filter.everyInstruction = isEvery;

    if (isEvery || filter.opcodesStop)
    {
        filter.pages = DebugHookFilter::s_kAllMarked;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::TryMatchBeforeWatchpoint
//
//  Prediction runs only while a before-mode watchpoint is enabled. A hit
//  stops before the instruction and arms the one-stop rule, so the accesses
//  that instruction makes when the run resumes are not reported again.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugStopMonitor::TryMatchBeforeWatchpoint (Word pc)
{
    AccessPrediction  prediction;
    WatchHit          hit;
    HRESULT           hr = S_OK;



    if (!m_watchpoints.HasEnabledBefore())
    {
        return false;
    }

    hr = EffectiveAddress::Predict (m_target.GetInstructionSet(), pc, m_target.GetRegisters(), m_context, prediction);

    if (FAILED (hr) || !m_watchpoints.TryMatchBefore (pc, prediction, hit))
    {
        return false;
    }

    for (const Watchpoint & entry : m_watchpoints.GetAll())
    {
        if (entry.id == hit.id)
        {
            m_watchpoints.SuppressAfterStopFor (pc, entry.first, entry.last);
        }
    }

    m_beforeHit = hit;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::GetUnwrittenHit
//
//  The read before written the heat map has, as a watchpoint hit: the
//  address read, the value and the instruction, under no watchpoint's id.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<WatchHit> DebugStopMonitor::GetUnwrittenHit() const
{
    HeatUnwrittenRead  read;
    WatchHit           hit;



    if (!m_isUnwrittenBreak || !m_target.TryGetUnwrittenStop (read))
    {
        return std::nullopt;
    }

    hit.id          = kUnwrittenHitId;
    hit.address     = read.address;
    hit.value       = read.value;
    hit.access      = WatchAccess::Read;
    hit.accessPc    = read.pc;
    hit.isUnwritten = true;
    return hit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::ReportStop
//
//  The breakpoint or watchpoint that caused the stop is attached here, where
//  it was recorded, then the Applesoft, disk call and disk reports and the
//  condition of the entry that stopped the machine.
//
////////////////////////////////////////////////////////////////////////////////

void DebugStopMonitor::ReportStop (StopEvent & event)
{
    AttachHit            (event);
    AttachBasicReport    (event);
    AttachDiskCallReport (event);
    AttachDiskReport     (event);
    AttachCondition      (event);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::AttachHit
//
//  A BRK ON or BRKOP stop has its breakpoint's id and a reason of its own,
//  as an RWTS or driver call's stop does. A stop at the Monitor's own
//  return, with no breakpoint behind it, is the program coming back: a run
//  to there.
//
////////////////////////////////////////////////////////////////////////////////

void DebugStopMonitor::AttachHit (StopEvent & event)
{
    Breakpoint  breakpoint;



    if (event.reason == StopReason::Breakpoint && m_beforeHit.has_value())
    {
        event.reason = StopReason::Watchpoint;
        event.watch  = m_beforeHit;
    }
    else if (event.reason == StopReason::Breakpoint)
    {
        event.breakpointId = m_lastBreakpointId;

        if (m_lastBreakpointId.has_value() && m_breakpoints.TryFind (*m_lastBreakpointId, breakpoint))
        {
            event.reason = (breakpoint.kind == BreakpointKind::Brk)    ? StopReason::Brk
                         : (breakpoint.kind == BreakpointKind::Opcode) ? StopReason::InvalidOpcode
                         : (breakpoint.IsDiskCall())                   ? StopReason::Disk
                         :                                                StopReason::Breakpoint;
        }
        else if (!m_lastBreakpointId.has_value() && m_monitorReturned)
        {
            event.reason = StopReason::RunTo;
        }
    }
    else if (event.reason == StopReason::Watchpoint)
    {
        event.watch = m_watchpoints.GetPendingHit();

        if (!event.watch.has_value())
        {
            event.watch = GetUnwrittenHit();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::AttachBasicReport
//
//  An Applesoft stop's report: a stop of its own when its breakpoint, step
//  or error stop made it, beside another stop at the same boundary
//  otherwise. Any stop ends a BASIC step.
//
////////////////////////////////////////////////////////////////////////////////

void DebugStopMonitor::AttachBasicReport (StopEvent & event)
{
    if (m_basicMonitor.HasStop())
    {
        event.basic = m_basicMonitor.GetStop().report;

        if (m_isBasicStop)
        {
            event.basic->isBasicStop = true;
            event.reason             = m_basicMonitor.GetStop().reason;
            event.breakpointId       = m_basicMonitor.GetStop().breakpointId;
        }
    }

    m_basicMonitor.ClearStop();
    m_basicMonitor.EndStep();
    m_isBasicStop = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::AttachDiskReport
//
//  The last instruction's disk events, for a stop that came another way,
//  then the disk report: a stop of its own when the disk breakpoint made it
//  or ended a step, beside a breakpoint's or watchpoint's stop otherwise
//  (FR-153).
//
////////////////////////////////////////////////////////////////////////////////

void DebugStopMonitor::AttachDiskReport (StopEvent & event)
{
    bool  isDiskReason = false;



    m_diskMonitor.Poll (m_context);

    if (m_diskMonitor.HasStop() && !event.disk.has_value())
    {
        isDiskReason       = m_isDiskStop || event.reason == StopReason::Step || event.reason == StopReason::Pause;
        event.disk         = m_diskMonitor.GetReport();
        event.reason       = isDiskReason ? StopReason::Disk : event.reason;
        event.breakpointId = isDiskReason ? std::optional<int> (event.disk->breakpointId) : event.breakpointId;
    }

    m_diskMonitor.ClearStop();
    m_isDiskStop = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::AttachDiskCallReport
//
//  The request of the RWTS or driver call the PC is at: the stop of that
//  breakpoint, or of an address breakpoint on the same routine, whose stop
//  has its own reason and the report of the lowest RWTS or driver id whose
//  request matches.
//
////////////////////////////////////////////////////////////////////////////////

void DebugStopMonitor::AttachDiskCallReport (StopEvent & event)
{
    DebugDiskState  state;
    DiskReport      report;
    Breakpoint      entry;
    bool            isStopped = event.breakpointId.has_value() && m_breakpoints.TryFind (*event.breakpointId, entry);
    auto            found     = std::ranges::find_if (m_breakpoints.GetAll(), [&] (const Breakpoint & candidate)
    {
        return candidate.enabled && candidate.IsDiskCall() && candidate.resolved == event.pc && DiskCallVector::IsRequestMatch (*candidate.disk, m_context);
    });



    if (!isStopped || (!entry.IsDiskCall() && found == m_breakpoints.GetAll().end()))
    {
        return;
    }

    entry = entry.IsDiskCall() ? entry : *found;
    m_target.TryGetDiskState (state);

    report.breakpointId = entry.id;
    report.event        = entry.disk->event;
    report.request      = DiskCallVector::ReadRequest (*entry.disk, m_context);
    report.vector       = DiskCallVector::GetVectorAddress (*entry.disk, state.slot);
    report.slot         = state.slot;
    report.drive        = std::clamp (report.request->drive - 1, 0, 1);
    report.quarterTrack = state.quarterTrack[report.drive];
    report.isMotorOn    = state.isMotorOn;
    report.isProtected  = state.isProtected[report.drive];
    report.phases       = state.phases;
    report.imageName    = state.imageName[report.drive];
    report.instruction  = event.pc;
    event.disk          = report;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::AttachCondition
//
//  The IF expression of the entry that stopped the machine, and the value it
//  had at the hit, which the table recorded then. A register breakpoint's
//  condition is the breakpoint itself, not an IF, and is not repeated.
//
////////////////////////////////////////////////////////////////////////////////

void DebugStopMonitor::AttachCondition (StopEvent & event) const
{
    Breakpoint  breakpoint;
    Watchpoint  watchpoint;



    if (event.breakpointId.has_value() && m_breakpoints.TryFind (*event.breakpointId, breakpoint) && breakpoint.kind != BreakpointKind::Register)
    {
        event.condition      = breakpoint.condition.text;
        event.conditionValue = m_breakpoints.GetLastConditionValue();
    }
    else if (event.watch.has_value() && m_watchpoints.TryFind (event.watch->id, watchpoint))
    {
        event.condition      = watchpoint.condition.text;
        event.conditionValue = event.watch->conditionValue;
    }
    else if (event.watch.has_value() && m_breakpoints.TryFind (event.watch->id, breakpoint))
    {
        event.condition      = breakpoint.condition.text;
        event.conditionValue = event.watch->conditionValue;
    }

    if (event.condition.empty())
    {
        event.conditionValue.reset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::OnStopped
//
//  A stop that is not the Monitor return's own leaves the return armed: the
//  address a Monitor `G` pushed is still on the stack, so a breakpoint partway
//  through the program does not let the final RTS run on into the ROM.
//
////////////////////////////////////////////////////////////////////////////////

void DebugStopMonitor::OnStopped()
{
    m_beamBreaks.OnStopped();
    m_watchpoints.ClearPending();
    m_target.ClearUnwrittenStop();
    m_lastBreakpointId.reset();
    m_beforeHit.reset();
    m_monitorReturned = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::ClearTemporary
//
//  A temporary breakpoint or watchpoint goes once it has caused a stop.
//
////////////////////////////////////////////////////////////////////////////////

void DebugStopMonitor::ClearTemporary (const StopEvent & stop)
{
    Breakpoint  breakpoint;
    Watchpoint  watchpoint;



    if (stop.breakpointId.has_value() && m_breakpoints.TryFind (*stop.breakpointId, breakpoint) && breakpoint.temporary)
    {
        m_breakpoints.TryClear (breakpoint.id);
    }

    if (stop.watch.has_value() && m_watchpoints.TryFind (stop.watch->id, watchpoint) && watchpoint.temporary)
    {
        m_watchpoints.TryClear (watchpoint.id);
    }

    //  A value breakpoint stops as a watchpoint does, under its own id.
    if (stop.watch.has_value() && m_breakpoints.TryFind (stop.watch->id, breakpoint) && breakpoint.temporary)
    {
        m_breakpoints.TryClear (breakpoint.id);
        m_watchpoints.RefreshWatchedPages();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::OnRunStarting
//
////////////////////////////////////////////////////////////////////////////////

void DebugStopMonitor::OnRunStarting()
{
    m_watchpoints.ClearPending();
    m_target.ClearUnwrittenStop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::OnMachineChanged
//
//  A different machine makes every recorded cause meaningless. With
//  isKeepingStops the breakpoints stay, so the last one's id does too.
//
////////////////////////////////////////////////////////////////////////////////

void DebugStopMonitor::OnMachineChanged (bool isKeepingStops)
{
    if (!isKeepingStops)
    {
        m_lastBreakpointId.reset();
    }

    m_watchpoints.ClearPending();
    m_target.ClearUnwrittenStop();
    m_beamBreaks.ClearAll();
    m_isUnwrittenBreak = false;
    m_target.SetUnwrittenBreak (false);
    m_beforeHit.reset();
    m_monitorReturn.reset();
    m_monitorReturned = false;
    m_basicMonitor.EndStep();
    m_basicMonitor.ClearStop();
    m_basicMonitor.Rebase();
    m_isBasicStop = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::OnReset
//
//  A reset abandons the stack a Monitor `G` pushed its return onto, so the
//  stop at that return is disarmed.
//
////////////////////////////////////////////////////////////////////////////////

void DebugStopMonitor::OnReset()
{
    m_monitorReturn.reset();
    m_monitorReturned = false;
    m_basicMonitor.Rebase();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugStopMonitor::ClearBreaks
//
//  The video, beam and read before written breaks, which go with the
//  breakpoints.
//
////////////////////////////////////////////////////////////////////////////////

void DebugStopMonitor::ClearBreaks()
{
    m_beamBreaks.ClearAll();
    SetUnwrittenBreak (false);
}
