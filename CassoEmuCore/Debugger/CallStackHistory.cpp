#include "Pch.h"

#include "Debugger/CallStackHistory.h"

#include "Debugger/DebugSession.h"
#include "Debugger/Reverse/ReverseController.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::CallStackHistory
//
////////////////////////////////////////////////////////////////////////////////

CallStackHistory::CallStackHistory (
    MachineHost   & machine,
    DebugSession  & session) :
    m_machine (machine),
    m_session (session)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::~CallStackHistory
//
////////////////////////////////////////////////////////////////////////////////

CallStackHistory::~CallStackHistory()
{
    Attach (nullptr, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::Attach
//
//  A rebuild under way was for the history and the rebuilder it started
//  with, so a change of either drops it. Linking history again does not ask
//  for a rebuild of the record already running: history that starts later
//  than that record holds less than it does.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::Attach (
    ReverseController    * history,
    ICallStackRebuilder  * rebuilder)
{
    if (history == m_history && rebuilder == m_rebuilder)
    {
        return;
    }

    Stop();

    m_history   = history;
    m_rebuilder = rebuilder;

    m_session.SetCallRebuildProgress (std::nullopt);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::OnMoving
//
//  Where the machine stands before a reverse command, and how much the
//  history's replayer has run on it, for OnMoved to compare with.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::OnMoving()
{
    MoveMark  mark;
    bool      isMarked = TryMarkMove (mark);



    m_moveMark.reset();

    if (isMarked)
    {
        m_moveMark = mark;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::OnMoved
//
//  The record the machine left behind was fed by the replay that moved it,
//  which loaded keyframes under it, so it starts again where the machine
//  landed, and is rebuilt from there. A command that left the machine where
//  it stood and replayed nothing on it -- a step back out with no caller,
//  or one that failed before it began -- fed the record nothing, and it goes
//  on. Without the mark OnMoving took there is no telling, and the record
//  starts again.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::OnMoved (bool isInterim)
{
    MoveMark  now;
    bool      isMarked = TryMarkMove (now);
    bool      isStill  = false;



    m_isDragging = isInterim;

    isStill = isMarked && m_moveMark.has_value() &&
              now.position == m_moveMark->position && now.cycle    == m_moveMark->cycle &&
              now.replayed == m_moveMark->replayed && now.restores == m_moveMark->restores;

    m_moveMark.reset();

    if (!isStill)
    {
        m_session.RestartCallRecording();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::Service
//
//  A record that started again since the last pass drops the rebuild under
//  way, and needs one of its own when it began mid-run: where the debugger
//  attached, or where a move through history landed. Then any rebuild that
//  has come in is taken in, and one due is asked for.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::Service()
{
    uint64_t                       record   = m_session.GetCallRecordGeneration();
    bool                           isOn     = m_session.IsCallRecording();
    bool                           isLinked = m_history != nullptr && m_rebuilder != nullptr && m_history->IsRecording();
    bool                           isMidRun = false;
    std::optional<CallStackBreak>  bottom;



    if (record != m_seenRecord)
    {
        Stop();

        bottom       = m_session.GetCallRecordBottom();
        isMidRun     = bottom.has_value() && (bottom->kind == CallBreakKind::TrackingBegan || bottom->kind == CallBreakKind::HistoryBegan);
        m_seenRecord = record;
        m_isDue      = isOn && isMidRun;
        m_began      = m_machine.GetPosition();
    }

    if (!isLinked || !isOn)
    {
        Stop();
    }
    else
    {
        TakeResults();

        if (m_isDue && !m_isDragging && m_awaited == 0)
        {
            Request();
        }
    }

    Publish();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::MakeJob
//
//  A part starts at the newest point the record would start again from
//  (FindFreshStart), or where the last job stopped, and at every keyframe
//  after it, up to the target, that a replay loads rather than reaches: one
//  after a change from outside, or after a gap, the part before which ends
//  where the gap began. A keyframe loaded at exactly the position a job
//  continues from is loaded again, which changes nothing if the last job
//  loaded it too. The inputs run from the first part's journal index, or
//  the last job's cursor, through the target; the disks are the ones in the
//  bays now.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CallStackHistory::MakeJob (
    bool                    isContinued,
    uint64_t                from,
    size_t                  journalCursor,
    uint64_t                target,
    CallStackRebuildJob   & outJob,
    bool                  & outHasJob)
{
    HRESULT                hr        = S_OK;
    KeyframeStore        * keyframes = nullptr;
    size_t                 count     = 0;
    size_t                 index     = 0;
    bool                   isFound   = false;
    bool                   isCopied  = false;
    CallStackRebuildPart   part;
    KeyframeInfo           info;



    outHasJob = false;
    outJob    = CallStackRebuildJob();

    CBRA (m_history);

    keyframes = &m_history->GetKeyframes();

    //  Keyframe indices are taken only once nothing is in flight.
    hr = keyframes->WaitForPending();
    CHR (hr);

    count = keyframes->GetCount();
    BAIL_OUT_IF (count == 0, S_OK);

    outJob.isContinued = isContinued;

    if (isContinued)
    {
        BAIL_OUT_IF (from >= target, S_OK);

        part               = CallStackRebuildPart();
        part.isLoaded      = false;
        part.startPosition = from;
        outJob.inputsFrom  = journalCursor;

        isFound = keyframes->TryFindByPosition (from, index);
        index   = !isFound ? 0 : (keyframes->GetInfo (index).position == from) ? index : index + 1;
    }
    else
    {
        index = FindFreshStart (*keyframes, target);
        info  = keyframes->GetInfo (index);

        BAIL_OUT_IF (info.position >= target, S_OK);

        hr = CopyPart (index, part);
        CHR (hr);

        outJob.inputsFrom = info.journalIndex;
        index++;
    }

    outJob.parts.push_back (part);

    for (; index < count; index++)
    {
        info = keyframes->GetInfo (index);

        if (info.position > target)
        {
            break;
        }

        if (!info.isBoundary && !info.hasGapBefore)
        {
            continue;
        }

        outJob.parts.back().endPosition = info.hasGapBefore ? (std::max) (info.gapStart, outJob.parts.back().startPosition) : info.position;

        hr = CopyPart (index, part);
        CHR (hr);

        outJob.parts.push_back (part);
    }

    outJob.parts.back().endPosition = target;

    isCopied = m_machine.GetInputJournal().TryCopyRecords (outJob.inputsFrom, target, outJob.inputs);
    BAIL_OUT_IF (!isCopied, S_OK);

    hr = m_diskCopier.Copy (m_machine.GetDiskStore(), outJob.disks);
    CHR (hr);

    outHasJob = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::Stop
//
//  Drops the rebuild under way, and any that was due.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::Stop()
{
    if (m_awaited != 0 && m_rebuilder != nullptr)
    {
        m_rebuilder->Cancel();
    }

    m_awaited       = 0;
    m_rebuildFrom   = 0;
    m_roundFrom     = 0;
    m_roundTo       = 0;
    m_continuations = 0;
    m_isDue         = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::Request
//
//  A rebuild from the newest point the record would start again from to
//  where the machine stands, unless history cannot reach there -- recording
//  paused at Maximum speed -- or the record it would give begins later than
//  the live one.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::Request()
{
    HRESULT                               hr         = S_OK;
    uint64_t                              target     = m_machine.GetPosition();
    uint64_t                              recorded   = m_history->GetRecordedEnd();
    uint64_t                              recordFrom = 0;
    bool                                  hasJob     = false;
    std::shared_ptr<CallStackRebuildJob>  job;



    m_isDue = false;

    BAIL_OUT_IF (target > recorded, S_OK);

    job = std::make_shared<CallStackRebuildJob>();
    CPRA (job);

    hr = MakeJob (false, 0, 0, target, *job, hasJob);
    CHR (hr);

    BAIL_OUT_IF (!hasJob, S_OK);

    recordFrom = GetRecordFrom (*job, 0);
    BAIL_OUT_IF (recordFrom > m_began, S_OK);

    job->generation = ++m_generation;

    hr = m_rebuilder->Submit (job);
    CHR (hr);

    m_awaited       = job->generation;
    m_rebuildFrom   = job->parts.front().startPosition;
    m_roundFrom     = m_rebuildFrom;
    m_roundTo       = target;
    m_continuations = 0;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::TakeResults
//
//  A result of any rebuild but the one awaited is stale.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::TakeResults()
{
    HRESULT                 hr      = S_OK;
    CallStackRebuildResult  result;
    bool                    isTaken = m_rebuilder->TryTakeResult (result);
    bool                    isOurs  = false;



    while (isTaken)
    {
        isOurs = m_awaited != 0 && result.generation == m_awaited;

        if (isOurs)
        {
            hr = Land (result);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        isTaken = m_rebuilder->TryTakeResult (result);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::Land
//
//  A rebuild that came in where the machine stands is taken as the record;
//  one the machine has run on from is continued. A failed one, and one
//  that had to begin later than the live record began, leave the live
//  record as it is.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CallStackHistory::Land (const CallStackRebuildResult & result)
{
    HRESULT   hr          = S_OK;
    uint64_t  position    = m_machine.GetPosition();
    bool      isUseful    = false;
    bool      isInstalled = false;



    m_awaited = 0;

    CHR (result.hr);

    isUseful = result.recordFrom <= m_began && result.position <= position;
    BAIL_OUT_IF (!isUseful, S_OK);

    if (result.position == position)
    {
        isInstalled = TryInstall (result);
        IGNORE_RETURN_VALUE (isInstalled, false);
        BAIL_OUT_IF (true, S_OK);
    }

    hr = Continue (result);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::Continue
//
//  From where the rebuild stopped to where the machine now stands: here, at
//  once, when that is close, and on the rebuilder's worker otherwise, after
//  which this runs again, closer each time.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CallStackHistory::Continue (const CallStackRebuildResult & result)
{
    HRESULT                               hr          = S_OK;
    const EmuCpu                        * cpu         = m_machine.GetCpu();
    uint64_t                              target      = m_machine.GetPosition();
    uint64_t                              recorded    = m_history->GetRecordedEnd();
    uint64_t                              cycle       = 0;
    uint64_t                              limit       = 0;
    uint64_t                              recordFrom  = 0;
    bool                                  hasJob      = false;
    bool                                  isClose     = false;
    bool                                  isInstalled = false;
    std::shared_ptr<CallStackRebuildJob>  job;
    CallStackRebuildResult                caught;



    CBRA (cpu);

    BAIL_OUT_IF (target > recorded, S_OK);

    job = std::make_shared<CallStackRebuildJob>();
    CPRA (job);

    hr = MakeJob (true, result.position, result.journalCursor, target, *job, hasJob);
    CHR (hr);

    BAIL_OUT_IF (!hasJob, S_OK);

    recordFrom = GetRecordFrom (*job, result.recordFrom);
    BAIL_OUT_IF (recordFrom > m_began, S_OK);

    job->generation = result.generation;

    cycle   = cpu->GetTotalCycles();
    limit   = (m_continuations > 0) ? kLateCatchUpCycles : kCatchUpCycles;
    isClose = cycle >= result.cycle && cycle - result.cycle <= limit;

    if (isClose)
    {
        hr = m_rebuilder->Rebuild (*job, caught);
        CHR (hr);

        BAIL_OUT_IF (caught.recordFrom > m_began, S_OK);

        isInstalled = TryInstall (caught);
        IGNORE_RETURN_VALUE (isInstalled, false);
        BAIL_OUT_IF (true, S_OK);
    }

    hr = m_rebuilder->Submit (job);
    CHR (hr);

    m_awaited   = job->generation;
    m_roundFrom = result.position;
    m_roundTo   = target;

    m_continuations++;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::TryInstall
//
//  Only a record of the machine as it stands, to the cycle and every
//  register, is taken: a second machine that stopped anywhere else replayed
//  some other run.
//
////////////////////////////////////////////////////////////////////////////////

bool CallStackHistory::TryInstall (const CallStackRebuildResult & result)
{
    const EmuCpu      * cpu       = m_machine.GetCpu();
    Cpu6502Registers    registers = {};
    bool                isSame    = false;



    if (cpu == nullptr)
    {
        return false;
    }

    registers = cpu->GetCpu6502()->GetRegisters();
    isSame    = result.position == m_machine.GetPosition() && result.cycle == cpu->GetTotalCycles() &&
                AreSameRegisters (result.registers, registers);

    if (isSame)
    {
        m_session.AdoptCallRecord (result.record);
    }

    return isSame;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::CopyPart
//
//  The part that starts at keyframe index, its keyframe copied out still
//  packed; its end is the caller's to set.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CallStackHistory::CopyPart (
    size_t                   index,
    CallStackRebuildPart   & outPart)
{
    HRESULT               hr   = S_OK;
    const KeyframeInfo  & info = m_history->GetKeyframes().GetInfo (index);
    KeyframeCopy          copy = KeyframeCopy::Gone;



    outPart = CallStackRebuildPart();

    hr = m_history->GetKeyframes().CopyPacked (info.position, m_noUnpacker, outPart.start, copy);
    CHR (hr);

    CBRA (copy == KeyframeCopy::Copied);

    outPart.isLoaded      = true;
    outPart.isAfterGap    = info.hasGapBefore;
    outPart.startPosition = info.position;
    outPart.startCycle    = info.cycle;
    outPart.journalIndex  = info.journalIndex;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::FindFreshStart
//
//  The keyframe a fresh job starts at: the newest point before target that
//  the record starts again from, whatever ran before it. That is the newest
//  keyframe after a gap, since what ran in the gap is not in history, or the
//  oldest keyframe when there is none; then, after it, the newest keyframe
//  taken before the last power cycle short of target, since a power cycle
//  voids the whole record and dates it again. A reset is not such a point:
//  the record goes on dating from where it began.
//
////////////////////////////////////////////////////////////////////////////////

size_t CallStackHistory::FindFreshStart (
    const KeyframeStore  & keyframes,
    uint64_t               target) const
{
    const InputJournal  & journal  = m_machine.GetInputJournal();
    const InputRecord   * record   = nullptr;
    size_t                count    = keyframes.GetCount();
    size_t                end      = journal.GetEndIndex();
    size_t                start    = 0;
    size_t                index    = 0;
    size_t                cycled   = 0;
    bool                  isCycled = false;



    for (index = 1; index < count && keyframes.GetInfo (index).position <= target; index++)
    {
        if (keyframes.GetInfo (index).hasGapBefore)
        {
            start = index;
        }
    }

    for (index = (std::max) (keyframes.GetInfo (start).journalIndex, journal.GetBeginIndex()); index < end; index++)
    {
        record = &journal.GetRecord (index);

        if (record->position >= target)
        {
            break;
        }

        if (record->kind == InputKind::PowerCycle)
        {
            cycled   = index;
            isCycled = true;
        }
    }

    //  A keyframe taken before the power cycle was journaled holds an
    //  index no greater than the power cycle's.
    for (index = start + 1; isCycled && index < count && keyframes.GetInfo (index).journalIndex <= cycled; index++)
    {
        start = index;
    }

    return start;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::TryMarkMove
//
//  False without history or a CPU to read.
//
////////////////////////////////////////////////////////////////////////////////

bool CallStackHistory::TryMarkMove (MoveMark & outMark) const
{
    const EmuCpu  * cpu      = m_machine.GetCpu();
    bool            isMarked = m_history != nullptr && cpu != nullptr;



    outMark = MoveMark();

    if (isMarked)
    {
        outMark.position = m_machine.GetPosition();
        outMark.cycle    = cpu->GetTotalCycles();
        outMark.replayed = m_history->GetReplayer().GetReplayedCount();
        outMark.restores = m_history->GetReplayer().GetRestoreCount();
    }

    return isMarked;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::Publish
//
//  The session shows a rebuild under way, or one that waits for a drag of
//  the timeline to end, and how far along it is: of the whole stretch from
//  where it started replaying to where the round on the worker ends, the
//  share replayed, counting the rounds before this one as done.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::Publish()
{
    std::optional<float>  progress;
    double                ofRound  = 0.0;
    double                done     = 0.0;
    double                span     = 0.0;



    if (m_awaited != 0)
    {
        ofRound  = m_rebuilder->GetProgress();
        done     = (double) (m_roundFrom - m_rebuildFrom) + ofRound * (double) (m_roundTo - m_roundFrom);
        span     = (double) (m_roundTo - m_rebuildFrom);
        progress = (float) ((span > 0.0) ? done / span : ofRound);
    }
    else if (m_isDue)
    {
        progress = 0.0f;
    }

    m_session.SetCallRebuildProgress (progress);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::GetRecordFrom
//
//  Where the record a job gives begins: at its first part, or where the
//  record it continues began, unless a part after a gap begins it again.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t CallStackHistory::GetRecordFrom (
    const CallStackRebuildJob  & job,
    uint64_t                     ongoing)
{
    uint64_t  from = job.isContinued ? ongoing : job.parts.front().startPosition;



    for (const CallStackRebuildPart & part : job.parts)
    {
        if (part.isAfterGap)
        {
            from = part.startPosition;
        }
    }

    return from;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::AreSameRegisters
//
////////////////////////////////////////////////////////////////////////////////

bool CallStackHistory::AreSameRegisters (
    const Cpu6502Registers  & a,
    const Cpu6502Registers  & b)
{
    return a.pc == b.pc && a.sp == b.sp && a.a == b.a && a.x == b.x && a.y == b.y && a.p == b.p;
}





