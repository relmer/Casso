#include "Pch.h"

#include "Debugger/Reverse/InputJournal.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Debugger/Reverse/ReverseController.h"
#include "Devices/Acia6551.h"
#include "Shell/DebugEventLogs.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugEventLogs::DebugEventLogs
//
////////////////////////////////////////////////////////////////////////////////

DebugEventLogs::DebugEventLogs (MachineHost & machine) :
    m_machine (machine),
    m_history (machine),
    m_disk    (m_history, std::make_unique<DiskEventProbe>()),
    m_input   (m_history, std::make_unique<InputEventProbe>()),
    m_video   (m_history, std::make_unique<IoEventProbe> (IoLogKind::Video)),
    m_sound   (m_history, std::make_unique<IoEventProbe> (IoLogKind::Sound)),
    m_serial  (m_history, std::make_unique<IoEventProbe> (IoLogKind::Serial)),
    m_network (m_history, std::make_unique<IoEventProbe> (IoLogKind::Network))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugEventLogs::~DebugEventLogs
//
////////////////////////////////////////////////////////////////////////////////

DebugEventLogs::~DebugEventLogs()
{
    Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Start
//
//  Every cache starts empty at the machine's position, an open pane gets
//  everything again, and the video and sound logs' clear marks go. The feed
//  goes on the History slot of the machine as built now.
//
////////////////////////////////////////////////////////////////////////////////

void DebugEventLogs::Start (ReverseController * controller)
{
    uint64_t  position = 0;



    Stop();

    m_controller = controller;
    position     = m_machine.GetPosition();

    m_history.Attach ((controller != nullptr) ? &controller->GetKeyframes() : nullptr);

    m_disk.cache.Reset  (position);
    m_input.cache.Reset (position);
    m_video.cache.Reset (position);
    m_sound.cache.Reset (position);
    m_serial.cache.Reset (position);
    m_network.cache.Reset (position);

    m_disk.isResetDue    = m_disk.isOpen;
    m_input.isResetDue   = m_input.isOpen;
    m_video.isResetDue   = m_video.isOpen;
    m_sound.isResetDue   = m_sound.isOpen;
    m_serial.isResetDue  = m_serial.isOpen;
    m_network.isResetDue = m_network.isOpen;

    m_videoClearedAt = 0;
    m_soundClearedAt = 0;
    m_isVideoCleared = false;
    m_isSoundCleared = false;

    m_disk.runner.SetMachine  (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
    m_input.runner.SetMachine (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
    m_video.runner.SetMachine (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
    m_sound.runner.SetMachine (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
    m_serial.runner.SetMachine (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
    m_network.runner.SetMachine (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
    m_trace.Start (controller);
    m_profile.Start (controller);

    m_feed.AttachToCaches (m_machine,
                           { &m_disk.cache, &m_input.cache, &m_video.cache, &m_sound.cache, &m_serial.cache, &m_network.cache },
                           [this] () { return IsRecording(); });
    m_isStarted = true;

    if (controller != nullptr)
    {
        controller->SetKeyframeListener (this, [this] (uint64_t at) { OnKeyframe (at); });
        controller->SetLiveListener     (this, [this] (uint64_t at) { OnLive (at); });
        controller->GetKeyframes().SetDropListener (this, [this] (KeyframeDrop drop) { OnDrop (drop); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Stop
//
//  Off the machine and history, before either goes: the feed off the
//  History slot, the listeners off the controller, and every rebuild
//  dropped. Once stopped, a second stop does nothing, so the machine may
//  be gone by then.
//
////////////////////////////////////////////////////////////////////////////////

void DebugEventLogs::Stop()
{
    if (!m_isStarted)
    {
        return;
    }

    m_isStarted = false;

    if (m_controller != nullptr)
    {
        m_controller->SetKeyframeListener (this, nullptr);
        m_controller->SetLiveListener     (this, nullptr);
        m_controller->GetKeyframes().SetDropListener (this, nullptr);
    }

    m_feed.Detach (m_machine);

    m_disk.replayer.OnHistoryCut  (0);
    m_input.replayer.OnHistoryCut (0);
    m_video.replayer.OnHistoryCut (0);
    m_sound.replayer.OnHistoryCut (0);
    m_serial.replayer.OnHistoryCut (0);
    m_network.replayer.OnHistoryCut (0);

    m_disk.wantedBefore    = 0;
    m_input.wantedBefore   = 0;
    m_video.wantedBefore   = 0;
    m_sound.wantedBefore   = 0;
    m_serial.wantedBefore  = 0;
    m_network.wantedBefore = 0;

    m_trace.Stop();
    m_profile.Stop();

    m_controller = nullptr;
    m_history.Attach (nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Pump
//
////////////////////////////////////////////////////////////////////////////////

bool DebugEventLogs::Pump()
{
    m_disk.replayer.Pump();
    m_input.replayer.Pump();
    m_video.replayer.Pump();
    m_sound.replayer.Pump();
    m_serial.replayer.Pump();
    m_network.replayer.Pump();

    return m_trace.Pump();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetOpen
//
//  A pane that opens gets everything in the log in its first delta.
//
////////////////////////////////////////////////////////////////////////////////

void DebugEventLogs::SetOpen (
    DebugLogKind  kind,
    bool          isOpen)
{
    ForLog (kind, [isOpen] (auto & log)
    {
        log.isResetDue = isOpen && !log.isOpen;
        log.isOpen     = isOpen;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ForLog
//
////////////////////////////////////////////////////////////////////////////////

template <typename Fn>
void DebugEventLogs::ForLog (DebugLogKind kind, Fn && fn)
{
    switch (kind)
    {
    case DebugLogKind::Disk:  fn (m_disk);  break;
    case DebugLogKind::Input: fn (m_input); break;
    case DebugLogKind::Video:   fn (m_video);   break;
    case DebugLogKind::Sound:   fn (m_sound);   break;
    case DebugLogKind::Serial:  fn (m_serial);  break;
    case DebugLogKind::Network: fn (m_network); break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RequestEarlier
//
//  The pane scrolled to its oldest row: the rows before it are rebuilt and
//  sent as they come in.
//
////////////////////////////////////////////////////////////////////////////////

void DebugEventLogs::RequestEarlier (DebugLogKind kind)
{
    ForLog (kind, [] (auto & log) { log.wantedBefore = log.heldFrom; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DropBefore
//
//  The pane dropped its rows before position: they are rebuilt when it
//  scrolls back to them, as rows before its oldest are.
//
////////////////////////////////////////////////////////////////////////////////

void DebugEventLogs::DropBefore (
    DebugLogKind  kind,
    uint64_t      position)
{
    ForLog (kind, [position] (auto & log)
    {
        if (log.wantedBefore == 0)
        {
            log.heldFrom = std::max (log.heldFrom, position);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsOpen
//
////////////////////////////////////////////////////////////////////////////////

bool DebugEventLogs::IsOpen (DebugLogKind kind) const
{
    switch (kind)
    {
    case DebugLogKind::Disk:  return m_disk.isOpen;
    case DebugLogKind::Input: return m_input.isOpen;
    case DebugLogKind::Video:   return m_video.isOpen;
    case DebugLogKind::Sound:   return m_sound.isOpen;
    case DebugLogKind::Serial:  return m_serial.isOpen;
    case DebugLogKind::Network: return m_network.isOpen;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeDelta
//
////////////////////////////////////////////////////////////////////////////////

bool DebugEventLogs::TakeDelta (DiskLogDelta & outDelta)
{
    return BuildDelta (m_disk, outDelta);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeDelta
//
////////////////////////////////////////////////////////////////////////////////

bool DebugEventLogs::TakeDelta (InputLogDelta & outDelta)
{
    return BuildDelta (m_input, outDelta);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeDelta
//
//  The video, sound, serial or network log's.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugEventLogs::TakeDelta (DebugLogKind kind, IoLogDelta & outDelta)
{
    bool  isTaken = false;



    outDelta = IoLogDelta();

    FollowRewind();

    switch (kind)
    {
    case DebugLogKind::Video:   isTaken = BuildDelta (m_video,   outDelta); break;
    case DebugLogKind::Sound:   isTaken = BuildDelta (m_sound,   outDelta); break;
    case DebugLogKind::Serial:  isTaken = BuildDelta (m_serial,  outDelta); break;
    case DebugLogKind::Network: isTaken = BuildDelta (m_network, outDelta); break;
    default:                    break;
    }

    return isTaken;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetWorkQueue
//
////////////////////////////////////////////////////////////////////////////////

void DebugEventLogs::SetWorkQueue (IWorkQueue * queue)
{
    m_disk.runner.SetWorkQueue  (queue);
    m_input.runner.SetWorkQueue (queue);
    m_video.runner.SetWorkQueue (queue);
    m_sound.runner.SetWorkQueue (queue);
    m_serial.runner.SetWorkQueue (queue);
    m_network.runner.SetWorkQueue (queue);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKeyframe
//
//  A keyframe taken live: the caches end there.
//
////////////////////////////////////////////////////////////////////////////////

void DebugEventLogs::OnKeyframe (uint64_t position)
{
    EndAt (position);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLive
//
//  The machine became live at its position: what the caches have from there
//  on was a future a change made in the past discarded. Its state may have
//  been loaded rather than run to, so the I/O decode is seeded again, and a
//  clear mark after the position moves back to it.
//
////////////////////////////////////////////////////////////////////////////////

void DebugEventLogs::OnLive (uint64_t position)
{
    EndAt (position);
    m_trace.OnHistoryChanged();

    m_feed.Reseed();

    m_videoClearedAt = std::min (m_videoClearedAt, position);
    m_soundClearedAt = std::min (m_soundClearedAt, position);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndAt
//
//  No cached event is from at or after the machine's position, and when
//  the newest keyframe is there after a gap, the gap is the last entry:
//  recording resumed there. Taking a keyframe and becoming live both come
//  here, the second just after the first when recording resumes.
//
////////////////////////////////////////////////////////////////////////////////

void DebugEventLogs::EndAt (uint64_t position)
{
    const KeyframeStore  * keyframes = (m_controller != nullptr) ? &m_controller->GetKeyframes() : nullptr;
    size_t                 count     = (keyframes != nullptr) ? keyframes->GetCount() : 0;
    bool                   isGap     = count > 0 && keyframes->GetInfo (count - 1).position == position && keyframes->GetInfo (count - 1).hasGapBefore;



    CutBack (m_disk,  position);
    CutBack (m_input, position);
    CutBack (m_video, position);
    CutBack (m_sound, position);
    CutBack (m_serial, position);
    CutBack (m_network, position);

    if (isGap)
    {
        m_disk.cache.MarkGap    (position, keyframes->GetInfo (count - 1).cycle);
        m_input.cache.MarkGap   (position, keyframes->GetInfo (count - 1).cycle);
        m_video.cache.MarkGap   (position, keyframes->GetInfo (count - 1).cycle);
        m_sound.cache.MarkGap   (position, keyframes->GetInfo (count - 1).cycle);
        m_serial.cache.MarkGap  (position, keyframes->GetInfo (count - 1).cycle);
        m_network.cache.MarkGap (position, keyframes->GetInfo (count - 1).cycle);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDrop
//
//  The newest keyframes go when history is cut at the machine's position,
//  and every keyframe when it is cut back to its start: the stretches
//  rebuilt after the cut go with them.
//
////////////////////////////////////////////////////////////////////////////////

void DebugEventLogs::OnDrop (KeyframeDrop drop)
{
    uint64_t  position = (drop == KeyframeDrop::All) ? 0 : m_machine.GetPosition();



    if (drop == KeyframeDrop::Oldest)
    {
        return;
    }

    m_disk.replayer.OnHistoryCut  (position);
    m_input.replayer.OnHistoryCut (position);
    m_video.replayer.OnHistoryCut (position);
    m_sound.replayer.OnHistoryCut (position);
    m_serial.replayer.OnHistoryCut (position);
    m_network.replayer.OnHistoryCut (position);
    m_trace.OnHistoryChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClearIoLog
//
////////////////////////////////////////////////////////////////////////////////

void DebugEventLogs::ClearIoLog (IoLogKind kind)
{
    Log<IoEvent>  & log = (kind == IoLogKind::Video) ? m_video : m_sound;



    //  VIDEOLOG and SOUNDLOG clear their logs; the serial and network logs
    //  have no command.
    if (kind == IoLogKind::Serial || kind == IoLogKind::Network)
    {
        return;
    }

    if (kind == IoLogKind::Video)
    {
        m_videoClearedAt = m_machine.GetPosition();
        m_isVideoCleared = true;
    }
    else
    {
        m_soundClearedAt = m_machine.GetPosition();
        m_isSoundCleared = true;
    }

    //  The pane drops its rows and starts again from here.
    log.floor      = m_machine.GetPosition();
    log.isResetDue = log.isOpen;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FollowRewind
//
//  A rewind behind a clear of the video or sound log lowers the clear to the
//  machine's position, so what runs from there is listed, and a pane open on
//  the log starts again from it.
//
////////////////////////////////////////////////////////////////////////////////

void DebugEventLogs::FollowRewind()
{
    uint64_t  position = m_machine.GetPosition();



    if (m_isVideoCleared && position < m_videoClearedAt)
    {
        m_videoClearedAt   = position;
        m_video.floor      = position;
        m_video.isResetDue = m_video.isOpen;
    }

    if (m_isSoundCleared && position < m_soundClearedAt)
    {
        m_soundClearedAt   = position;
        m_sound.floor      = position;
        m_sound.isResetDue = m_sound.isOpen;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetIoEntries
//
//  The newest maxCount entries since the log was cleared: the cache's, and
//  before where the cache is complete, history's, rebuilt here.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugEventLogs::GetIoEntries (
    IoLogKind       kind,
    size_t          maxCount,
    IoLogEntries  & outEntries)
{
    HRESULT          hr      = S_OK;
    bool             isVideo = kind == IoLogKind::Video;
    Log<IoEvent>   & log     = isVideo ? m_video : m_sound;
    uint64_t         from    = isVideo ? m_videoClearedAt : m_soundClearedAt;
    uint64_t         covered = log.cache.GetCoveredFrom();
    size_t           excess  = 0;



    CBREx (kind == IoLogKind::Video || kind == IoLogKind::Sound, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    FollowRewind();

    from                  = isVideo ? m_videoClearedAt : m_soundClearedAt;
    outEntries            = IoLogEntries();
    outEntries.wasCleared = isVideo ? m_isVideoCleared : m_isSoundCleared;
    outEntries.isComplete = covered <= from;

    log.cache.CopySpan (std::max (from, covered), UINT64_MAX, outEntries.entries);

    if (!outEntries.isComplete && outEntries.entries.size() < maxCount)
    {
        hr = RebuildIoBefore (log, from, covered, maxCount - outEntries.entries.size(), outEntries);
        CHR (hr);
    }

    if (outEntries.entries.size() > maxCount)
    {
        excess = outEntries.entries.size() - maxCount;

        outEntries.entries.erase (outEntries.entries.begin(), outEntries.entries.begin() + (ptrdiff_t) excess);
        outEntries.isComplete = false;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildIoBefore
//
//  The stretches of history before position before and after from are
//  rebuilt on this thread, newest first, kListingBatch at a time, until
//  targetCount entries are found, the oldest stretch rebuilt begins at or before
//  from, or kListingBudgetMs pass. The
//  entries found go ahead of those in ioEntries, oldest first. A stretch
//  that cannot be rebuilt is one Unavailable entry; a gap in recording
//  after a stretch, a Gap entry where recording resumed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugEventLogs::RebuildIoBefore (
    Log<IoEvent>   & log,
    uint64_t         from,
    uint64_t         before,
    size_t           targetCount,
    IoLogEntries   & ioEntries)
{
    using Clock = std::chrono::steady_clock;

    HRESULT                                        hr         = S_OK;
    std::vector<DebugEventBounds>                  stretches;
    std::vector<DebugEventBounds>                  batch;
    std::vector<DebugEventStretchResult<IoEvent>>  results;
    std::vector<DebugEventEntry<IoEvent>>          older;
    std::vector<DebugEventEntry<IoEvent>>          chunk;
    DebugEventJob                                  job;
    Clock::time_point                              began      = Clock::now();
    size_t                                         next       = 0;
    size_t                                         found      = 0;
    size_t                                         k          = 0;
    size_t                                         first      = 0;
    size_t                                         s          = 0;
    uint64_t                                       reached    = before;
    bool                                           isLate     = false;
    bool                                           isFailed   = false;
    bool                                           hasResults = false;



    m_history.GetStretches (stretches);

    next = stretches.size();

    while (next > 0 && stretches[next - 1].startPosition >= before)
    {
        next--;
    }

    log.replayer.Cancel();
    log.runner.WaitForWork();

    while (next > 0 && found < targetCount && reached > from && !isLate && !isFailed)
    {
        batch.clear();
        chunk.clear();

        for (; next > 0 && batch.size() < kListingBatch && stretches[next - 1].endPosition > from; next--)
        {
            batch.push_back (stretches[next - 1]);
        }

        if (batch.empty())
        {
            break;
        }

        first = next;

        hr = m_history.BuildJob (batch, job);
        CHR (hr);

        hr = log.runner.Rebuild (job, results);

        //  A stretch that fails ends the job: the stretches rebuilt before it
        //  are listed, it is an Unavailable entry, and the listing stops there.
        isFailed   = FAILED (hr);
        hasResults = !results.empty();
        CBREx (!isFailed || hasResults, hr);
        hr = S_OK;

        //  The results are newest first; the chunk is built oldest first.
        for (k = results.size(); k > 0; k--)
        {
            const DebugEventStretchResult<IoEvent>  & result = results[k - 1];

            if (result.isUnavailable || FAILED (result.hr))
            {
                chunk.push_back ({ DebugEventEntryKind::Unavailable, std::max (result.startPosition, from), 0, IoEvent {} });
            }

            for (const DebugEventEntry<IoEvent> & entry : result.entries)
            {
                if (entry.position >= from && entry.position < before)
                {
                    chunk.push_back (entry);
                    found++;
                }
            }

            //  batch[k - 1] is stretches[s]; recording resumed where the next
            //  one begins, unless the cache has that gap.
            s = first + batch.size() - k;

            if (stretches[s].hasGapAfter && s + 1 < stretches.size() && stretches[s + 1].startPosition < before)
            {
                chunk.push_back ({ DebugEventEntryKind::Gap, stretches[s + 1].startPosition, 0, IoEvent {} });
            }

            reached = std::min (reached, result.startPosition);
        }

        older.insert (older.begin(), chunk.begin(), chunk.end());

        isLate = Clock::now() - began > std::chrono::milliseconds (kListingBudgetMs);
    }

    ioEntries.entries.insert (ioEntries.entries.begin(), older.begin(), older.end());
    ioEntries.isComplete    = reached <= from;
    ioEntries.isTimeLimited = isLate && !ioEntries.isComplete && found < targetCount;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsRecording
//
//  Whether the caches take what the machine does now: always while history
//  is off, and while history is on, only when the machine is live and not
//  paused or replaying.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugEventLogs::IsRecording() const
{
    return m_controller == nullptr || !m_controller->IsRecording() || m_controller->IsRecordingLive();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetWallTime
//
//  The host's clock at a cycle, from history's keyframes; with no history,
//  the clock now, since an event is sent as soon as it is cached.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DebugEventLogs::GetWallTime (uint64_t cycle) const
{
    FILETIME        now  = {};
    ULARGE_INTEGER  time = {};



    if (m_controller != nullptr && m_controller->IsRecording())
    {
        return m_controller->GetWallTimeAt (cycle);
    }

    GetSystemTimeAsFileTime (&now);

    time.LowPart  = now.dwLowDateTime;
    time.HighPart = now.dwHighDateTime;

    return time.QuadPart;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetOldest
//
//  The oldest position history can rebuild from, or UINT64_MAX with no
//  history.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DebugEventLogs::GetOldest() const
{
    bool  hasHistory = m_controller != nullptr && m_controller->IsRecording() && m_controller->GetKeyframes().GetCount() > 0;



    return hasHistory ? m_controller->GetKeyframes().GetInfo (0).position : UINT64_MAX;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CutBack
//
//  The next delta drops the pane's rows from position on, when it has any.
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
void DebugEventLogs::CutBack (
    Log<T>    & log,
    uint64_t    position)
{
    InputJournal  & journal = m_machine.GetInputJournal();



    log.cache.CutBack (position);

    if (log.isOpen && !log.isResetDue)
    {
        log.cutFrom     = std::min (log.cutFrom, position);
        log.sentSerial  = std::min (log.sentSerial, log.cache.GetEndSerial());
        log.sentJournal = std::min (log.sentJournal, journal.GetEndIndex());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildDelta
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
bool DebugEventLogs::BuildDelta (
    Log<T>            & log,
    DebugLogDelta<T>  & outDelta)
{
    bool  isNeeded = false;



    outDelta = DebugLogDelta<T>();

    if (!log.isOpen)
    {
        return false;
    }

    if (log.isResetDue)
    {
        log.isResetDue   = false;
        log.cutFrom      = DebugLogDelta<T>::kNoCut;
        log.heldFrom     = std::max (log.cache.GetCoveredFrom(), log.floor);
        log.sentSerial   = log.cache.GetFirstSerial();
        log.sentJournal  = FindJournalIndex (log.heldFrom);
        log.wantedBefore = 0;

        //  A log cleared since the cache began sends only what came after.
        for (size_t index = 0; index < log.cache.GetCount() && log.cache.GetAt (index).position < log.floor; index++)
        {
            log.sentSerial++;
        }

        outDelta.isReset = true;
    }

    if (log.cutFrom != DebugLogDelta<T>::kNoCut)
    {
        outDelta.cutFrom = log.cutFrom;
        log.heldFrom     = std::min (log.heldFrom, log.cutFrom);
        log.cutFrom      = DebugLogDelta<T>::kNoCut;
    }

    SendNewer (log, outDelta);
    SendOlder (log, outDelta);

    outDelta.heldFrom     = log.heldFrom;
    outDelta.isAtStart    = log.heldFrom <= GetOldestOf (log);
    outDelta.isRebuilding = log.wantedBefore != 0;

    isNeeded = !outDelta.IsEmpty() ||
               outDelta.heldFrom     != log.sentHeldFrom ||
               outDelta.isAtStart    != log.sentAtStart  ||
               outDelta.isRebuilding != log.sentRebuilding;

    log.sentHeldFrom   = outDelta.heldFrom;
    log.sentAtStart    = outDelta.isAtStart;
    log.sentRebuilding = outDelta.isRebuilding;

    return isNeeded;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SendNewer
//
//  The entries cached since the last delta, with the journal's entries made
//  since, in position order.
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
void DebugEventLogs::SendNewer (
    Log<T>            & log,
    DebugLogDelta<T>  & outDelta)
{
    InputJournal                     & journal = m_machine.GetInputJournal();
    uint64_t                           first   = log.cache.GetFirstSerial();
    size_t                             index   = 0;
    std::vector<DebugEventEntry<T>>    entries;



    //  Entries the ring dropped before they were sent are gone.
    log.sentSerial = std::max (log.sentSerial, first);

    for (index = static_cast<size_t> (log.sentSerial - first); index < log.cache.GetCount(); index++)
    {
        entries.push_back (log.cache.GetAt (index));
    }

    AppendEntries (entries, outDelta.newer);

    log.sentJournal = std::max (log.sentJournal, journal.GetBeginIndex());

    MergeJournal (outDelta.newer, 0, UINT64_MAX, log.sentJournal, journal.GetEndIndex());

    log.sentSerial  = log.cache.GetEndSerial();
    log.sentJournal = journal.GetEndIndex();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SendOlder
//
//  While a request for rows before the pane's oldest is open, requests the
//  stretch there (the replayer fetches more before it), then sends the rows
//  of every rebuilt stretch back from the oldest row in one piece, which
//  closes the request. Nothing is older than where history begins.
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
void DebugEventLogs::SendOlder (
    Log<T>            & log,
    DebugLogDelta<T>  & outDelta)
{
    HRESULT                          hr       = S_OK;
    uint64_t                         from     = log.heldFrom;
    size_t                           i        = 0;
    bool                             isAtTop  = log.heldFrom <= GetOldestOf (log);
    std::vector<DebugEventBounds>    stretches;
    std::vector<DebugEventEntry<T>>  span;



    BAIL_OUT_IF (log.wantedBefore == 0, S_OK);

    if (isAtTop)
    {
        log.wantedBefore = 0;
        BAIL_OUT_IF (true, S_OK);
    }

    hr = log.replayer.Request (log.heldFrom - 1, log.heldFrom);
    CHR (hr);

    m_history.GetStretches (stretches);

    for (i = stretches.size(); i > 0; i--)
    {
        if (stretches[i - 1].startPosition >= from)
        {
            continue;
        }

        if (!log.replayer.GetSpan (stretches[i - 1].startPosition, from, span))
        {
            break;
        }

        from = stretches[i - 1].startPosition;
    }

    //  A stretch can begin before the log was cleared; nothing from before
    //  then is sent.
    from = std::max (from, log.floor);

    BAIL_OUT_IF (from >= log.heldFrom, S_OK);

    (void) log.replayer.GetSpan (from, log.heldFrom, span);

    AppendEntries (span, outDelta.older);
    MergeJournal  (outDelta.older, from, log.heldFrom, FindJournalIndex (from), FindJournalIndex (log.heldFrom));

    log.heldFrom     = from;
    log.wantedBefore = 0;

Error:
    if (FAILED (hr))
    {
        log.wantedBefore = 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MergeJournal
//
//  The journal's entries in [firstIndex, endIndex) at positions in
//  [from, to) that are rows of this log, merged into rows, which are in
//  position order. An entry goes before an event at its position: it was
//  applied before the instruction there ran.
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
void DebugEventLogs::MergeJournal (
    std::vector<DebugLogRow<T>>  & rows,
    uint64_t                       from,
    uint64_t                       to,
    size_t                         firstIndex,
    size_t                         endIndex)
{
    InputJournal                 & journal = m_machine.GetInputJournal();
    size_t                         index   = 0;
    std::vector<DebugLogRow<T>>    records;
    std::vector<DebugLogRow<T>>    merged;
    DebugLogRow<T>                 row;



    for (index = firstIndex; index < endIndex; index++)
    {
        const InputRecord  & record = journal.GetRecord (index);

        if (record.position >= from && record.position < to && TryMakeRow (record, row))
        {
            row.wallTime = GetWallTime (row.cycle);
            records.push_back (row);
        }
    }

    if (records.empty())
    {
        return;
    }

    merged.reserve (rows.size() + records.size());

    std::ranges::merge (records, rows, std::back_inserter (merged), [] (const DebugLogRow<T> & a, const DebugLogRow<T> & b)
    {
        return a.position < b.position;
    });

    rows = std::move (merged);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppendEntries
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
void DebugEventLogs::AppendEntries (
    const std::vector<DebugEventEntry<T>>  & entries,
    std::vector<DebugLogRow<T>>            & rows) const
{
    DebugLogRow<T>  row;



    for (const DebugEventEntry<T> & entry : entries)
    {
        switch (entry.kind)
        {
            case DebugEventEntryKind::Gap:         row.kind = DebugLogRowKind::Gap;         break;
            case DebugEventEntryKind::Unavailable: row.kind = DebugLogRowKind::Unavailable; break;
            default:                               row.kind = DebugLogRowKind::Event;       break;
        }

        row.position = entry.position;
        row.cycle    = entry.cycle;
        row.wallTime = GetWallTime (entry.cycle);
        row.event    = entry.event;
        row.data     = entry.data;

        rows.push_back (row);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryMakeRow
//
//  A journal entry as a row: resets and power cycles in every log, and the
//  host's input in the input log. A key's latch is a key down when its
//  strobe is set; a strobe the guest cleared is not host input. A key down
//  entry that clears the line is the last key coming up.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugEventLogs::TryMakeRow (
    const InputRecord  & record,
    DiskLogRow         & outRow)
{
    return TryMakeResetRow (record, outRow);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryMakeRow
//
////////////////////////////////////////////////////////////////////////////////

bool DebugEventLogs::TryMakeRow (
    const InputRecord  & record,
    IoLogRow           & outRow)
{
    return TryMakeResetRow (record, outRow);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryMakeResetRow
//
//  A reset or a power cycle, the only journal entries the Disk II, video and
//  sound logs list.
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
bool DebugEventLogs::TryMakeResetRow (
    const InputRecord  & record,
    DebugLogRow<T>     & outRow)
{
    bool  isRow = record.kind == InputKind::Reset || record.kind == InputKind::PowerCycle;



    outRow          = DebugLogRow<T>();
    outRow.kind     = (record.kind == InputKind::Reset) ? DebugLogRowKind::Reset : DebugLogRowKind::PowerCycle;
    outRow.position = record.position;
    outRow.cycle    = record.cycle;

    return isRow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryMakeRow
//
////////////////////////////////////////////////////////////////////////////////

bool DebugEventLogs::TryMakeRow (
    const InputRecord  & record,
    InputLogRow        & outRow) const
{
    constexpr Byte  kStrobe = 0x80;
    InputEvent &    event   = outRow.event;
    bool            isRow   = true;



    outRow          = InputLogRow();
    outRow.position = record.position;
    outRow.cycle    = record.cycle;

    event.category = InputEventCategory::Host;
    event.cycle    = record.cycle;

    switch (record.kind)
    {
        case InputKind::Reset:
            outRow.kind = DebugLogRowKind::Reset;
            break;

        case InputKind::PowerCycle:
            outRow.kind = DebugLogRowKind::PowerCycle;
            break;

        case InputKind::KeyLatch:
            isRow                   = (record.value & kStrobe) != 0;
            event.type              = InputEventType::HostKeyDown;
            event.payload.key.ascii = static_cast<uint8_t> (record.value & ~kStrobe);
            break;

        case InputKind::KeyDown:
            isRow      = record.value == 0;
            event.type = InputEventType::HostKeyUp;
            break;

        case InputKind::AutoRepeat:
            event.type              = InputEventType::HostAutoRepeat;
            event.payload.key.ascii = record.value;
            break;

        case InputKind::PasteChar:
            event.type              = InputEventType::HostPaste;
            event.payload.key.ascii = record.value;
            break;

        case InputKind::Paddle:
            event.type               = InputEventType::HostPaddle;
            event.payload.io.address = record.detail;
            event.payload.io.value   = record.value;
            break;

        case InputKind::Button:
            event.type               = InputEventType::HostButton;
            event.payload.io.address = record.detail;
            event.payload.io.value   = record.value;
            break;

        case InputKind::MouseMove:
            event.type              = InputEventType::HostMouseMove;
            event.payload.motion.dx = static_cast<int16_t> (static_cast<int32_t> (record.data >> 32));
            event.payload.motion.dy = static_cast<int16_t> (static_cast<int32_t> (record.data & 0xFFFFFFFF));
            break;

        case InputKind::MouseButton:
            event.type             = InputEventType::HostMouseButton;
            event.payload.io.value = record.value;
            break;

        case InputKind::AciaByte:
            isRow                    = record.detail < m_machine.GetRefs().serialChips.size();
            event.type               = InputEventType::HostSerialByte;
            event.payload.io.address = isRow ? m_machine.GetRefs().serialChips[record.detail]->GetCommPort() : 0;
            event.payload.io.value   = record.value;
            event.payload.io.flags   = static_cast<uint8_t> (record.data);
            break;

        case InputKind::AciaLines:
            event.type               = InputEventType::HostSerialLines;
            event.payload.io.address = record.detail;
            event.payload.io.value   = record.value;
            break;

        case InputKind::AciaConnection:
            event.type               = InputEventType::HostSerialLink;
            event.payload.io.address = record.detail;
            event.payload.io.value   = record.value;
            break;

        default:
            isRow = false;
            break;
    }

    return isRow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindJournalIndex
//
//  The index of the first journal entry at or after position, or the
//  journal's end when none is.
//
////////////////////////////////////////////////////////////////////////////////

size_t DebugEventLogs::FindJournalIndex (uint64_t position) const
{
    const InputJournal  & journal = m_machine.GetInputJournal();
    size_t                low     = journal.GetBeginIndex();
    size_t                high    = journal.GetEndIndex();
    size_t                mid     = 0;



    while (low < high)
    {
        mid = low + (high - low) / 2;

        if (journal.GetRecord (mid).position < position)
        {
            low = mid + 1;
        }
        else
        {
            high = mid;
        }
    }

    return low;
}
