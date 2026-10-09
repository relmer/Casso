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
//  A rebuild under way, or due, was for the history and the rebuilder it
//  started with, so a change of either gives it up. Linking history again
//  does not request a rebuild of the record already running: history that
//  starts later than that record holds less than it does. The copies of the
//  record were of the old history and go; the new one's keyframes, as they
//  are taken and dropped, are followed from here.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::Attach (
    ReverseController    * history,
    ICallStackRebuilder  * rebuilder)
{
    bool  isOwed = m_isDue || m_awaited != 0;



    if (history == m_history && rebuilder == m_rebuilder)
    {
        return;
    }

    if (isOwed)
    {
        Abandon();
    }
    else
    {
        Stop();
    }

    if (history != m_history)
    {
        if (m_history != nullptr)
        {
            m_history->SetKeyframeListener (this, nullptr);
            m_history->GetKeyframes().SetDropListener (this, nullptr);
        }

        m_copies.Clear();

        if (history != nullptr)
        {
            history->SetKeyframeListener (this, [this] (uint64_t position) { OnKeyframeTaken (position); });
            history->GetKeyframes().SetDropListener (this, [this] (KeyframeDrop drop) { OnKeyframeDrop (drop); });
        }
    }

    m_history      = history;
    m_rebuilder    = rebuilder;
    m_restoresSeen = GetRestoreCount();

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
//  starts again, as it does when a keyframe was loaded under the running
//  machine before the command and no pass has followed since (CheckLoads).
//  Either way, the keyframes loaded so far are accounted for here.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::OnMoved (bool isInterim)
{
    MoveMark  now;
    bool      isMarked = TryMarkMove (now);
    bool      isStill  = false;



    m_isDragging = isInterim;

    isStill = isMarked && m_moveMark.has_value() && m_moveMark->restores == m_restoresSeen &&
              now.position == m_moveMark->position && now.cycle    == m_moveMark->cycle &&
              now.replayed == m_moveMark->replayed && now.restores == m_moveMark->restores;

    m_moveMark.reset();

    m_restoresSeen = GetRestoreCount();

    if (!isStill)
    {
        m_session.RestartCallRecording();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::Service
//
//  A keyframe loaded under the running machine starts the record again
//  (CheckLoads), and a record that started again since the last pass is
//  looked at (OnRecordChanged). Without history to rebuild from, a rebuild
//  due or under way is given up; with it, any rebuild that has come in is
//  taken in, and one due is requested.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::Service()
{
    uint64_t  record   = 0;
    bool      isOn     = m_session.IsCallRecording();
    bool      isLinked = m_history != nullptr && m_rebuilder != nullptr && m_history->IsRecording();
    bool      isOwed   = false;



    CheckLoads();

    record = m_session.GetCallRecordGeneration();

    if (record != m_seenRecord)
    {
        OnRecordChanged (record, isOn);
    }

    if (!isLinked || !isOn)
    {
        isOwed = isOn && (m_isDue || m_awaited != 0);

        if (isOwed)
        {
            Abandon();
        }
        else
        {
            Stop();
        }
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
//  A fresh job starts at the newest copy of the record at or after the
//  newest point the record would start again from (FindFreshStart), seeded
//  with the copy, or at that point itself; a job that continues starts where
//  the last job stopped. A part starts there and at every keyframe after
//  it, up to the target, that a replay loads rather than reaches: one after
//  a change from outside. A keyframe loaded at exactly the position a job
//  continues from is loaded again, which changes nothing if the last job
//  loaded it too. Every keyframe from the start to the target without a copy
//  of the record is listed for the replay to make one. The inputs run from
//  the first part's journal index, or the last job's cursor, through the
//  target; the disks are the ones in the bays now.
//
//  No part after the first follows a gap: a fresh job starts after the last
//  one up to the target, and for a job that continues, a record begun again
//  after one would hold less than the live record, so there is no job.
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
    size_t                 start     = 0;
    bool                   isFound   = false;
    bool                   isSeeded  = false;
    bool                   isCopied  = false;
    std::span<const Byte>  seed;
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
        start    = FindFreshStart (*keyframes, target);
        info     = keyframes->GetInfo (start);
        isSeeded = TryFindSeed (*keyframes, info.position, target, index);

        BAIL_OUT_IF (!isSeeded && info.position >= target, S_OK);

        if (!isSeeded)
        {
            index = start;
            AddCopyAt (info, outJob);
        }

        hr = CopyPart (index, part);
        CHR (hr);

        if (isSeeded)
        {
            seed = m_copies.GetPacked (part.startPosition);

            part.seed.assign (seed.begin(), seed.end());
            part.seedFrom = info.position;
        }

        outJob.inputsFrom = part.journalIndex;
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

        BAIL_OUT_IF (info.hasGapBefore, S_OK);

        AddCopyAt (info, outJob);

        if (!info.isBoundary)
        {
            continue;
        }

        outJob.parts.back().endPosition = info.position;

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
//  Drops the rebuild under way, any that was due, and the copies it made.
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

    m_pendingCopies.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::Abandon
//
//  No rebuild will replace the record: the one due or under way is dropped,
//  and a record begun again after a move marks only that the calls before
//  it are not available, since history does not start where it began.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::Abandon()
{
    Stop();

    m_session.MarkCallRecordUnrebuilt();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::Request
//
//  A rebuild to where the machine stands. Where the machine stands at the
//  newest point the record would start again from, the record begun there
//  is the one a rebuild would give, and none is needed. Where history cannot
//  reach the machine -- recording paused at Maximum speed -- or the record a
//  rebuild would give begins later than the live one, none can help, and the
//  record is given up on.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::Request()
{
    HRESULT                               hr         = S_OK;
    KeyframeStore                       & keyframes  = m_history->GetKeyframes();
    uint64_t                              target     = m_machine.GetPosition();
    uint64_t                              recorded   = m_history->GetRecordedEnd();
    uint64_t                              recordFrom = 0;
    size_t                                start      = 0;
    bool                                  hasJob     = false;
    bool                                  isAtStart  = false;
    bool                                  isSettled  = false;
    std::shared_ptr<CallStackRebuildJob>  job;



    m_isDue = false;

    BAIL_OUT_IF (target > recorded, S_OK);

    hr = keyframes.WaitForPending();
    CHR (hr);

    if (keyframes.GetCount() > 0)
    {
        start     = FindFreshStart (keyframes, target);
        isAtStart = keyframes.GetInfo (start).position == target;
    }

    if (isAtStart)
    {
        NoteWhole();
        isSettled = true;
        BAIL_OUT_IF (true, S_OK);
    }

    job = std::make_shared<CallStackRebuildJob>();
    CPRA (job);

    hr = MakeJob (false, 0, 0, target, *job, hasJob);
    CHR (hr);

    BAIL_OUT_IF (!hasJob, S_OK);

    recordFrom = GetRecordFrom (job->parts.front());
    BAIL_OUT_IF (recordFrom > m_began, S_OK);

    job->generation = ++m_generation;

    hr = m_rebuilder->Submit (job);
    CHR (hr);

    m_awaited       = job->generation;
    m_rebuildFrom   = job->parts.front().startPosition;
    m_roundFrom     = m_rebuildFrom;
    m_roundTo       = target;
    m_continuations = 0;
    isSettled       = true;

Error:
    if (!isSettled)
    {
        Abandon();
    }

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
//  one the machine has run on from is continued. Its copies wait until its
//  record is taken. A failed one, and one that had to begin later than the
//  live record began, leave the live record as it is, given up on.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CallStackHistory::Land (CallStackRebuildResult & result)
{
    HRESULT   hr        = S_OK;
    uint64_t  position  = m_machine.GetPosition();
    bool      isUseful  = false;
    bool      isSettled = false;



    m_awaited = 0;

    CHR (result.hr);

    KeepCopies (result.copies);

    isUseful = result.recordFrom <= m_began && result.position <= position;
    BAIL_OUT_IF (!isUseful, S_OK);

    if (result.position == position)
    {
        isSettled = TryInstall (result);
        BAIL_OUT_IF (true, S_OK);
    }

    hr = Continue (result, isSettled);
    CHR (hr);

Error:
    if (!isSettled)
    {
        Abandon();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::Continue
//
//  From where the rebuild stopped to where the machine now stands: here, at
//  once, when that is close, and on the rebuilder's worker otherwise, after
//  which this runs again, closer each time. outIsSettled when the record was
//  taken or the next round is under way.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CallStackHistory::Continue (
    const CallStackRebuildResult  & result,
    bool                          & outIsSettled)
{
    HRESULT                               hr       = S_OK;
    const EmuCpu                        * cpu      = m_machine.GetCpu();
    uint64_t                              target   = m_machine.GetPosition();
    uint64_t                              recorded = m_history->GetRecordedEnd();
    uint64_t                              cycle    = 0;
    uint64_t                              limit    = 0;
    bool                                  hasJob   = false;
    bool                                  isClose  = false;
    std::shared_ptr<CallStackRebuildJob>  job;
    CallStackRebuildResult                caught;



    outIsSettled = false;

    CBRA (cpu);

    BAIL_OUT_IF (target > recorded, S_OK);

    job = std::make_shared<CallStackRebuildJob>();
    CPRA (job);

    hr = MakeJob (true, result.position, result.journalCursor, target, *job, hasJob);
    CHR (hr);

    BAIL_OUT_IF (!hasJob, S_OK);

    job->generation = result.generation;

    cycle   = cpu->GetTotalCycles();
    limit   = (m_continuations > 0) ? kLateCatchUpCycles : kCatchUpCycles;
    isClose = cycle >= result.cycle && cycle - result.cycle <= limit;

    if (isClose)
    {
        hr = m_rebuilder->Rebuild (*job, caught);
        CHR (hr);

        KeepCopies (caught.copies);

        BAIL_OUT_IF (caught.recordFrom > m_began, S_OK);

        outIsSettled = TryInstall (caught);
        BAIL_OUT_IF (true, S_OK);
    }

    hr = m_rebuilder->Submit (job);
    CHR (hr);

    m_awaited    = job->generation;
    m_roundFrom  = result.position;
    m_roundTo    = target;
    outIsSettled = true;

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
//  some other run. The record taken holds what a rebuild does, and the
//  rebuild's copies are kept.
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

        NoteWhole();
        CommitCopies();
    }

    return isSame;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::NoteWhole
//
//  The live record now holds what a rebuild to the machine would, so a
//  copy of it is kept at each keyframe taken from here, until it starts
//  again or is fed across a keyframe loaded under it.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::NoteWhole()
{
    m_isWhole     = true;
    m_wholeRecord = m_session.GetCallRecordGeneration();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::KeepCopies
//
//  A result's copies, moved out of it, to wait for its record to be taken.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::KeepCopies (std::vector<CallRecordCopy> & copies)
{
    m_pendingCopies.insert (m_pendingCopies.end(), std::make_move_iterator (copies.begin()), std::make_move_iterator (copies.end()));
    copies.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::CommitCopies
//
//  The copies the rebuild just taken made, each kept where its keyframe is
//  still the one it was made at -- the same cycle and checksum -- and no
//  copy is kept already. A copy without bytes is the one before it again.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::CommitCopies()
{
    const KeyframeStore    & keyframes = m_history->GetKeyframes();
    std::span<const Byte>    last;
    size_t                   index     = 0;
    bool                     isSame    = false;
    bool                     isAdded   = false;



    m_copies.SetBudget (keyframes.GetSettings().budgetBytes / kCopyBudgetParts);

    for (const CallRecordCopy & copy : m_pendingCopies)
    {
        if (!copy.packed.empty())
        {
            last = copy.packed;
        }

        isSame = keyframes.TryFindByPosition (copy.position, index);
        isSame = isSame && keyframes.GetInfo (index).position == copy.position && keyframes.GetInfo (index).cycle == copy.cycle &&
                 keyframes.GetInfo (index).checksum == copy.checksum;

        if (!isSame || last.empty())
        {
            continue;
        }

        isAdded = m_copies.TryAdd (copy.position, last);
        IGNORE_RETURN_VALUE (isAdded, false);
    }

    m_pendingCopies.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::CheckLoads
//
//  A keyframe the history's replayer loaded outside a reverse command was
//  loaded under the machine as it ran on from the past -- past a gap, or
//  back to the last good keyframe after its replay diverged -- and the
//  record was fed across it, so it starts again where the machine stands.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::CheckLoads()
{
    size_t  restores = GetRestoreCount();



    if (restores == m_restoresSeen)
    {
        return;
    }

    m_restoresSeen = restores;

    m_session.RestartCallRecording();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::OnRecordChanged
//
//  A record that started again drops the rebuild under way, and needs one
//  of its own when it began mid-run: where the debugger attached, or where a
//  move through history landed. A record from power-on holds what a rebuild
//  would, and so does one a reset started again when the record before it
//  did and nothing else started it again since.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::OnRecordChanged (
    uint64_t  record,
    bool      isOn)
{
    std::optional<CallStackBreak>  bottom   = m_session.GetCallRecordBottom();
    bool                           hasKind  = bottom.has_value();
    CallBreakKind                  kind     = hasKind ? bottom->kind : CallBreakKind::TrackingBegan;
    bool                           isMidRun = hasKind && (kind == CallBreakKind::TrackingBegan || kind == CallBreakKind::HistoryBegan);
    bool                           wasWhole = m_isWhole && m_wholeRecord == m_seenRecord && record == m_seenRecord + 1;



    Stop();

    m_isWhole     = hasKind && (kind == CallBreakKind::PowerOn || (kind == CallBreakKind::Reset && wasWhole));
    m_wholeRecord = record;
    m_seenRecord  = record;
    m_isDue       = isOn && isMidRun;
    m_began       = m_machine.GetPosition();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::OnKeyframeTaken
//
//  Live, just after a keyframe is taken: while the record holds what a
//  rebuild would, a copy of it is kept there.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::OnKeyframeTaken (uint64_t position)
{
    if (!IsCopying())
    {
        return;
    }

    CallRecordCopies::Pack (m_session.GetCallRecord(), m_packed);

    m_copies.SetBudget (m_history->GetKeyframes().GetSettings().budgetBytes / kCopyBudgetParts);
    m_copies.Set (position, m_packed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::OnKeyframeDrop
//
//  A copy goes with its keyframe.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::OnKeyframeDrop (KeyframeDrop drop)
{
    const KeyframeStore  & keyframes = m_history->GetKeyframes();



    switch (drop)
    {
    case KeyframeDrop::Oldest:
        m_copies.Drop (keyframes.GetInfo (0).position);
        break;

    case KeyframeDrop::Newest:
        m_copies.Drop (keyframes.GetInfo (keyframes.GetCount() - 1).position);
        break;

    default:
        m_copies.Clear();
        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::IsCopying
//
//  The live record holds what a rebuild would: it was found to, it has not
//  started again since, and no keyframe has been loaded under it.
//
////////////////////////////////////////////////////////////////////////////////

bool CallStackHistory::IsCopying() const
{
    return m_isWhole && m_history != nullptr && m_session.IsCallRecording() &&
           m_session.GetCallRecordGeneration() == m_wholeRecord && GetRestoreCount() == m_restoresSeen;
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
    outPart.startPosition = info.position;
    outPart.startCycle    = info.cycle;
    outPart.journalIndex  = info.journalIndex;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::AddCopyAt
//
//  The keyframe goes on the job's list of those to copy the record at.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHistory::AddCopyAt (
    const KeyframeInfo     & info,
    CallStackRebuildJob    & ioJob) const
{
    ioJob.copyAt.push_back (CallRecordCopy { info.position, info.cycle, info.checksum, {} });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::TryFindSeed
//
//  The keyframe index of the newest copy of the record at or after from and
//  at or before target; false when there is none.
//
////////////////////////////////////////////////////////////////////////////////

bool CallStackHistory::TryFindSeed (
    const KeyframeStore  & keyframes,
    uint64_t               from,
    uint64_t               target,
    size_t               & outIndex) const
{
    uint64_t  position = 0;
    bool      isFound  = m_copies.TryFindAtOrBefore (target, position);



    isFound = isFound && position >= from && keyframes.TryFindByPosition (position, outIndex);
    isFound = isFound && keyframes.GetInfo (outIndex).position == position;

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHistory::FindFreshStart
//
//  The keyframe a rebuild with no copy of the record starts at: the newest
//  point before target that the record starts again from, whatever ran
//  before it. That is the newest keyframe after a gap, since what ran in the
//  gap is not in history, or the oldest keyframe when there is none; then,
//  after it, the newest keyframe taken before the last power cycle short of
//  target, since a power cycle voids the whole record and dates it again. A
//  reset is not such a point: the record goes on dating from where it began.
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
//  CallStackHistory::GetRestoreCount
//
//  The keyframes the history's replayer has loaded; none without history.
//
////////////////////////////////////////////////////////////////////////////////

size_t CallStackHistory::GetRestoreCount() const
{
    return (m_history != nullptr) ? m_history->GetReplayer().GetRestoreCount() : 0;
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
//  Where the record a fresh job gives begins: at its first part, or, for a
//  seeded one, where a rebuild without the seed would have begun it.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t CallStackHistory::GetRecordFrom (const CallStackRebuildPart & first)
{
    return first.seed.empty() ? first.startPosition : first.seedFrom;
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





