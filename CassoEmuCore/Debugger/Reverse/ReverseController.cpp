#include "Pch.h"

#include "Debugger/Reverse/ReverseController.h"

#include "Core/StateWriter.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Debugger/Reverse/IReverseStopTest.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseController::ReverseController
//
////////////////////////////////////////////////////////////////////////////////

ReverseController::ReverseController (MachineHost & machine) :
    m_machine  (machine),
    m_replayer (machine, m_keyframes)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseController::~ReverseController
//
////////////////////////////////////////////////////////////////////////////////

ReverseController::~ReverseController()
{
    Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Start
//
//  Turns the input journal on and takes the first keyframe at once, so
//  history begins at the moment the journal attaches; the keyframe store
//  takes its whole budget there. The disks are machine state from here: a
//  disk that leaves its bay is kept in memory while history may put it back,
//  and the automatic flushes are held whenever the machine is behind live.
//  A pause chosen before the start holds from the first keyframe on.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::Start (const ReverseSettings & settings)
{
    constexpr uint64_t  kMinInstructionCycles = 2;
    constexpr size_t    kStepSlack            = 64;
    HRESULT             hr                    = S_OK;
    EmuCpu            * cpu                   = m_machine.GetCpu();



    CBRA (cpu);

    m_keyframes.Configure (settings.keyframes);

    hr = UseWorkQueue();
    CHR (hr);

    m_steps.reserve (static_cast<size_t> (settings.keyframes.intervalCycles / kMinInstructionCycles) + kStepSlack);

    m_machine.GetInputJournal().Clear();
    m_machine.SetInputJournalOn  (true);
    m_machine.SetHistoryRecorder (this);

    m_machine.GetDiskStore().SetFlushHold      (false);
    m_machine.GetDiskStore().SetMediaRetention (true);

    m_isRecording = true;
    m_isLive      = true;

    hr = CaptureNow();
    CHR (hr);

    if (m_isPaused)
    {
        BecomeLive();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Stop
//
//  Detaches from the machine, drops all history and gives back the memory
//  it held. The disks keep any writes they hold, which the next flush
//  writes as they stand.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::Stop()
{
    if (m_machine.GetHistoryRecorder() == this)
    {
        m_machine.SetHistoryRecorder (nullptr);
    }

    if (m_isRecording)
    {
        m_machine.SetInputJournalOn (false);
        m_machine.GetInputJournal().Clear();

        m_machine.GetDiskStore().SetFlushHold      (false);
        m_machine.GetDiskStore().SetMediaRetention (false);
    }

    m_keyframes.Release();

    DiscardStepTable();
    m_steps = std::vector<ReplayStep>();

    m_isRecording   = false;
    m_isLive        = true;
    m_isEditPending = false;
    m_nextDueCycle  = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetUserMaximumSpeed
//
//  The user chose Maximum speed, or left it. Recording pauses at Maximum:
//  where the machine stands is the end of what history holds, until a
//  keyframe taken where recording resumes starts the next part, marked as
//  following a gap. A speed raised automatically must not come here.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::SetUserMaximumSpeed (bool isMaximum)
{
    HRESULT   hr       = S_OK;
    uint64_t  gapStart = m_pauseStart;
    bool      hasGap   = false;



    BAIL_OUT_IF (isMaximum == m_isPaused, S_OK);

    m_isPaused = isMaximum;

    BAIL_OUT_IF (!m_isRecording || !m_isLive, S_OK);

    if (isMaximum)
    {
        BecomeLive();
        BAIL_OUT_IF (true, S_OK);
    }

    hasGap = m_machine.GetPosition() > gapStart;

    m_machine.SetInputJournalOn (true);

    if (hasGap)
    {
        hr = CaptureBoundary (true, gapStart);
        CHR (hr);
    }

    BecomeLive();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMachineChanged
//
//  Something outside the recorded inputs changed the machine: the recorded
//  future no longer follows from it, so it is dropped, and the changed state
//  becomes a boundary keyframe, which a replay reaching it loads, since no
//  replay from an earlier one would reproduce the change. The machine is live
//  again where it stands.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::OnMachineChanged()
{
    HRESULT   hr         = S_OK;
    EmuCpu  * cpu        = m_machine.GetCpu();
    uint64_t  position   = m_machine.GetPosition();
    uint64_t  gapStart   = m_pauseStart;
    bool      isAfterGap = false;



    CBRA (cpu);
    BAIL_OUT_IF (!m_isRecording, S_OK);

    isAfterGap = m_isLive && m_isPaused && position > gapStart;

    if (!m_isLive)
    {
        m_machine.GetInputJournal().Truncate (m_replayer.GetJournalCursor());
    }

    // By position: a power cycle restarts the cycle counter, and every
    // keyframe from before it would look newer than the machine.
    if (position == 0)
    {
        m_keyframes.Clear();
    }
    else
    {
        hr = m_keyframes.DropAfterPosition (position - 1);
        CHR (hr);
    }

    BecomeLive();

    hr = CaptureBoundary (isAfterGap, gapStart);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMediaChanged
//
//  A disk went in or out, or its file's write protection changed. The journal
//  holds the command, but a replay cannot redo it from the file, which may
//  have changed since; the boundary keyframe taken here holds the disks as
//  they now stand instead.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::OnMediaChanged (MachineHost & machine)
{
    HRESULT  hr = S_OK;



    if (!m_isRecording || m_replayer.IsReplaying() || &machine != &m_machine)
    {
        return;
    }

    hr = OnMachineChanged();
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMachineEdited
//
//  The debugger changed memory, registers or I/O state. In the past that
//  drops the recorded future at once. Live, the change becomes one boundary
//  keyframe before the next instruction or reverse command, so a fill or a
//  run of pokes costs one capture rather than one each.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::OnMachineEdited (MachineHost & machine)
{
    HRESULT  hr = S_OK;



    if (!m_isRecording || m_replayer.IsReplaying() || &machine != &m_machine)
    {
        return;
    }

    if (!m_isLive)
    {
        hr = OnMachineChanged();
        IGNORE_RETURN_VALUE (hr, S_OK);
        return;
    }

    m_isEditPending = true;
    m_nextDueCycle  = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepBack
//
//  To the instruction before this one. The first step into a stretch builds
//  its table; one into a gap stops at the edge of it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::StepBack (ReverseResult & result)
{
    HRESULT     hr       = S_OK;
    uint64_t    position = m_machine.GetPosition();
    uint64_t    oldest   = GetOldestPosition();
    ReplayStep  step;
    bool        isInGap  = false;
    uint64_t    gapStart = 0;



    if (position <= oldest)
    {
        hr = LandAt (oldest, ReverseOutcome::AtHistoryStart, result);
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

    hr = LandAt (isInGap ? gapStart : position - 1, isInGap ? ReverseOutcome::AtHistoryGap : ReverseOutcome::Moved, result);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepForward
//
//  To the instruction after this one, replaying the recorded future. Live,
//  there is nothing recorded ahead and the machine stays where it is.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::StepForward (ReverseResult & result)
{
    return SeekToPosition (m_machine.GetPosition() + 1, result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepBackOver
//
//  To the instruction before this one at the same stack depth: when the one
//  before was a return, back over the whole call to its JSR.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::StepBackOver (ReverseResult & result)
{
    HRESULT         hr      = S_OK;
    bool            isFound = false;
    bool            isGap   = false;
    uint64_t        target  = 0;
    ReverseOutcome  outcome = ReverseOutcome::Moved;



    m_isCut = false;

    hr = FindStepOverTarget (m_machine.GetPosition(), isFound, target, isGap);
    CHR (hr);

    if (m_isCut)
    {
        result = m_cutResult;
        BAIL_OUT_IF (true, S_OK);
    }

    outcome = isGap ? ReverseOutcome::AtHistoryGap : (isFound ? ReverseOutcome::Moved : ReverseOutcome::AtHistoryStart);

    hr = LandAt (isFound ? target : GetOldestPosition(), outcome, result);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepBackOut
//
//  To the JSR, or the interrupt, that entered the current routine.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::StepBackOut (ReverseResult & result)
{
    HRESULT         hr      = S_OK;
    bool            isFound = false;
    bool            isGap   = false;
    uint64_t        target  = 0;
    ReverseOutcome  outcome = ReverseOutcome::Moved;



    m_isCut = false;

    hr = FindStepOutTarget (m_machine.GetPosition(), isFound, target, isGap);
    CHR (hr);

    if (m_isCut)
    {
        result = m_cutResult;
        BAIL_OUT_IF (true, S_OK);
    }

    outcome = isGap ? ReverseOutcome::AtHistoryGap : (isFound ? ReverseOutcome::Moved : ReverseOutcome::AtHistoryStart);

    hr = LandAt (isFound ? target : GetOldestPosition(), outcome, result);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepBackCycles
//
//  Back by a cycle count, a scanline or a frame, to the first instruction
//  boundary at or after the target cycle.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::StepBackCycles (
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

    now         = cpu->GetTotalCycles();
    oldestCycle = (m_keyframes.GetCount() > 0) ? m_keyframes.GetInfo (0).cycle : now;
    target      = (now > cycles) ? now - cycles : 0;
    isBeforeAll = target < oldestCycle;

    hr = SeekToCycle (isBeforeAll ? oldestCycle : target, result);
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
//  ReverseContinue
//
//  To the latest position before this one where the stop test fires. Each
//  stretch of history is replayed forward once with the stop test asked
//  about every instruction, newest stretch first, and the latest hit wins; a
//  breakpoint lands before its instruction and a watchpoint after the
//  instruction that tripped it, as running forward would have. Nothing the
//  replay runs reaches the debugger's own breakpoints or watchpoints.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::ReverseContinue (
    IReverseStopTest  & stopTest,
    ReverseResult     & result)
{
    HRESULT       hr      = S_OK;
    uint64_t      from    = m_machine.GetPosition();
    uint64_t      end     = from;
    uint64_t      oldest  = 0;
    Stretch       stretch;
    ReplayTarget  target;
    ReplayReport  report;



    CBRAEx (m_isRecording, E_UNEXPECTED);

    hr = LeaveLive();
    CHR (hr);

    oldest = GetOldestPosition();

    while (end > oldest)
    {
        hr = RestoreAtOrBefore (end - 1, 0, false, stretch);
        CHR (hr);

        target.position = std::min (end, stretch.end);

        hr = m_replayer.RunTo (target, from, &stopTest, report);
        CHR (hr);

        if (report.isDiverged)
        {
            hr = HandleDivergence (report, result);
            CHR (hr);
            BAIL_OUT_IF (true, S_OK);
        }

        if (report.hasHit)
        {
            hr = LandAt (report.lastHit, ReverseOutcome::Moved, result);
            CHR (hr);
            BAIL_OUT_IF (true, S_OK);
        }

        end = stretch.start;
    }

    hr = LandAt (oldest, ReverseOutcome::AtHistoryStart, result);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SeekToCycle
//
//  To the first instruction boundary at or after cycle, within history.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::SeekToCycle (
    uint64_t         cycle,
    ReverseResult  & result)
{
    ReplayTarget  target;
    ReplayReport  report;



    target.cycle = cycle;

    return Seek (target, true, nullptr, report, result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SeekToPosition
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::SeekToPosition (
    uint64_t         position,
    ReverseResult  & result)
{
    ReplayTarget  target;
    ReplayReport  report;



    target.position = position;

    return Seek (target, false, nullptr, report, result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsInHistory
//
////////////////////////////////////////////////////////////////////////////////

bool ReverseController::IsInHistory() const
{
    return m_isRecording && !m_isLive;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetOldestPosition
//
//  The first keyframe is the oldest position any replay can start from.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ReverseController::GetOldestPosition() const
{
    return (m_keyframes.GetCount() > 0) ? m_keyframes.GetInfo (0).position : m_machine.GetPosition();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLiveEndPosition
//
//  The newest position history holds: where the machine is while live, and
//  where it was when it last left live otherwise.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ReverseController::GetLiveEndPosition() const
{
    return m_isLive ? m_machine.GetPosition() : m_liveEndPosition;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Seek
//
//  A target in the stretch the machine stands in, and ahead of it, is
//  replayed on from where the machine stands; any other loads the keyframe
//  at or before it first. A target in a gap stops at its edge: the near edge
//  going forward from before it, the far one otherwise. Reaching the end of
//  history makes the machine live again.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::Seek (
    const ReplayTarget  & target,
    bool                  isByCycle,
    IReverseStopTest    * stopTest,
    ReplayReport        & report,
    ReverseResult       & result)
{
    HRESULT   hr        = S_OK;
    EmuCpu  * cpu       = m_machine.GetCpu();
    bool      isForward = false;
    bool      isHere    = false;
    bool      isShort   = false;
    size_t    keyframe  = 0;
    Stretch   stretch;



    CBRA (cpu);
    CBRAEx (m_isRecording, E_UNEXPECTED);

    isForward = isByCycle ? target.cycle >= cpu->GetTotalCycles() : target.position >= m_machine.GetPosition();

    result.outcome = ReverseOutcome::Moved;

    if (isForward && m_isLive)
    {
        FillResult (result);
        BAIL_OUT_IF (true, S_OK);
    }

    hr = LeaveLive();
    CHR (hr);

    // Keyframe indices are taken only once nothing is in flight.
    hr = m_keyframes.WaitForPending();
    CHR (hr);

    isHere = TryFindStretch (m_machine.GetPosition(), stretch);
    isHere = isHere && isForward && (isByCycle ? m_keyframes.TryFindAtOrBefore (target.cycle, keyframe) : m_keyframes.TryFindByPosition (target.position, keyframe));
    isHere = isHere && keyframe == stretch.keyframe && (isByCycle || target.position <= stretch.end);

    if (!isHere)
    {
        hr = RestoreAtOrBefore (target.position, target.cycle, isByCycle, stretch);
        CHR (hr);

        // Going forward into a gap: on to the keyframe where it ends.
        if (isForward && !isByCycle && target.position > stretch.end && stretch.keyframe + 1 < m_keyframes.GetCount())
        {
            hr = m_replayer.RestoreKeyframe (stretch.keyframe + 1);
            CHR (hr);

            result.outcome = ReverseOutcome::AtHistoryGap;
            stretch.end    = m_machine.GetPosition();
        }
    }

    hr = m_replayer.RunTo (target, stretch.end, stopTest, report);
    CHR (hr);

    if (report.isDiverged)
    {
        hr = HandleDivergence (report, result);
        CHR (hr);
        BAIL_OUT_IF (true, S_OK);
    }

    isShort = isByCycle ? cpu->GetTotalCycles() < target.cycle : m_machine.GetPosition() < target.position;

    if (isShort && m_machine.GetPosition() < m_liveEndPosition)
    {
        result.outcome = ReverseOutcome::AtHistoryGap;
    }

    if (m_machine.GetPosition() >= m_liveEndPosition)
    {
        BecomeLive();
    }

    FillResult (result);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RestoreAtOrBefore
//
//  Loads the newest keyframe at or before the target, or the oldest keyframe
//  when the target is older than all history, and gives the stretch it
//  starts.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::RestoreAtOrBefore (
    uint64_t    position,
    uint64_t    cycle,
    bool        isByCycle,
    Stretch   & outStretch)
{
    HRESULT  hr       = S_OK;
    size_t   keyframe = 0;
    bool     hasAny   = false;
    bool     isFound  = false;



    hr = m_keyframes.WaitForPending();
    CHR (hr);

    hasAny = m_keyframes.GetCount() > 0;
    CBRAEx (hasAny, E_UNEXPECTED);

    isFound = isByCycle ? m_keyframes.TryFindAtOrBefore (cycle, keyframe) : m_keyframes.TryFindByPosition (position, keyframe);

    if (!isFound)
    {
        keyframe = 0;
    }

    hr = m_replayer.RestoreKeyframe (keyframe);
    CHR (hr);

    outStretch.keyframe = keyframe;
    outStretch.start    = m_keyframes.GetInfo (keyframe).position;
    outStretch.end      = GetSegmentEnd (keyframe);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleDivergence
//
//  A replay reached a keyframe and the machine did not hash to it, so the
//  history from the keyframe before it on cannot be trusted. The machine goes
//  back to that last good keyframe, and every keyframe and input after it is
//  dropped; the machine is live there.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::HandleDivergence (
    const ReplayReport  & report,
    ReverseResult       & result)
{
    HRESULT       hr      = S_OK;
    bool          hasGood = report.divergedKeyframe > 0;
    size_t        good    = 0;
    KeyframeInfo  info;



    CBRAEx (hasGood, E_UNEXPECTED);

    good = report.divergedKeyframe - 1;
    info = m_keyframes.GetInfo (good);

    hr = m_replayer.RestoreKeyframe (good);
    CHR (hr);

    hr = m_keyframes.DropAfterPosition (info.position);
    CHR (hr);

    m_machine.GetInputJournal().Truncate (info.journalIndex);

    BecomeLive();

    FillResult (result);
    result.outcome = ReverseOutcome::HistoryCut;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetStep
//
//  The PC and stack pointer the instruction at position began with, from
//  the table of its stretch, which is built first when the table holds
//  another stretch or none. A position recording skipped is in a gap, and
//  outGapStart is where the gap began. Leaves the machine wherever building
//  the table left it, so the caller seeks afterward.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::GetStep (
    uint64_t      position,
    ReplayStep  & outStep,
    bool        & outIsInGap,
    uint64_t    & outGapStart)
{
    HRESULT  hr       = S_OK;
    Stretch  stretch;
    bool     isFound  = false;
    bool     isBuilt  = false;



    outIsInGap = false;

    hr = LeaveLive();
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

    isBuilt = m_hasSteps && m_stepsStart == stretch.start && m_stepsEnd == stretch.end;

    if (!isBuilt)
    {
        hr = BuildStepTable (stretch);
        CHR (hr);

        BAIL_OUT_IF (m_isCut, S_OK);
    }

    outStep = m_steps[static_cast<size_t> (position - m_stepsStart)];

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildStepTable
//
//  Replays the whole stretch once from its keyframe, keeping the PC and
//  stack pointer of every instruction in it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::BuildStepTable (const Stretch & stretch)
{
    HRESULT       hr         = S_OK;
    ReplayTarget  target;
    ReplayReport  report;
    size_t        stepCount  = 0;



    DiscardStepTable();

    hr = m_replayer.RestoreKeyframe (stretch.keyframe);
    CHR (hr);

    target.position = stretch.end;

    hr = m_replayer.RunTo (target, stretch.end, nullptr, report, &m_steps);
    CHR (hr);

    if (report.isDiverged)
    {
        DiscardStepTable();

        hr = HandleDivergence (report, m_cutResult);
        CHR (hr);

        m_isCut = true;
        BAIL_OUT_IF (true, S_OK);
    }

    stepCount = m_steps.size();
    CBRA (stepCount == stretch.end - stretch.start);

    m_stepsStart = stretch.start;
    m_stepsEnd   = stretch.end;
    m_hasSteps   = true;

    m_tableBuilds++;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindStepOverTarget
//
//  The instruction before current, unless it was a return: an instruction
//  that began with the stack two or more bytes deeper than now. Then the
//  target is the newest instruction before it that began no deeper than now,
//  which is the JSR (or the interrupted instruction) the return came back
//  over. A gap stops the search at its edge.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::FindStepOverTarget (
    uint64_t    current,
    bool      & outFound,
    uint64_t  & outTarget,
    bool      & outIsGap)
{
    constexpr int  kReturnDepth = 2;
    HRESULT        hr           = S_OK;
    uint64_t       oldest       = GetOldestPosition();
    int            spNow        = m_machine.GetCpu()->GetSP();
    ReplayStep     step;
    uint64_t       q            = 0;
    bool           isReturn     = false;
    bool           isInGap      = false;
    uint64_t       gapStart     = 0;



    outFound = false;
    outIsGap = false;

    BAIL_OUT_IF (current <= oldest, S_OK);

    hr = GetStep (current - 1, step, isInGap, gapStart);
    CHR (hr);

    BAIL_OUT_IF (m_isCut, S_OK);

    isReturn = spNow - static_cast<int> (step.sp) >= kReturnDepth;

    if (isInGap || !isReturn)
    {
        outFound  = true;
        outIsGap  = isInGap;
        outTarget = isInGap ? gapStart : current - 1;
        BAIL_OUT_IF (true, S_OK);
    }

    for (q = current - 1; q > oldest && !outFound; q--)
    {
        hr = GetStep (q - 1, step, isInGap, gapStart);
        CHR (hr);

        BAIL_OUT_IF (m_isCut, S_OK);

        if (isInGap || static_cast<int> (step.sp) >= spNow)
        {
            outFound  = true;
            outIsGap  = isInGap;
            outTarget = isInGap ? q : q - 1;
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindStepOutTarget
//
//  Walking back, the call that entered the current routine is the newest
//  JSR (the stack two bytes deeper after it) or interrupt (three bytes)
//  whose stack after it is as shallow as anything since: a call that already
//  returned left a shallower stack behind it. A gap stops the search at its
//  edge.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::FindStepOutTarget (
    uint64_t    current,
    bool      & outFound,
    uint64_t  & outTarget,
    bool      & outIsGap)
{
    constexpr int  kCallDepth      = 2;
    constexpr int  kInterruptDepth = 3;
    HRESULT        hr              = S_OK;
    uint64_t       oldest          = GetOldestPosition();
    int            spNext          = m_machine.GetCpu()->GetSP();
    int            ceiling         = spNext;
    int            pushed          = 0;
    ReplayStep     step;
    uint64_t       q               = 0;
    bool           isInGap         = false;
    uint64_t       gapStart        = 0;



    outFound = false;
    outIsGap = false;

    for (q = current; q > oldest && !outFound; q--)
    {
        hr = GetStep (q - 1, step, isInGap, gapStart);
        CHR (hr);

        BAIL_OUT_IF (m_isCut, S_OK);

        if (isInGap)
        {
            outFound  = true;
            outIsGap  = true;
            outTarget = (q == current) ? gapStart : q;
            break;
        }

        pushed = static_cast<int> (step.sp) - spNext;

        if ((pushed == kCallDepth || pushed == kInterruptDepth) && spNext >= ceiling)
        {
            outFound  = true;
            outTarget = q - 1;
        }

        spNext  = step.sp;
        ceiling = std::max (ceiling, spNext);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LandAt
//
//  Seeks to position and reports outcome, unless the seek itself ended
//  otherwise. A landing outside the stretch the step table holds drops it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::LandAt (
    uint64_t         position,
    ReverseOutcome   outcome,
    ReverseResult  & result)
{
    HRESULT  hr = S_OK;



    hr = SeekToPosition (position, result);
    CHR (hr);

    if (result.outcome == ReverseOutcome::Moved)
    {
        result.outcome = outcome;
    }

    if (m_hasSteps && (result.position < m_stepsStart || result.position >= m_stepsEnd))
    {
        DiscardStepTable();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureNow
//
//  Saves the machine once and hands the state to the keyframe store.
//  Keyframes dropped over budget take their journal records with them.
//
//  The save shares the RAM chunks and disk tracks not written since the
//  last one, so only what changed is copied; the keyframe store flattens it
//  into a buffer of its own and packs it on its work queue. Every buffer,
//  list and writer here is reused, so a capture allocates nothing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::CaptureNow()
{
    HRESULT            hr         = S_OK;
    EmuCpu           * cpu        = m_machine.GetCpu();
    InputJournal     & journal    = m_machine.GetInputJournal();
    uint64_t           position   = m_machine.GetPosition();
    uint64_t           cycle      = 0;
    size_t             journalEnd = 0;
    StateWriter      & writer     = m_writer;
    std::string_view   hostState;



    CBRA (cpu);

    cycle = cpu->GetTotalCycles();

    // The store orders keyframes by cycle, which a power cycle restarts. A
    // capture before the cycle count passes the newest keyframe's again (a
    // debugger edit or a disk change soon after a power cycle) cannot follow
    // it, so history before the power cycle goes and starts over here.
    if (m_keyframes.GetCount() > 0 && m_keyframes.GetInfo (m_keyframes.GetCount() - 1).cycle >= cycle)
    {
        m_keyframes.Clear();
    }

    // A host write no read has seen yet is in the snapshot; the sync record
    // puts it in front of any replay that crosses this point as well.
    m_hostWriter.Reuse (m_hostWriter.TakeBytes());

    hr = m_machine.SaveHostInputState (m_hostWriter);
    CHR (hr);

    hostState = std::string_view (reinterpret_cast<const char *> (m_hostWriter.GetBytes().data()), m_hostWriter.GetBytes().size());

    m_machine.RecordInput (InputKind::HostState, 0, 0, hostState);

    journalEnd = journal.GetEndIndex();

    writer.Reuse      (writer.TakeBytes(), writer.TakeSegments());
    writer.SetSharing (true);

    hr = m_machine.SaveState (writer);
    CHR (hr);

    m_machine.CheckSharedSave (writer);

    hr = m_keyframes.Add (position, cycle, journalEnd, writer);
    CHR (hr);

    journal.DiscardBefore (m_keyframes.GetInfo (0).journalIndex);

    PruneRetainedMedia();

Error:
    ScheduleCaptures();

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureBoundary
//
//  A keyframe a replay loads rather than reaches: after a change from
//  outside the recorded inputs, or where recording resumed after a gap that
//  began at gapStart. While paused, recording stops again just after it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::CaptureBoundary (
    bool      isAfterGap,
    uint64_t  gapStart)
{
    HRESULT  hr = S_OK;



    hr = CaptureNow();
    CHR (hr);

    if (isAfterGap)
    {
        m_keyframes.MarkNewestGap (gapStart);
    }
    else
    {
        m_keyframes.MarkNewestBoundary();
    }

    if (m_isPaused)
    {
        m_pauseStart = m_machine.GetPosition();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCaptureDue
//
//  A keyframe fell due, or the machine is in history, where every
//  instruction start comes here. During a replay the keyframes exist
//  already. A machine stepped from the past by anything but a replay is
//  running live from there, so the recorded future goes first.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::OnCaptureDue (uint64_t cycle)
{
    HRESULT  hr = S_OK;



    UNREFERENCED_PARAMETER (cycle);

    if (m_replayer.IsReplaying())
    {
        return;
    }

    if (!m_isLive || m_isEditPending)
    {
        m_isEditPending = false;

        hr = OnMachineChanged();
    }
    else
    {
        hr = CaptureNow();
    }

    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LeaveLive
//
//  Remembers where history ends before the machine moves into it. While
//  paused, the position the machine reached is kept first as a keyframe
//  after the gap, so stepping back from it, and coming back to it, work.
//  From here every instruction start reaches OnCaptureDue, which is how a
//  machine run from the past without a seek is noticed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::LeaveLive()
{
    HRESULT   hr       = S_OK;
    uint64_t  position = m_machine.GetPosition();



    // A debugger edit made here, live, is kept first, so going back and then
    // forward again comes back to the edited machine.
    if (m_isLive && m_isEditPending)
    {
        m_isEditPending = false;

        hr = OnMachineChanged();
        CHR (hr);
    }

    if (m_isLive && m_isPaused && position > m_pauseStart)
    {
        hr = CaptureBoundary (true, m_pauseStart);
        CHR (hr);
    }

    if (m_isLive)
    {
        m_liveEndPosition = position;
        m_liveEndCycle    = m_machine.GetCpu()->GetTotalCycles();
        m_isLive          = false;

        m_machine.GetDiskStore().SetFlushHold (true);
    }

    m_nextDueCycle = 0;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BecomeLive
//
//  The machine is at the end of history: the journal records again and the
//  keyframes fall due on their own schedule, or, while paused, recording
//  stops where the machine stands. The automatic disk flushes resume.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::BecomeLive()
{
    m_isLive = true;

    m_machine.GetDiskStore().SetFlushHold (false);

    DiscardStepTable();

    if (m_isPaused)
    {
        m_pauseStart = m_machine.GetPosition();
    }

    m_machine.SetInputJournalOn (!m_isPaused);

    ScheduleCaptures();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScheduleCaptures
//
//  The next cycle at which a keyframe falls due: never while paused, and
//  zero in history, so every instruction start is checked there.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::ScheduleCaptures()
{
    if (!m_isLive)
    {
        m_nextDueCycle = 0;
    }
    else if (m_isPaused)
    {
        m_nextDueCycle = UINT64_MAX;
    }
    else
    {
        m_nextDueCycle = m_keyframes.GetNextDueCycle();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiscardStepTable
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::DiscardStepTable()
{
    m_steps.clear();

    m_hasSteps   = false;
    m_stepsStart = 0;
    m_stepsEnd   = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PruneRetainedMedia
//
//  Lets the disk store drop the disks that left their bays before every
//  keyframe still held.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::PruneRetainedMedia()
{
    if (m_keyframes.GetCount() > 0)
    {
        m_machine.GetDiskStore().PruneRetainedMedia (m_keyframes.GetInfo (0).position);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UseWorkQueue
//
//  Keyframes are packed on the queue a test set, or on the thread pool,
//  whose queue is created on first use with room for every buffer the store
//  can have in flight.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::UseWorkQueue()
{
    HRESULT  hr        = S_OK;
    bool     isCreated = m_workQueue.IsCreated();



    if (m_workQueueOverride != nullptr)
    {
        m_keyframes.SetWorkQueue (m_workQueueOverride);
        BAIL_OUT_IF (true, S_OK);
    }

    if (!isCreated)
    {
        hr = m_workQueue.Create (KeyframeStore::kBufferCount);
        CHR (hr);
    }

    m_keyframes.SetWorkQueue (&m_workQueue);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FillResult
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::FillResult (ReverseResult & result) const
{
    const EmuCpu  * cpu = m_machine.GetCpu();



    result.position = m_machine.GetPosition();
    result.cycle    = (cpu != nullptr) ? cpu->GetTotalCycles() : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryFindStretch
//
//  The stretch that holds position: the one starting at the newest keyframe
//  at or before it. False when position is older than every keyframe.
//
////////////////////////////////////////////////////////////////////////////////

bool ReverseController::TryFindStretch (
    uint64_t    position,
    Stretch   & outStretch) const
{
    size_t  keyframe = 0;
    bool    isFound  = m_keyframes.TryFindByPosition (position, keyframe);



    if (isFound)
    {
        outStretch.keyframe = keyframe;
        outStretch.start    = m_keyframes.GetInfo (keyframe).position;
        outStretch.end      = GetSegmentEnd (keyframe);
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSegmentEnd
//
//  The last position a replay from keyframe can reach: the next keyframe,
//  or where recording paused before it, or, for the newest, the end of
//  history.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ReverseController::GetSegmentEnd (size_t keyframe) const
{
    uint64_t              end  = m_liveEndPosition;
    const KeyframeInfo  * next = nullptr;



    if (keyframe + 1 < m_keyframes.GetCount())
    {
        next = &m_keyframes.GetInfo (keyframe + 1);
        end  = next->hasGapBefore ? next->gapStart : next->position;
    }
    else if (m_isLive)
    {
        end = m_isPaused ? m_pauseStart : m_machine.GetPosition();
    }

    return end;
}





