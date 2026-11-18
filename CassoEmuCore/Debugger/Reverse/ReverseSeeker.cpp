#include "Pch.h"

#include "Debugger/Reverse/ReverseSeeker.h"
#include "Debugger/Reverse/HistorySpan.h"
#include "Debugger/Reverse/HistoryStatus.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Debugger/Reverse/IReverseStopTest.h"
#include "Debugger/Reverse/KeyframeCapturer.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Debugger/Reverse/ReplayControl.h"
#include "Debugger/Reverse/Replayer.h"
#include "Debugger/Reverse/ReplayStepTable.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSeeker::ReverseSeeker
//
////////////////////////////////////////////////////////////////////////////////

ReverseSeeker::ReverseSeeker (
    MachineHost       & machine,
    KeyframeStore     & keyframes,
    HistoryTimeline   & timeline,
    Replayer          & replayer,
    HistorySpan       & span,
    ReplayStepTable   & stepTable,
    KeyframeCapturer  & capturer) :
    m_machine   (machine),
    m_keyframes (keyframes),
    m_timeline  (timeline),
    m_replayer  (replayer),
    m_span      (span),
    m_stepTable (stepTable),
    m_capturer  (capturer)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSeeker::SeekToCycle
//
//  To the first instruction boundary at or after cycle, within history.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseSeeker::SeekToCycle (
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
//  ReverseSeeker::SeekToPosition
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseSeeker::SeekToPosition (
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
//  ReverseSeeker::StepForward
//
//  To the instruction after this one, replaying the recorded future. Live,
//  nothing is recorded ahead and the machine stays where it is.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseSeeker::StepForward (ReverseResult & result)
{
    return SeekToPosition (m_machine.GetRunner().GetPosition() + 1, result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSeeker::ReverseContinue
//
//  To the latest position before this one where the stop test fires. Each
//  stretch of history is replayed forward once with the stop test called
//  for every instruction, newest stretch first, and the latest hit wins; a
//  breakpoint lands before its instruction and a watchpoint after the
//  instruction that tripped it, as running forward would have. Nothing the
//  replay runs counts on the debugger's breakpoints or watchpoints. A
//  stretch the stop test rules out is passed over without a replay, so a
//  search through idle history costs little. Stopped, the run lands on the
//  oldest position it searched, after which no stop came.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseSeeker::ReverseContinue (
    IReverseStopTest  & stopTest,
    ReverseResult     & result)
{
    HRESULT         hr       = S_OK;
    uint64_t        from     = m_machine.GetRunner().GetPosition();
    uint64_t        end      = from;
    uint64_t        oldest   = 0;
    size_t          keyframe = 0;
    bool            isRuled  = false;
    bool            isRecord = m_span.IsRecording();
    Stretch         stretch;
    StretchSummary  summary;
    ReplayTarget    target;
    ReplayReport    report;



    CBRAEx (isRecord, E_UNEXPECTED);

    hr = LeaveLive();
    CHR (hr);

    oldest = GetOldestPosition();

    while (end > oldest)
    {
        if (IsStopDue (from, end))
        {
            hr = LandAt (end, ReverseOutcome::Stopped, result);
            CHR (hr);
            BAIL_OUT_IF (true, S_OK);
        }

        isRuled = m_keyframes.GetArena().TryFindByPosition (end - 1, keyframe) && m_timeline.TryGetSummary (keyframe, summary) && !stopTest.CanStopIn (summary);

        if (isRuled)
        {
            end = m_keyframes.GetArena().GetInfo (keyframe).position;
            continue;
        }

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
//  ReverseSeeker::Seek
//
//  A target in the stretch the machine is in, and ahead of it, is replayed
//  on from where the machine is; any other loads the keyframe at or before
//  it first. A target in a gap stops at its edge: the near edge going
//  forward from before it, the far one otherwise. Reaching the end of
//  history makes the machine live again. Keyframe indices are taken only
//  once nothing is in flight.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseSeeker::Seek (
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
    bool      isRecord  = m_span.IsRecording();
    Stretch   stretch;



    CBRA   (cpu);
    CBRAEx (isRecord, E_UNEXPECTED);

    isForward = isByCycle ? target.cycle >= cpu->GetHistoryCycles() : target.position >= m_machine.GetRunner().GetPosition();

    result.outcome = ReverseOutcome::Moved;

    if (isForward && m_span.IsLive())
    {
        FillResult (m_machine, result);
        BAIL_OUT_IF (true, S_OK);
    }

    hr = LeaveLive();
    CHR (hr);

    hr = m_keyframes.WaitForPending();
    CHR (hr);

    isHere = TryFindStretch (m_machine.GetRunner().GetPosition(), stretch);
    isHere = isHere && isForward && (isByCycle ? m_keyframes.GetArena().TryFindAtOrBefore (target.cycle, keyframe) : m_keyframes.GetArena().TryFindByPosition (target.position, keyframe));
    isHere = isHere && keyframe == stretch.keyframe && (isByCycle || target.position <= stretch.end);

    if (!isHere)
    {
        hr = RestoreAtOrBefore (target.position, target.cycle, isByCycle, stretch);
        CHR (hr);

        // Going forward into a gap: on to the keyframe where it ends.
        if (isForward && !isByCycle && target.position > stretch.end && stretch.keyframe + 1 < m_keyframes.GetArena().GetCount())
        {
            hr = m_replayer.RestoreKeyframe (stretch.keyframe + 1);
            CHR (hr);

            result.outcome = ReverseOutcome::AtHistoryGap;
            stretch.end    = m_machine.GetRunner().GetPosition();
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

    isShort = isByCycle ? cpu->GetHistoryCycles() < target.cycle : m_machine.GetRunner().GetPosition() < target.position;

    if (isShort && m_machine.GetRunner().GetPosition() < m_span.GetLeftAt())
    {
        result.outcome = ReverseOutcome::AtHistoryGap;
    }

    if (m_machine.GetRunner().GetPosition() >= m_span.GetLeftAt())
    {
        BecomeLive();
    }

    FillResult (m_machine, result);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSeeker::RestoreAtOrBefore
//
//  Loads the newest keyframe at or before the target, or the oldest keyframe
//  when the target is older than all history, and returns the stretch it
//  starts.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseSeeker::RestoreAtOrBefore (
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

    hasAny = m_keyframes.GetArena().GetCount() > 0;
    CBRAEx (hasAny, E_UNEXPECTED);

    isFound = isByCycle ? m_keyframes.GetArena().TryFindAtOrBefore (cycle, keyframe) : m_keyframes.GetArena().TryFindByPosition (position, keyframe);

    if (!isFound)
    {
        keyframe = 0;
    }

    hr = m_replayer.RestoreKeyframe (keyframe);
    CHR (hr);

    outStretch.keyframe = keyframe;
    outStretch.start    = m_keyframes.GetArena().GetInfo (keyframe).position;
    outStretch.end      = m_timeline.GetSegmentEnd (keyframe, GetRecordedEnd());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSeeker::HandleDivergence
//
//  A replay reached a keyframe and the machine did not hash to it, so the
//  history from the keyframe before it on cannot be trusted. The machine goes
//  back to that last good keyframe, and every keyframe and input after it is
//  dropped; the machine is live there. The host's connections stay open, so
//  the bytes they sent after the keyframe are queued for the guest again
//  before the journal is truncated, as a resume from the past does.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseSeeker::HandleDivergence (
    const ReplayReport  & report,
    ReverseResult       & result)
{
    HRESULT       hr      = S_OK;
    bool          hasGood = report.divergedKeyframe > 0;
    size_t        good    = 0;
    KeyframeInfo  info;



    CBRAEx (hasGood, E_UNEXPECTED);

    m_lastDivergence = report.divergence;
    m_cutCount++;

    good = report.divergedKeyframe - 1;
    info = m_keyframes.GetArena().GetInfo (good);

    hr = m_replayer.RestoreKeyframe (good);
    CHR (hr);

    hr = m_keyframes.DropAfterPosition (info.position);
    CHR (hr);

    m_machine.GetHostInput().ResyncConnections (info.journalIndex);
    m_machine.GetHostInput().GetJournal().Truncate (info.journalIndex);

    BecomeLive();

    FillResult (m_machine, result);
    result.outcome = ReverseOutcome::HistoryCut;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSeeker::OnMachineChanged
//
//  Something outside the recorded inputs changed the machine: the history
//  after it no longer follows from it, so it is dropped, and the changed
//  state becomes a boundary keyframe, which a replay arriving there loads,
//  since no replay from an earlier one would reproduce the change. The
//  machine is live again at its position. From the past, its connections on
//  the host are from later, so they are brought back in step with it ahead
//  of the keyframe and before the journal is cut, since the journal has what
//  the host sent after the point: one that matches the restored state stays
//  open, and what it sent after the point goes to the guest again. History
//  is dropped by position: a power cycle restarts the cycle counter, and
//  every keyframe from before it would look newer than the machine.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseSeeker::OnMachineChanged()
{
    HRESULT   hr         = S_OK;
    EmuCpu  * cpu        = m_machine.GetCpu();
    uint64_t  position   = m_machine.GetRunner().GetPosition();
    uint64_t  gapStart   = m_span.GetPauseStart();
    bool      isAfterGap = false;



    CBRA (cpu);
    BAIL_OUT_IF (!m_span.IsRecording(), S_OK);

    isAfterGap = m_span.IsPausedBefore (position);

    if (!m_span.IsLive())
    {
        m_machine.GetHostInput().ResyncConnections (m_replayer.GetJournalCursor());
        m_machine.GetHostInput().GetJournal().Truncate (m_replayer.GetJournalCursor());
    }

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

    hr = m_capturer.CaptureBoundary (isAfterGap, gapStart);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSeeker::ReplayHere
//
//  The machine is about to run the instruction at its position itself,
//  behind live: the debugger's step or run, or the emulator running on from
//  a point in history. That is replaying the recorded future, so the inputs
//  recorded there are applied first, as a replay applies them, and the
//  instruction then runs with the debug hook attached, so breakpoints stop
//  it as they stop a live run. A stretch that ends in a gap goes on at the
//  keyframe after it, and reaching the end of history makes the machine
//  live there. The machine runs at its pace here, so its sound plays as it
//  did live; the printer stays muted, since what it prints was printed live,
//  and the disk files are not touched, as in any replay. The stretch is
//  looked up only when the machine leaves the one it was in.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseSeeker::ReplayHere (bool isStarting)
{
    HRESULT        hr       = S_OK;
    uint64_t       position = m_machine.GetRunner().GetPosition();
    bool           isFound  = false;
    Stretch        stretch;
    ReplayReport   report;
    ReverseResult  cut;



    if (position >= m_span.GetLeftAt())
    {
        hr = m_replayer.PrepareStepHere (report, false);
        CHR (hr);

        BecomeLive();
        BAIL_OUT_IF (true, S_OK);
    }

    MarkReplaying();

    if (!m_span.IsHere (position))
    {
        isFound = TryFindStretch (position, stretch);
        CBRAEx (isFound, E_UNEXPECTED);

        if (position >= stretch.end && stretch.keyframe + 1 < m_keyframes.GetArena().GetCount())
        {
            hr = m_replayer.RestoreKeyframe (stretch.keyframe + 1);
            CHR (hr);

            position = m_machine.GetRunner().GetPosition();
            isFound  = TryFindStretch (position, stretch);
            CBRAEx (isFound, E_UNEXPECTED);
        }

        m_span.SetHere (stretch.start, std::max (stretch.end, stretch.start + 1));
    }

    hr = m_replayer.PrepareStepHere (report, isStarting);
    CHR (hr);

    //  A keyframe the step loaded was loaded muted, and gave the output back
    //  as a replay leaves it.
    MarkReplaying();

    if (report.isDiverged)
    {
        hr = HandleDivergence (report, cut);
        CHR (hr);
        BAIL_OUT_IF (true, S_OK);
    }

    if (m_machine.GetRunner().GetPosition() >= m_span.GetLeftAt())
    {
        BecomeLive();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSeeker::LeaveLive
//
//  Notes where history ends before the machine moves into it. A debugger
//  edit made live is stored first, so going back and then forward again
//  comes back to the edited machine. While paused, the position the machine
//  reached is stored first as a keyframe after the gap, so stepping back
//  from it, and coming back to it, work. From here every instruction start
//  reaches the history recorder, which is how a machine run from the past
//  without a seek is noticed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseSeeker::LeaveLive()
{
    HRESULT   hr       = S_OK;
    uint64_t  position = m_machine.GetRunner().GetPosition();
    bool      isLeft   = false;



    if (m_span.IsLive() && m_span.TakeEdit())
    {
        hr = OnMachineChanged();
        CHR (hr);
    }

    if (m_span.IsPausedBefore (position))
    {
        hr = m_capturer.CaptureBoundary (true, m_span.GetPauseStart());
        CHR (hr);
    }

    isLeft = m_span.Leave (position, m_machine.GetCpu()->GetHistoryCycles());

    if (isLeft)
    {
        m_machine.GetDiskStore().SetFlushHold (true);
    }

    if (m_onLiveChanged)
    {
        m_onLiveChanged (false);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSeeker::BecomeLive
//
//  The machine is at the end of history: the journal records again, or,
//  while paused, recording stops where the machine is. The automatic disk
//  flushes resume, the watch on input behind live comes off the devices,
//  and the step table goes. The live callback is called last.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseSeeker::BecomeLive()
{
    m_span.BecomeLive (m_machine.GetRunner().GetPosition());

    m_machine.GetDiskStore().SetFlushHold (false);
    m_machine.GetDiskStore().SetReplaying (false);
    m_machine.GetOutputMute().SetMuted (false);

    m_machine.SetHeldInputWatch (nullptr);

    m_stepTable.Discard();

    m_machine.GetHostInput().SetJournalOn (!m_span.IsPaused());

    if (m_onLiveChanged)
    {
        m_onLiveChanged (true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSeeker::LandAt
//
//  Seeks to position and reports outcome, unless the seek itself ended
//  otherwise. A landing outside the stretch the step table has discards it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseSeeker::LandAt (
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

    if (m_stepTable.HasSteps() && !m_stepTable.Contains (result.position))
    {
        m_stepTable.Discard();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSeeker::IsStopDue
//
//  A search for where a command lands, which began at from and has reached
//  back to reached, reports how much of history it has covered, and returns
//  whether the user asked it to stop.
//
////////////////////////////////////////////////////////////////////////////////

bool ReverseSeeker::IsStopDue (
    uint64_t  from,
    uint64_t  reached)
{
    uint64_t  oldest = GetOldestPosition();
    uint64_t  span   = (from > oldest) ? from - oldest : 0;
    uint64_t  done   = (from > reached) ? from - reached : 0;



    if (m_control == nullptr)
    {
        return false;
    }

    m_control->progress.store ((span > 0) ? (float) done / (float) span : 1.0f, std::memory_order_relaxed);

    return m_control->isStopRequested.load (std::memory_order_acquire);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSeeker::GetOldestPosition
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ReverseSeeker::GetOldestPosition() const
{
    return m_timeline.GetOldestPosition (m_machine.GetRunner().GetPosition());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSeeker::GetLiveEndPosition
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ReverseSeeker::GetLiveEndPosition() const
{
    return m_span.GetLiveEndPosition (m_machine.GetRunner().GetPosition());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSeeker::GetLiveEndCycle
//
//  The machine's cycle while live, since the span notes the cycle only on
//  the way out of live.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ReverseSeeker::GetLiveEndCycle() const
{
    const EmuCpu  * cpu = m_machine.GetCpu();



    return (cpu != nullptr) ? m_span.GetLiveEndCycle (cpu->GetHistoryCycles()) : m_span.GetLeftCycle();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSeeker::GetRecordedEnd
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ReverseSeeker::GetRecordedEnd() const
{
    return m_span.GetRecordedEnd (m_machine.GetRunner().GetPosition());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSeeker::FillResult
//
////////////////////////////////////////////////////////////////////////////////

void ReverseSeeker::FillResult (const MachineHost & machine, ReverseResult & result)
{
    const EmuCpu  * cpu = machine.GetCpu();



    result.position = machine.GetRunner().GetPosition();
    result.cycle    = (cpu != nullptr) ? cpu->GetHistoryCycles() : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSeeker::TryFindStretch
//
////////////////////////////////////////////////////////////////////////////////

bool ReverseSeeker::TryFindStretch (uint64_t position, Stretch & outStretch) const
{
    return m_timeline.TryFindStretch (position, GetRecordedEnd(), outStretch);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseSeeker::MarkReplaying
//
//  Behind live the printer is muted and the disk store is replaying, so
//  nothing the machine replays is printed or written again.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseSeeker::MarkReplaying()
{
    m_machine.GetOutputMute().SetMuted (false, true);
    m_machine.GetDiskStore().SetReplaying (true);
}
