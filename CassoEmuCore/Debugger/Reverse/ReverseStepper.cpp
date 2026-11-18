#include "Pch.h"

#include "Debugger/Reverse/ReverseStepper.h"
#include "Debugger/DebugMemoryView.h"
#include "Debugger/Reverse/HistorySpan.h"
#include "Debugger/Reverse/IReverseSeeker.h"
#include "Debugger/Reverse/IReverseStopTest.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Debugger/Reverse/Replayer.h"
#include "Debugger/Reverse/ReplayStepTable.h"
#include "Debugger/Reverse/ReverseSeeker.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::ReverseStepper
//
////////////////////////////////////////////////////////////////////////////////

ReverseStepper::ReverseStepper (
    MachineHost       & machine,
    KeyframeStore     & keyframes,
    HistoryTimeline   & timeline,
    Replayer          & replayer,
    HistorySpan       & span,
    ReplayStepTable   & stepTable,
    IReverseSeeker    & seeker) :
    m_machine   (machine),
    m_keyframes (keyframes),
    m_timeline  (timeline),
    m_replayer  (replayer),
    m_span      (span),
    m_stepTable (stepTable),
    m_seeker    (seeker)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::Release
//
//  Frees the stack-pointer buffer and the caller links.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseStepper::Release()
{
    DiscardCallerLinks();

    m_stackPointers = std::vector<Byte>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::DiscardCallerLinks
//
////////////////////////////////////////////////////////////////////////////////

void ReverseStepper::DiscardCallerLinks()
{
    m_callerLinks.clear();

    m_callerDepth   = 0;
    m_callerLanding = UINT64_MAX;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::StepBack
//
//  To the instruction before this one. The first step into a stretch builds
//  its table; one into a gap stops at the edge of it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseStepper::StepBack (ReverseResult & result)
{
    HRESULT     hr       = S_OK;
    uint64_t    position = m_machine.GetRunner().GetPosition();
    uint64_t    oldest   = GetOldestPosition();
    ReplayStep  step;
    bool        isInGap  = false;
    uint64_t    gapStart = 0;



    if (position <= oldest)
    {
        hr = m_seeker.LandAt (oldest, ReverseOutcome::AtHistoryStart, result);
        CHR (hr);
        BAIL_OUT_IF (true, S_OK);
    }

    m_isCut = false;

    hr = GetStep (position - 1, step, isInGap, gapStart);
    CHR (hr);

    if (m_isCut)
    {
        result = m_cutResult;
        BAIL_OUT_IF (true, S_OK);
    }

    hr = m_seeker.LandAt (isInGap ? gapStart : position - 1, isInGap ? ReverseOutcome::AtHistoryGap : ReverseOutcome::Moved, result);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::StepBackOver
//
//  To the instruction before this one at the same stack depth: when the one
//  before was a return, back over the whole call to its JSR.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseStepper::StepBackOver (ReverseResult & result)
{
    HRESULT   hr      = S_OK;
    bool      isFound = false;
    bool      isGap   = false;
    uint64_t  target  = 0;



    m_isCut     = false;
    m_isStopped = false;

    hr = FindStepOverTarget (m_machine.GetRunner().GetPosition(), m_machine.GetCpu()->GetSP(), isFound, target, isGap);
    CHR (hr);

    hr = Land (isFound, target, isGap, result);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::StepBackOut
//
//  To the JSR, or the interrupt, that entered the current routine. Live,
//  the debugger's call record returns at once when no call in history
//  entered it, and the machine stays where it is; when it has the call, the
//  machine seeks straight to it. Otherwise the search walks back, and ends
//  at the call it finds.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseStepper::StepBackOut (ReverseResult & result)
{
    HRESULT   hr      = S_OK;
    uint64_t  current = m_machine.GetRunner().GetPosition();
    int       spNow   = m_machine.GetCpu()->GetSP();
    bool      isDone  = false;
    bool      isFound = false;
    bool      isGap   = false;
    uint64_t  target  = 0;



    if (HasNoCaller())
    {
        ReverseSeeker::FillResult (m_machine, result);
        result.outcome = ReverseOutcome::NoCaller;
        BAIL_OUT_IF (true, S_OK);
    }

    m_isCut     = false;
    m_isStopped = false;

    hr = StepOutByRecord (isDone, result);
    CHR (hr);

    BAIL_OUT_IF (isDone, S_OK);

    hr = FindStepOutTarget (current, spNow, isFound, target, isGap);
    CHR (hr);

    hr = Land (isFound, target, isGap, result);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::Land
//
//  Where a search ended: the cut it made, the target it found, the edge of
//  a gap, where the user stopped it, or the start of history.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseStepper::Land (bool isFound, uint64_t target, bool isGap, ReverseResult & result)
{
    HRESULT         hr      = S_OK;
    ReverseOutcome  outcome = ReverseOutcome::Moved;



    if (m_isCut)
    {
        result = m_cutResult;
        BAIL_OUT_IF (true, S_OK);
    }

    outcome = isGap ? ReverseOutcome::AtHistoryGap : (isFound ? ReverseOutcome::Moved : ReverseOutcome::AtHistoryStart);
    outcome = m_isStopped ? ReverseOutcome::Stopped : outcome;

    hr = m_seeker.LandAt (isFound ? target : GetOldestPosition(), outcome, result);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::StepBackCycles
//
//  Back by a cycle count, a scanline or a frame, to the first instruction
//  boundary at or after the target cycle. A target at or before the oldest
//  keyframe lands on it, at history's start.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseStepper::StepBackCycles (
    uint64_t         cycles,
    ReverseResult  & result)
{
    HRESULT   hr          = S_OK;
    EmuCpu  * cpu         = m_machine.GetCpu();
    uint64_t  now         = 0;
    uint64_t  oldestCycle = 0;
    uint64_t  target      = 0;
    bool      isBeforeAll = false;



    CBRA (cpu);

    now         = cpu->GetHistoryCycles();
    oldestCycle = (m_keyframes.GetArena().GetCount() > 0) ? m_keyframes.GetArena().GetInfo (0).cycle : now;
    target      = (now > cycles) ? now - cycles : 0;
    isBeforeAll = target <= oldestCycle;

    hr = m_seeker.SeekToCycle (isBeforeAll ? oldestCycle : target, result);
    CHR (hr);

    if (isBeforeAll && result.outcome == ReverseOutcome::Moved)
    {
        result.outcome = ReverseOutcome::AtHistoryStart;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::PrepareRecentSteps
//
//  Behind live, the step table gets the instructions that led to the
//  current position, so the trace pane can list them: a step command has
//  usually built it already; otherwise the stretch is replayed once and the
//  machine put back where it was. A replay that diverges cuts history, and
//  result then reports the cut in place of the command's outcome.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseStepper::PrepareRecentSteps (ReverseResult & result)
{
    HRESULT        hr        = S_OK;
    uint64_t       position  = m_machine.GetRunner().GetPosition();
    bool           isInGap   = false;
    uint64_t       gapStart  = 0;
    ReplayStep     step;
    ReverseResult  landed;



    BAIL_OUT_IF (!m_span.IsInHistory() || position <= GetOldestPosition() || m_stepTable.Leads (position), S_OK);

    m_isCut = false;

    hr = GetStep (position - 1, step, isInGap, gapStart);
    CHR (hr);

    if (m_isCut)
    {
        result = m_cutResult;
        BAIL_OUT_IF (true, S_OK);
    }

    BAIL_OUT_IF (m_machine.GetRunner().GetPosition() == position, S_OK);

    hr = m_seeker.SeekToPosition (position, landed);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::GetRecentSteps
//
////////////////////////////////////////////////////////////////////////////////

void ReverseStepper::GetRecentSteps (
    size_t                     count,
    std::vector<ReplayStep>  & outSteps) const
{
    m_stepTable.GetRecent (m_machine.GetRunner().GetPosition(), count, outSteps);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::GetStep
//
//  The registers the instruction at position began with, from the table of
//  its stretch, which is built first when the table has another stretch or
//  none. A position recording skipped is in a gap, and outGapStart is where
//  the gap began. The machine is where building the table left it, so the
//  caller seeks afterward.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseStepper::GetStep (
    uint64_t      position,
    ReplayStep  & outStep,
    bool        & outIsInGap,
    uint64_t    & outGapStart)
{
    HRESULT  hr      = S_OK;
    bool     isFound = false;
    Stretch  stretch;



    outIsInGap = false;

    hr = m_seeker.LeaveLive();
    CHR (hr);

    hr = m_keyframes.WaitForPending();
    CHR (hr);

    isFound = TryFindStretch (position, stretch);
    CBRAEx (isFound, E_INVALIDARG);

    if (position >= stretch.end)
    {
        outIsInGap  = true;
        outGapStart = stretch.end;
        BAIL_OUT_IF (true, S_OK);
    }

    if (!m_stepTable.IsBuiltFor (stretch.start, stretch.end))
    {
        hr = BuildStepTable (stretch);
        CHR (hr);

        BAIL_OUT_IF (m_isCut, S_OK);
    }

    outStep = m_stepTable.Get (position);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::BuildStepTable
//
//  Replays the whole stretch once from its keyframe, filling the registers
//  of every instruction in it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseStepper::BuildStepTable (const Stretch & stretch)
{
    HRESULT                    hr     = S_OK;
    std::vector<ReplayStep>  & steps  = m_stepTable.BeginFill();
    ReplayTarget               target;
    ReplayReport               report;



    hr = m_replayer.RestoreKeyframe (stretch.keyframe);
    CHR (hr);

    target.position = stretch.end;

    hr = m_replayer.RunTo (target, stretch.end, nullptr, report, &steps);
    CHR (hr);

    if (report.isDiverged)
    {
        m_stepTable.Discard();

        hr = Cut (report);
        CHR (hr);
        BAIL_OUT_IF (true, S_OK);
    }

    hr = m_stepTable.Commit (stretch.start, stretch.end);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::Cut
//
//  A replay for a step diverged: history is cut, and the step reports the
//  cut.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseStepper::Cut (const ReplayReport & report)
{
    HRESULT  hr = S_OK;



    hr = m_seeker.HandleDivergence (report, m_cutResult);
    CHR (hr);

    m_isCut = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::ReplayStackLevels
//
//  The stack pointer each instruction from the start of the stretch that
//  has end - 1 up to end began with, into m_stackPointers: a replay from
//  the stretch's keyframe that stores one byte per instruction and nothing
//  else. A position recording skipped is in a gap, and outStretch.end is
//  where the gap began.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseStepper::ReplayStackLevels (
    uint64_t    end,
    Stretch   & outStretch,
    bool      & outIsInGap)
{
    HRESULT       hr      = S_OK;
    bool          isFound = false;
    size_t        count   = 0;
    ReplayTarget  target;
    ReplayReport  report;



    outIsInGap = false;

    hr = m_seeker.LeaveLive();
    CHR (hr);

    hr = m_keyframes.WaitForPending();
    CHR (hr);

    isFound = TryFindStretch (end - 1, outStretch);
    CBRAEx (isFound, E_INVALIDARG);

    outIsInGap = end - 1 >= outStretch.end;
    BAIL_OUT_IF (outIsInGap, S_OK);

    m_stackPointers.clear();

    hr = m_replayer.RestoreKeyframe (outStretch.keyframe);
    CHR (hr);

    target.position = end;

    hr = m_replayer.RunTo (target, outStretch.end, nullptr, report, nullptr, &m_stackPointers);
    CHR (hr);

    if (report.isDiverged)
    {
        hr = Cut (report);
        CHR (hr);
        BAIL_OUT_IF (true, S_OK);
    }

    count = m_stackPointers.size();
    CBRA (count == end - outStretch.start);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::FindStepOverTarget
//
//  The instruction before current, unless it was a return: an instruction
//  that began with the stack two or more bytes deeper than now. Then the
//  target is the newest instruction before it that began no deeper than now,
//  which is the JSR (or the interrupted instruction) the return came back
//  over. A gap stops the search at its edge.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseStepper::FindStepOverTarget (
    uint64_t    current,
    int         spNow,
    bool      & outFound,
    uint64_t  & outTarget,
    bool      & outIsGap)
{
    constexpr int  kReturnDepth = 2;
    HRESULT        hr           = S_OK;
    uint64_t       oldest       = GetOldestPosition();
    uint64_t       q            = 0;
    bool           isReturn     = false;
    bool           isInGap      = false;
    Stretch        stretch;



    outFound = false;
    outIsGap = false;

    BAIL_OUT_IF (current <= oldest, S_OK);

    hr = ReplayStackLevels (current, stretch, isInGap);
    CHR (hr);

    BAIL_OUT_IF (m_isCut, S_OK);

    isReturn = !isInGap && spNow - static_cast<int> (m_stackPointers.back()) >= kReturnDepth;

    if (!isReturn)
    {
        outFound  = true;
        outIsGap  = isInGap;
        outTarget = isInGap ? stretch.end : current - 1;
        BAIL_OUT_IF (true, S_OK);
    }

    for (q = current - 1; q > oldest && !outFound; q--)
    {
        // The stretch is used up: on to the one before it.
        if (q == stretch.start)
        {
            if (m_seeker.IsStopDue (current, q))
            {
                outFound    = true;
                outTarget   = q;
                m_isStopped = true;
                break;
            }

            hr = ReplayStackLevels (q, stretch, isInGap);
            CHR (hr);

            BAIL_OUT_IF (m_isCut, S_OK);
        }

        outFound  = isInGap || static_cast<int> (m_stackPointers[static_cast<size_t> (q - 1 - stretch.start)]) >= spNow;
        outIsGap  = isInGap;
        outTarget = isInGap ? q : q - 1;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::FindStepOutTarget
//
//  Walking back, the call that entered the current routine is the newest
//  JSR (the stack two bytes deeper after it) or interrupt (three bytes)
//  whose stack after it is as shallow as anything since: a call that already
//  returned left a shallower stack behind it. A gap stops the search at its
//  edge. The search replays one stretch at a time, newest first. The
//  progress reaches where the search ended, the last stretch included.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseStepper::FindStepOutTarget (
    uint64_t    current,
    int         spNow,
    bool      & outFound,
    uint64_t  & outTarget,
    bool      & outIsGap)
{
    constexpr int  kCallDepth      = 2;
    constexpr int  kInterruptDepth = 3;
    HRESULT        hr              = S_OK;
    uint64_t       oldest          = GetOldestPosition();
    int            spNext          = spNow;
    int            ceiling         = spNow;
    int            sp              = 0;
    int            pushed          = 0;
    uint64_t       q               = current;
    bool           isInGap         = false;
    bool           isStopDue       = false;
    Stretch        stretch;



    outFound = false;
    outIsGap = false;

    while (q > oldest && !outFound)
    {
        if (m_seeker.IsStopDue (current, q))
        {
            outFound    = true;
            outTarget   = q;
            m_isStopped = true;
            break;
        }

        hr = ReplayStackLevels (q, stretch, isInGap);
        CHR (hr);

        BAIL_OUT_IF (m_isCut, S_OK);

        if (isInGap)
        {
            outFound  = true;
            outIsGap  = true;
            outTarget = (q == current) ? stretch.end : q;
            break;
        }

        for (; q > stretch.start && !outFound; q--)
        {
            sp     = m_stackPointers[static_cast<size_t> (q - 1 - stretch.start)];
            pushed = sp - spNext;

            if ((pushed == kCallDepth || pushed == kInterruptDepth) && spNext >= ceiling)
            {
                outFound  = true;
                outTarget = q - 1;
            }

            spNext  = sp;
            ceiling = std::max (ceiling, spNext);
        }
    }

    isStopDue = m_seeker.IsStopDue (current, q);
    IGNORE_RETURN_VALUE (isStopDue, false);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::HasNoCaller
//
//  Only live: the call record follows the machine as it runs, and stands
//  for where history ends, not for a position inside it.
//
////////////////////////////////////////////////////////////////////////////////

bool ReverseStepper::HasNoCaller() const
{
    bool  isAskable = m_span.IsRecording() && m_span.IsLive() && m_callerProbe && m_keyframes.GetArena().GetCount() > 0;



    return isAskable && m_callerProbe (m_keyframes.GetArena().GetInfo (0).cycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::StepOutByRecord
//
//  The innermost link the call record gives is the call the search would
//  find, so the machine seeks straight to the cycle it began at: one
//  keyframe loaded and at most one stretch replayed. The landing is checked
//  against the link (the cycle, PC and stack pointer, and the JSR or BRK
//  there), and on a mismatch, with no link, with a link older than history,
//  or with a gap since the call, outIsDone stays false and the search runs.
//  A replay that diverges cuts history, and result then reports the cut.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseStepper::StepOutByRecord (
    bool           & outIsDone,
    ReverseResult  & result)
{
    HRESULT     hr       = S_OK;
    size_t      count    = 0;
    bool        hasLinks = false;
    bool        isUsable = false;
    CallerLink  link;



    outIsDone = false;

    hr = m_keyframes.WaitForPending();
    CHR (hr);

    hasLinks = TryGetCallerLinks (count);
    BAIL_OUT_IF (!hasLinks || count == 0, S_OK);

    link     = m_callerLinks[count - 1];
    isUsable = link.cycle >= m_keyframes.GetArena().GetInfo (0).cycle && !HasGapSince (link.cycle, m_machine.GetRunner().GetPosition());

    if (!isUsable)
    {
        DiscardCallerLinks();
        BAIL_OUT_IF (true, S_OK);
    }

    hr = m_seeker.SeekToCycle (link.cycle, result);
    CHR (hr);

    if (result.outcome == ReverseOutcome::HistoryCut)
    {
        outIsDone = true;
        BAIL_OUT_IF (true, S_OK);
    }

    if (!IsAtCallerLink (link))
    {
        DiscardCallerLinks();
        BAIL_OUT_IF (true, S_OK);
    }

    if (m_stepTable.HasSteps() && !m_stepTable.Contains (result.position))
    {
        m_stepTable.Discard();
    }

    m_callerDepth   = count - 1;
    m_callerLanding = m_machine.GetRunner().GetPosition();
    result.outcome  = ReverseOutcome::Moved;
    outIsDone       = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::TryGetCallerLinks
//
//  How many of the links step back out may use from where the machine is:
//  live, the record is read now and all of them; behind live, the ones
//  outside the call the last step back out landed on, while the machine is
//  on it. From that call, the search would find the next link out: any call
//  between the two that the stack has not risen above since is in the
//  record, as a link between them.
//
////////////////////////////////////////////////////////////////////////////////

bool ReverseStepper::TryGetCallerLinks (size_t & outCount)
{
    bool  isAskable = m_span.IsRecording() && m_span.IsLive() && m_callerLinksProbe && m_keyframes.GetArena().GetCount() > 0;
    bool  isLanded  = !m_span.IsLive() && m_machine.GetRunner().GetPosition() == m_callerLanding;



    if (isAskable)
    {
        m_callerLinksProbe (m_callerLinks);

        m_callerDepth   = m_callerLinks.size();
        m_callerLanding = m_machine.GetRunner().GetPosition();
    }

    outCount = m_callerDepth;

    return isAskable || isLanded;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::HasGapSince
//
//  Whether recording skipped any position between cycle and position: a
//  keyframe after cycle, at or before position, that ends a gap, or, live
//  while paused, the stretch the next step back would close.
//
////////////////////////////////////////////////////////////////////////////////

bool ReverseStepper::HasGapSince (
    uint64_t  cycle,
    uint64_t  position) const
{
    size_t                index  = m_keyframes.GetArena().GetCount();
    bool                  hasGap = m_span.IsPausedBefore (position);
    const KeyframeInfo  * info   = nullptr;



    while (index > 0 && !hasGap)
    {
        index--;
        info = &m_keyframes.GetArena().GetInfo (index);

        if (info->cycle <= cycle)
        {
            break;
        }

        hasGap = info->hasGapBefore && info->position <= position;
    }

    return hasGap;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::IsAtCallerLink
//
//  The machine is where the link's call began: at its cycle, with its PC
//  and stack pointer, and on a JSR or BRK when it was one.
//
////////////////////////////////////////////////////////////////////////////////

bool ReverseStepper::IsAtCallerLink (const CallerLink & link)
{
    const EmuCpu     * cpu    = m_machine.GetCpu();
    DebugMemoryView    memory (m_machine);
    Byte               opcode = 0;
    bool               isRead = memory.TryPeek (link.callSite, opcode);
    bool               isCall = link.isInterrupt || (isRead && opcode == link.opcode);



    return cpu->GetHistoryCycles() == link.cycle && cpu->GetPC() == link.callSite && cpu->GetSP() == link.stackLevel && isCall;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::TryFindStretch
//
////////////////////////////////////////////////////////////////////////////////

bool ReverseStepper::TryFindStretch (uint64_t position, Stretch & outStretch) const
{
    return m_timeline.TryFindStretch (position, m_span.GetRecordedEnd (m_machine.GetRunner().GetPosition()), outStretch);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper::GetOldestPosition
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ReverseStepper::GetOldestPosition() const
{
    return m_timeline.GetOldestPosition (m_machine.GetRunner().GetPosition());
}
