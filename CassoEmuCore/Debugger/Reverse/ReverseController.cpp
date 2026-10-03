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
    m_replayer (machine, m_keyframes, m_ring)
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
//  Turns the input journal on and takes the first keyframe and checkpoint at
//  once, so history begins at the moment the journal attaches. The disks are
//  machine state from here: the automatic flushes are held, and a disk that
//  leaves its bay is kept in memory while history may put it back.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::Start (const ReverseSettings & settings)
{
    HRESULT   hr  = S_OK;
    EmuCpu  * cpu = m_machine.GetCpu();



    CBRA (cpu);

    m_keyframes.Configure (settings.keyframes);
    m_ring.Configure      (settings.ring, settings.keyframes.intervalCycles);

    m_machine.GetInputJournal().Clear();
    m_machine.SetInputJournalOn  (true);
    m_machine.SetHistoryRecorder (this);

    m_machine.GetDiskStore().SetFlushHold      (true);
    m_machine.GetDiskStore().SetMediaRetention (true);

    m_isRecording = true;
    m_isLive      = true;

    hr = CaptureNow (true, true);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Stop
//
//  Detaches from the machine and drops all history. The disks keep any writes
//  they hold, which the next flush writes as they stand.
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

    m_keyframes.Clear();
    m_ring.Clear();

    m_isRecording  = false;
    m_isLive       = true;
    m_nextDueCycle = 0;
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
    HRESULT   hr       = S_OK;
    EmuCpu  * cpu      = m_machine.GetCpu();
    uint64_t  cycle    = 0;
    uint64_t  position = m_machine.GetPosition();



    CBRA (cpu);
    BAIL_OUT_IF (!m_isRecording, S_OK);

    cycle = cpu->GetTotalCycles();

    if (!m_isLive)
    {
        m_machine.GetInputJournal().Truncate (m_replayer.GetJournalCursor());
    }

    if (cycle == 0)
    {
        m_keyframes.Clear();
    }
    else
    {
        hr = m_keyframes.TruncateAfter (cycle - 1);
        CHR (hr);
    }

    m_ring.TruncateFrom (position);

    BecomeLive();

    hr = CaptureNow (true, true);
    CHR (hr);

    m_keyframes.MarkNewestBoundary();

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
//  StepBack
//
//  To the instruction before this one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::StepBack (ReverseResult & result)
{
    uint64_t  position = m_machine.GetPosition();
    bool      hasPrior = position > GetOldestPosition();



    return LandAt (hasPrior ? position - 1 : GetOldestPosition(), hasPrior, result);
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
    HRESULT   hr      = S_OK;
    bool      isFound = false;
    uint64_t  target  = 0;



    hr = FindStepOverTarget (m_machine.GetPosition(), isFound, target);
    CHR (hr);

    hr = LandAt (target, isFound, result);
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
    HRESULT   hr      = S_OK;
    bool      isFound = false;
    uint64_t  target  = 0;



    hr = FindStepOutTarget (m_machine.GetPosition(), isFound, target);
    CHR (hr);

    hr = LandAt (target, isFound, result);
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
//  stretch of history is replayed forward with the stop test asked about
//  every instruction, newest stretch first, and the latest hit wins; a
//  breakpoint lands before its instruction and a watchpoint after the
//  instruction that tripped it, as running forward would have.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::ReverseContinue (
    IReverseStopTest  & stopTest,
    ReverseResult     & result)
{
    HRESULT       hr      = S_OK;
    uint64_t      from    = m_machine.GetPosition();
    uint64_t      end     = from;
    uint64_t      start   = 0;
    uint64_t      oldest  = GetOldestPosition();
    ReplayTarget  target;
    ReplayReport  report;



    while (end > oldest)
    {
        LeaveLive();

        hr = RestoreAtOrBefore (end - 1, 0, false);
        CHR (hr);

        start           = m_machine.GetPosition();
        target.position = end;

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
            hr = LandAt (report.lastHit, true, result);
            CHR (hr);
            BAIL_OUT_IF (true, S_OK);
        }

        end = start;
    }

    hr = LandAt (oldest, false, result);
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
    uint64_t  oldest = m_machine.GetPosition();



    if (m_keyframes.GetCount() > 0)
    {
        oldest = m_keyframes.GetInfo (0).position;
    }
    else if (m_ring.GetCheckpointCount() > 0)
    {
        oldest = m_ring.GetCheckpoint (0).position;
    }

    return oldest;
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
//  A target behind the machine loads the nearest snapshot at or before it
//  first; one ahead replays on from where the machine stands. Reaching the
//  end of history makes the machine live again.
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



    CBRA (cpu);
    CBRAEx (m_isRecording, E_UNEXPECTED);

    isForward = isByCycle ? target.cycle >= cpu->GetTotalCycles() : target.position >= m_machine.GetPosition();

    result.outcome = ReverseOutcome::Moved;

    if (isForward && m_isLive)
    {
        FillResult (result);
        BAIL_OUT_IF (true, S_OK);
    }

    LeaveLive();

    if (!isForward)
    {
        hr = RestoreAtOrBefore (target.position, target.cycle, isByCycle);
        CHR (hr);
    }

    hr = m_replayer.RunTo (target, m_liveEndPosition, stopTest, report);
    CHR (hr);

    if (report.isDiverged)
    {
        hr = HandleDivergence (report, result);
        CHR (hr);
        BAIL_OUT_IF (true, S_OK);
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
//  Loads whichever is later of the newest ring checkpoint and the newest
//  keyframe at or before the target, or the oldest keyframe when the target
//  is older than all history.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::RestoreAtOrBefore (
    uint64_t  position,
    uint64_t  cycle,
    bool      isByCycle)
{
    HRESULT   hr            = S_OK;
    size_t    checkpoint    = 0;
    size_t    keyframe      = 0;
    bool      hasCheckpoint = isByCycle ? m_ring.TryFindCheckpointByCycle (cycle, checkpoint) : m_ring.TryFindCheckpointAtOrBefore (position, checkpoint);
    bool      hasKeyframe   = isByCycle ? m_keyframes.TryFindAtOrBefore (cycle, keyframe)    : TryFindKeyframeAtOrBefore (position, keyframe);
    bool      hasAny        = m_keyframes.GetCount() > 0;
    bool      useCheckpoint = false;



    CBRAEx (hasAny, E_UNEXPECTED);

    useCheckpoint = hasCheckpoint && (!hasKeyframe || m_ring.GetCheckpoint (checkpoint).position >= m_keyframes.GetInfo (keyframe).position);

    if (useCheckpoint)
    {
        hr = m_replayer.RestoreCheckpoint (checkpoint);
        CHR (hr);
    }
    else
    {
        hr = m_replayer.RestoreKeyframe (hasKeyframe ? keyframe : 0);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleDivergence
//
//  A replay reached a keyframe and the machine did not hash to it, so the
//  history from the keyframe before it on cannot be trusted. The machine goes
//  back to that last good keyframe, and every keyframe, input and ring entry
//  after it is dropped; the machine is live there.
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

    hr = m_keyframes.TruncateAfter (info.cycle);
    CHR (hr);

    m_machine.GetInputJournal().Truncate (info.journalIndex);
    m_ring.Clear();

    BecomeLive();

    FillResult (result);
    result.outcome = ReverseOutcome::HistoryCut;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRecord
//
//  The registers the instruction at position began with. A position older
//  than the ring is rebuilt by replaying from the snapshot before it up to
//  just past it, which leaves the machine there; the ring then holds every
//  position from that snapshot on, which is what a scan going backward reads
//  next.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::GetRecord (
    uint64_t      position,
    UndoRecord  & outRecord)
{
    HRESULT       hr     = S_OK;
    bool          isHeld = m_ring.TryGetRecord (position, outRecord);
    ReplayTarget  target;
    ReplayReport  report;



    BAIL_OUT_IF (isHeld, S_OK);

    LeaveLive();

    hr = RestoreAtOrBefore (position, 0, false);
    CHR (hr);

    target.position = position + 1;

    hr = m_replayer.RunTo (target, m_liveEndPosition, nullptr, report);
    CHR (hr);

    CBREx (!report.isDiverged, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    isHeld = m_ring.TryGetRecord (position, outRecord);
    CBRA (isHeld);

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
//  over.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::FindStepOverTarget (
    uint64_t    current,
    bool      & outFound,
    uint64_t  & outTarget)
{
    constexpr int  kReturnDepth = 2;
    HRESULT        hr           = S_OK;
    uint64_t       oldest       = GetOldestPosition();
    int            spNow        = m_machine.GetCpu()->GetSP();
    UndoRecord     record;
    uint64_t       q            = 0;
    bool           isReturn     = false;



    outFound = false;

    BAIL_OUT_IF (current <= oldest, S_OK);

    hr = GetRecord (current - 1, record);
    CHR (hr);

    isReturn = spNow - static_cast<int> (record.sp) >= kReturnDepth;

    if (!isReturn)
    {
        outFound  = true;
        outTarget = current - 1;
        BAIL_OUT_IF (true, S_OK);
    }

    for (q = current - 1; q > oldest && !outFound; q--)
    {
        hr = GetRecord (q - 1, record);
        CHR (hr);

        if (static_cast<int> (record.sp) >= spNow)
        {
            outFound  = true;
            outTarget = q - 1;
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
//  returned left a shallower stack behind it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::FindStepOutTarget (
    uint64_t    current,
    bool      & outFound,
    uint64_t  & outTarget)
{
    constexpr int  kCallDepth      = 2;
    constexpr int  kInterruptDepth = 3;
    HRESULT        hr              = S_OK;
    uint64_t       oldest          = GetOldestPosition();
    int            spNext          = m_machine.GetCpu()->GetSP();
    int            ceiling         = spNext;
    int            pushed          = 0;
    UndoRecord     record;
    uint64_t       q               = 0;



    outFound = false;

    for (q = current; q > oldest && !outFound; q--)
    {
        hr = GetRecord (q - 1, record);
        CHR (hr);

        pushed = static_cast<int> (record.sp) - spNext;

        if ((pushed == kCallDepth || pushed == kInterruptDepth) && spNext >= ceiling)
        {
            outFound  = true;
            outTarget = q - 1;
        }

        spNext  = record.sp;
        ceiling = std::max (ceiling, spNext);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LandAt
//
//  Seeks to position; when the command found no target, that is the oldest
//  position and the outcome is AtHistoryStart.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::LandAt (
    uint64_t         position,
    bool             isFound,
    ReverseResult  & result)
{
    HRESULT  hr = S_OK;



    hr = SeekToPosition (position, result);
    CHR (hr);

    if (!isFound && result.outcome == ReverseOutcome::Moved)
    {
        result.outcome = ReverseOutcome::AtHistoryStart;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureNow
//
//  Saves the machine once and hands the state to the keyframe store, the
//  ring, or both. Keyframes dropped over budget take their journal records
//  with them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::CaptureNow (
    bool  takeKeyframe,
    bool  takeCheckpoint)
{
    HRESULT          hr         = S_OK;
    EmuCpu         * cpu        = m_machine.GetCpu();
    InputJournal   & journal    = m_machine.GetInputJournal();
    uint64_t         position   = m_machine.GetPosition();
    uint64_t         cycle      = 0;
    size_t           journalEnd = 0;
    std::string      hostState;
    StateWriter    & writer     = m_writer;



    CBRA (cpu);
    BAIL_OUT_IF (!takeKeyframe && !takeCheckpoint, S_OK);

    cycle = cpu->GetTotalCycles();

    // A host write no read has seen yet is in the snapshot; the sync record
    // puts it in front of any replay that crosses this point as well.
    hr = m_machine.SaveHostInputState (hostState);
    CHR (hr);

    m_machine.RecordInput (InputKind::HostState, 0, 0, hostState);

    journalEnd = journal.GetEndIndex();

    // A keyframe needs the whole blob in one buffer; a checkpoint alone
    // shares the disk tracks that have not changed since the last one.
    writer.Reuse      (m_ring.TakeSpareBuffer());
    writer.SetSharing (!takeKeyframe);

    hr = m_machine.SaveState (writer);
    CHR (hr);

    if (takeKeyframe)
    {
        hr = m_keyframes.Add (position, cycle, journalEnd, writer.GetBytes());
        CHR (hr);

        journal.DiscardBefore (m_keyframes.GetInfo (0).journalIndex);
    }

    if (takeCheckpoint)
    {
        hr = m_ring.AddCheckpoint (position, cycle, journalEnd, writer.TakeBytes(), writer.TakeSegments());
        CHR (hr);
    }

    PruneRetainedMedia();

Error:
    ScheduleCaptures();

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCaptureDue
//
//  A keyframe or checkpoint fell due, or the machine is in history, where
//  every instruction start comes here. During a replay the replayer takes
//  the ring's checkpoints and the keyframes exist already. A machine stepped
//  from the past by anything but a replay is running live from there, so the
//  recorded future goes first.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::OnCaptureDue (uint64_t cycle)
{
    HRESULT  hr = S_OK;



    if (m_replayer.IsReplaying())
    {
        return;
    }

    if (!m_isLive)
    {
        hr = OnMachineChanged();
    }
    else
    {
        hr = CaptureNow (m_keyframes.IsDue (cycle), m_ring.IsCheckpointDue (cycle));
    }

    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LeaveLive
//
//  Remembers where history ends before the machine moves into it. From here
//  every instruction start reaches OnCaptureDue, which is how a machine run
//  from the past without a seek is noticed.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::LeaveLive()
{
    if (m_isLive)
    {
        m_liveEndPosition = m_machine.GetPosition();
        m_liveEndCycle    = m_machine.GetCpu()->GetTotalCycles();
        m_isLive          = false;
    }

    m_nextDueCycle = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BecomeLive
//
//  The machine is at the end of history: the journal records again and the
//  snapshots fall due on their own schedule.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::BecomeLive()
{
    m_isLive = true;
    m_machine.SetInputJournalOn (true);

    ScheduleCaptures();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScheduleCaptures
//
//  The next cycle at which a keyframe or a checkpoint falls due, or zero in
//  history, so every instruction start is checked there.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::ScheduleCaptures()
{
    m_nextDueCycle = m_isLive ? std::min (m_keyframes.GetNextDueCycle(), m_ring.GetNextCheckpointCycle()) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PruneRetainedMedia
//
//  Lets the disk store drop the disks that left their bays before every
//  snapshot still held.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseController::PruneRetainedMedia()
{
    uint64_t  oldest    = 0;
    bool      hasOldest = false;



    if (m_keyframes.GetCount() > 0)
    {
        oldest    = m_keyframes.GetInfo (0).position;
        hasOldest = true;
    }

    if (m_ring.GetCheckpointCount() > 0 && (!hasOldest || m_ring.GetCheckpoint (0).position < oldest))
    {
        oldest    = m_ring.GetCheckpoint (0).position;
        hasOldest = true;
    }

    if (hasOldest)
    {
        m_machine.GetDiskStore().PruneRetainedMedia (oldest);
    }
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
//  TryFindKeyframeAtOrBefore
//
//  The newest keyframe whose position is at or before position.
//
////////////////////////////////////////////////////////////////////////////////

bool ReverseController::TryFindKeyframeAtOrBefore (
    uint64_t   position,
    size_t   & outIndex) const
{
    size_t  low   = 0;
    size_t  high  = m_keyframes.GetCount();
    size_t  mid   = 0;
    bool    found = false;



    while (low < high)
    {
        mid = low + (high - low) / 2;

        if (m_keyframes.GetInfo (mid).position <= position)
        {
            low = mid + 1;
        }
        else
        {
            high = mid;
        }
    }

    found = low > 0;

    if (found)
    {
        outIndex = low - 1;
    }

    return found;
}
