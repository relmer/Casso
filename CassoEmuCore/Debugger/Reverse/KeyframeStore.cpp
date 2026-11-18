#include "Pch.h"

#include "Debugger/Reverse/KeyframeStore.h"
#include "Core/StateWriter.h"
#include "Shell/MachineHost.h"
#include "Core/IWorkQueue.h"
#include "Debugger/Reverse/KeyframeLayout.h"
#include "Debugger/Reverse/KeyframeUnpacker.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeStore::KeyframeStore
//
//  Each drop in the arena goes to the listeners.
//
////////////////////////////////////////////////////////////////////////////////

KeyframeStore::KeyframeStore()
{
    m_arena.SetDropCallback ([this] (KeyframeDrop drop) { NotifyDrop (drop); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeStore::~KeyframeStore
//
//  Waits for the work in flight, which writes into the pack queue's buffers.
//
////////////////////////////////////////////////////////////////////////////////

KeyframeStore::~KeyframeStore()
{
    m_packQueue.WaitAll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Configure
//
//  Takes new settings and empties the store, giving back its memory, since
//  keyframes taken on the old interval do not fall on the new one's
//  boundaries and the table and arena are sized from the budget.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::Configure (const KeyframeSettings & settings)
{
    m_settings = settings;

    Release();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChangeBudget
//
//  Takes a new byte budget without losing the history that fits it. Before
//  the first keyframe there is nothing to move, and the budget sizes the
//  store when it is first reserved. After it, the oldest groups are dropped
//  until what is stored fits the new table and arena, then the keyframes
//  kept are copied, oldest first, into memory sized from the new budget:
//  smaller or larger, the store takes its new size now. The group of the
//  keyframe at or before keepPosition is not dropped, nor any newer one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::ChangeBudget (size_t budgetBytes, uint64_t keepPosition)
{
    HRESULT  hr         = S_OK;
    size_t   slotCount  = 0;
    size_t   arenaBytes = 0;



    m_settings.budgetBytes = budgetBytes;

    BAIL_OUT_IF (!m_arena.IsAllocated(), S_OK);

    hr = WaitForPending();
    CHR (hr);

    // A new budget starts with room to spare until it has to drop again.
    m_arena.SetHasRoom();

    ComputeLayout (m_arena.GetWholeBytes(), slotCount, arenaBytes);

    hr = m_arena.Relayout (slotCount, arenaBytes, m_settings.budgetBytes, keepPosition);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Clear
//
//  Waits for the work in flight and drops it with every keyframe. The table,
//  the arena and the buffers keep their memory for the next recording.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::Clear()
{
    m_packQueue.DropAll();
    m_arena.Clear();
    m_packQueue.GetCodec().ClearLatest();

    m_nextDueCycle = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Release
//
//  Clears the store and gives back every buffer it reserved.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::Release()
{
    Clear();

    m_arena.Free();
    m_packQueue.Release();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetWorkQueue
//
//  Where the checksum, XOR and packing of each keyframe run from here on;
//  null runs them inside Add. Any work in flight on the old queue finishes
//  first.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::SetWorkQueue (IWorkQueue * queue)
{
    HRESULT  hr = S_OK;



    hr = WaitForPending();
    IGNORE_RETURN_VALUE (hr, S_OK);

    m_packQueue.SetWorkQueue (queue);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDropListener
//
//  The owner's listener replaces any it set before; a null one removes it.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::SetDropListener (
    const void    * owner,
    DropListener    listener)
{
    std::erase_if (m_dropListeners, [owner] (const std::pair<const void *, DropListener> & each) { return each.first == owner; });

    if (listener)
    {
        m_dropListeners.emplace_back (owner, std::move (listener));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Add
//
//  Stores one machine state taken at the given position and cycle, which
//  must be later than the newest keyframe's, with the input journal's end
//  index at that moment, where a replay from it starts reading inputs. It
//  is stored whole when it opens a group (the store is empty, the group is
//  done, or the state's size changed) and as a difference otherwise. The
//  side bytes, if any, are stored with it. Then the next keyframe is
//  scheduled and the budget enforced. After a failed add the next keyframe
//  is due an interval later, not at the next instruction.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::Add (
    uint64_t                   position,
    uint64_t                   cycle,
    size_t                     journalIndex,
    const std::vector<Byte>  & state,
    const std::vector<Byte>  & side)
{
    HRESULT   hr  = S_OK;
    Job     * job = nullptr;



    hr = Reserve (state.size());
    CHR (hr);

    hr = TakeJob (job);
    CHR (hr);

    job->state.assign (state.begin(), state.end());
    job->side.assign  (side.begin(),  side.end());

    hr = SubmitJob (*job, position, cycle, journalIndex);
    CHR (hr);

Error:
    if (FAILED (hr))
    {
        ScheduleAfter (cycle);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Add (from a writer)
//
//  As Add, with the state a writer has, flattened straight into the job's
//  buffer, so a sharing save doesn't copy it first.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::Add (
    uint64_t                   position,
    uint64_t                   cycle,
    size_t                     journalIndex,
    const StateWriter        & writer,
    const std::vector<Byte>  & side)
{
    HRESULT   hr  = S_OK;
    Job     * job = nullptr;



    hr = TakeJob (job);
    CHR (hr);

    writer.FlattenInto (job->state);
    job->side.assign (side.begin(), side.end());

    hr = Reserve (job->state.size());
    CHR (hr);

    hr = SubmitJob (*job, position, cycle, journalIndex);
    CHR (hr);

Error:
    if (FAILED (hr))
    {
        ScheduleAfter (cycle);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Capture
//
//  Saves the machine's whole state and adds it at the CPU's current cycle.
//  Call it on the thread that runs the machine, between instructions.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::Capture (
    const MachineHost  & machine,
    uint64_t             position)
{
    HRESULT         hr  = S_OK;
    StateWriter     writer;
    const EmuCpu  * cpu = machine.GetCpu();



    CBRA (cpu);

    hr = machine.GetStateStream().SaveState (writer);
    CHR (hr);

    hr = Add (position, cpu->GetHistoryCycles(), machine.GetHostInput().GetJournal().GetEndIndex(), writer);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reserve
//
//  Takes all the memory the store will use, once, sized from the budget and
//  the first state: the table and the arena (KeyframeLayout::ComputeLayout),
//  and the pack queue's buffers.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::Reserve (size_t stateBytes)
{
    HRESULT  hr         = S_OK;
    size_t   slotCount  = 0;
    size_t   arenaBytes = 0;



    BAIL_OUT_IF (m_arena.IsAllocated(), S_OK);

    ComputeLayout (stateBytes, slotCount, arenaBytes);

    hr = m_arena.Allocate (slotCount, arenaBytes);
    CHR (hr);

    m_packQueue.Reserve (stateBytes);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeLayout
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::ComputeLayout (
    size_t    stateBytes,
    size_t  & outSlotCount,
    size_t  & outArenaBytes) const
{
    KeyframeLayout::ComputeLayout (m_settings, stateBytes, m_arena.GetGroupLength(), KeyframeArena::GetEntryBytes(), outSlotCount, outArenaBytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryGrowTable
//
//  A table that fills while much of the arena is free (keyframes smaller
//  than the budget sized the table for, as an idle machine's are) takes
//  slots from the arena, so the budget fits as many keyframes as their bytes
//  allow rather than as the table allows. outGrew is false when the arena
//  has too little to spare, or no more slots would fit.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::TryGrowTable (bool & outGrew)
{
    HRESULT  hr         = S_OK;
    size_t   wholeBytes = m_arena.GetWholeBytes();
    size_t   usable     = (m_settings.budgetBytes > wholeBytes) ? m_settings.budgetBytes - wholeBytes : 0;
    size_t   room       = std::min (m_arena.GetArenaBytes(), usable);
    size_t   spare      = room - std::min (m_arena.GetStoredBytes(), room);
    size_t   slotCount  = 0;
    size_t   arenaBytes = 0;



    outGrew = false;

    BAIL_OUT_IF (spare <= room / kArenaSpareParts, S_OK);

    KeyframeLayout::ComputeFitLayout (m_settings,
                                      m_arena.GetPlacedCount(),
                                      m_arena.GetStoredBytes(),
                                      wholeBytes,
                                      m_arena.GetCapacity(),
                                      KeyframeArena::GetEntryBytes(),
                                      slotCount,
                                      arenaBytes);
    BAIL_OUT_IF (slotCount <= m_arena.GetCapacity(), S_OK);

    hr = m_arena.Relayout (slotCount, arenaBytes, m_settings.budgetBytes);
    CHR (hr);

    outGrew = m_arena.GetCount() < m_arena.GetCapacity();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetReservedBytes
//
//  All the memory the store has: the arena, the table, the unpacked
//  snapshots and the work buffers.
//
////////////////////////////////////////////////////////////////////////////////

size_t KeyframeStore::GetReservedBytes() const
{
    return m_arena.GetArenaBytes() + m_arena.GetTableBytes() + m_packQueue.GetBufferBytes();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeJob
//
//  The next job, whose buffer the state is written into. Results already in
//  are collected first; when every job is in flight, this waits for them
//  rather than allocate another buffer.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::TakeJob (Job *& outJob)
{
    HRESULT  hr = S_OK;



    hr = Collect();
    CHR (hr);

    if (m_packQueue.IsFull())
    {
        hr = WaitForPending();
        CHR (hr);
    }

    outJob = &m_packQueue.GetNext();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SubmitJob
//
//  Lists the keyframe with its facts so far, whole or difference chosen
//  here, and submits its packing. The checksum and packed bytes arrive
//  when it is collected. A full table first grows from the arena, or else
//  drops its oldest group.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::SubmitJob (
    Job       & job,
    uint64_t    position,
    uint64_t    cycle,
    size_t      journalIndex)
{
    HRESULT       hr          = S_OK;
    size_t        count       = m_arena.GetCount();
    bool          isEmpty     = count == 0;
    bool          isLater     = isEmpty || cycle > m_arena.GetInfo (count - 1).cycle;
    bool          isSameSize  = job.state.size() == m_arena.GetWholeBytes();
    bool          isGroupFull = KeyframeLayout::IsGroupDone (m_settings,
                                                             m_arena.GetGroupLength(),
                                                             m_arena.GetCapacity(),
                                                             m_arena.GetGroupDiffs(),
                                                             m_arena.GetGroupWhole());
    bool          isDropped   = false;
    bool          isGrown     = false;
    KeyframeInfo  info;



    CBRAEx (isLater, E_INVALIDARG);

    if (m_arena.GetCount() == m_arena.GetCapacity())
    {
        hr = WaitForPending();
        CHR (hr);

        hr = TryGrowTable (isGrown);
        CHR (hr);
    }

    if (m_arena.GetCount() == m_arena.GetCapacity())
    {
        isDropped = m_arena.TryDropOldestGroup();
        CBRA (isDropped);
    }

    info.position     = position;
    info.cycle        = cycle;
    info.journalIndex = journalIndex;
    info.wallTime     = m_wallClock();
    info.stateBytes   = job.state.size();
    info.sideBytes    = job.side.size();
    info.isWhole      = isEmpty || !isSameSize || isGroupFull;

    (void) m_arena.List (info);

    ScheduleAfter (cycle);

    m_packQueue.Submit (info.isWhole);

    hr = Collect();
    CHR (hr);

    m_arena.DropOldestGroups (m_settings.budgetBytes);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadWallClock
//
//  The host's clock as a UTC FILETIME, stamped on each keyframe so the
//  debugger can show when history begins. It is stored beside the state,
//  never in it, so replay and the state checksum do not depend on it.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t KeyframeStore::ReadWallClock()
{
    FILETIME        now  = {};
    ULARGE_INTEGER  wall = {};



    GetSystemTimeAsFileTime (&now);

    wall.LowPart  = now.dwLowDateTime;
    wall.HighPart = now.dwHighDateTime;
    return wall.QuadPart;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Collect
//
//  Lays the results of finished jobs, oldest first, into the arena for the
//  newest entries, one each. Stops at the first job in flight, since
//  jobs finish in the order they were submitted. Making room may drop the
//  oldest groups, which moves every index, so a caller takes its keyframe
//  indices after a wait, never before. A failed job is retired with every
//  job after it, so no later wait is on it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::Collect()
{
    HRESULT  hr            = S_OK;
    Job    * job           = nullptr;
    bool     isSideDropped = false;



    while (m_packQueue.TryTakeDone (job))
    {
        hr = job->hr;

        if (SUCCEEDED (hr))
        {
            hr = m_arena.PlaceNext (job->packed, job->side, job->checksum, isSideDropped);
        }

        if (FAILED (hr))
        {
            (void) DropPending();
        }

        CHR (hr);

        m_packQueue.PopDone();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DropPending
//
//  Drops every keyframe in flight, after a job failed: the failed one, and
//  every one after it, which may be a difference against it. The newest
//  group left is reloaded, so the next keyframe follows it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::DropPending()
{
    HRESULT  hr = S_OK;



    m_packQueue.DropAll();
    m_arena.DropUnplaced();

    if (m_arena.GetCount() == 0)
    {
        Clear();
        BAIL_OUT_IF (true, S_OK);
    }

    hr = ReloadLatestWhole();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WaitForPending
//
//  Waits for every job in flight and collects them all, so every keyframe
//  listed has its checksum and packed bytes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::WaitForPending()
{
    HRESULT  hr        = S_OK;
    size_t   unplaced  = 0;



    m_packQueue.WaitAll();

    hr = Collect();
    CHR (hr);

    unplaced = m_packQueue.GetPendingCount();
    CBRA (unplaced == 0);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Restore
//
//  Unpacks keyframe index into outState: the whole snapshot of its group
//  (already unpacked when it is the newest group), and for a difference, the
//  difference XORed onto it. Call WaitForPending before taking the index.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::Restore (
    size_t               index,
    std::vector<Byte>  & outState)
{
    HRESULT  hr         = S_OK;
    size_t   count      = 0;
    size_t   groupStart = 0;
    bool     isNewest   = false;
    bool     isSameSize = false;



    hr = WaitForPending();
    CHR (hr);

    count = m_arena.GetCount();
    CBRAEx (index < count, E_INVALIDARG);

    groupStart = m_arena.FindGroupStart (index);
    isNewest   = groupStart == m_arena.FindGroupStart (count - 1);
    isSameSize = m_arena.GetInfo (index).isWhole || m_arena.GetInfo (index).stateBytes == m_arena.GetInfo (groupStart).stateBytes;
    CBRA (isSameSize);

    hr = m_packQueue.GetCodec().Unpack (m_arena.GetPacked (groupStart), m_arena.GetPacked (index), isNewest, outState);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RestoreAtPosition
//
//  Unpacks the keyframe taken at position into outState. For a caller that
//  stores a position across time: its index moves as the oldest groups are
//  dropped, including while the keyframes in flight are collected, so the
//  index is found only after that wait. outIsFound is false, and outState
//  untouched, when the store no longer has a keyframe at position.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::RestoreAtPosition (
    uint64_t             position,
    std::vector<Byte>  & outState,
    bool               & outIsFound)
{
    HRESULT  hr    = S_OK;
    size_t   index = 0;



    outIsFound = false;

    hr = WaitForPending();
    CHR (hr);

    outIsFound = m_arena.TryFindByPosition (position, index) && m_arena.GetInfo (index).position == position;
    BAIL_OUT_IF (!outIsFound, S_OK);

    hr = Restore (index, outState);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CopyPacked
//
//  Copies the keyframe taken at position out of the store packed, for
//  unpacker to unpack on another thread: the difference, and its group's
//  whole snapshot unless unpacker has it already. Only the packed bytes are
//  copied, a few kilobytes, so the thread that runs the machine never
//  unpacks. Keyframes in flight are collected when they are done but never
//  waited for: a keyframe being packed is Pending, to be copied later, and
//  one the store no longer has is Gone. Collecting can drop the oldest
//  groups, so the index is found after it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::CopyPacked (
    uint64_t                  position,
    const KeyframeUnpacker  & unpacker,
    PackedKeyframe          & outPacked,
    KeyframeCopy            & outCopy)
{
    HRESULT                  hr     = S_OK;
    size_t                   index  = 0;
    bool                     isHeld = false;
    const KeyframeInfo     * entry  = nullptr;
    const KeyframeInfo     * whole  = nullptr;
    KeyframeCodec::Packed    packed;



    outCopy = KeyframeCopy::Gone;

    hr = Collect();
    CHR (hr);

    isHeld = m_arena.TryFindByPosition (position, index) && m_arena.GetInfo (index).position == position;
    BAIL_OUT_IF (!isHeld, S_OK);

    outCopy = (index < m_arena.GetPlacedCount()) ? KeyframeCopy::Copied : KeyframeCopy::Pending;
    BAIL_OUT_IF (outCopy == KeyframeCopy::Pending, S_OK);

    entry = &m_arena.GetInfo (index);
    whole = &m_arena.GetInfo (m_arena.FindGroupStart (index));

    CBRA (entry->stateBytes == whole->stateBytes);

    outPacked.wholePosition = whole->position;
    outPacked.wholeChecksum = whole->checksum;
    outPacked.stateBytes    = entry->stateBytes;
    outPacked.isWhole       = entry->isWhole;

    outPacked.whole.clear();
    outPacked.difference.clear();

    if (!unpacker.HoldsWhole (whole->position, whole->checksum))
    {
        packed = m_arena.GetPacked (m_arena.FindGroupStart (index));
        outPacked.whole.assign (packed.data, packed.data + packed.packedBytes);
    }

    if (!entry->isWhole)
    {
        packed = m_arena.GetPacked (index);
        outPacked.difference.assign (packed.data, packed.data + packed.packedBytes);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DoesStateMatch
//
//  Whether state, saved from a machine that replayed to keyframe index, has
//  that keyframe's checksum. A mismatch means the replay diverged. Waits for
//  the keyframes in flight, whose checksums are not in yet.
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeStore::DoesStateMatch (
    size_t                     index,
    const std::vector<Byte>  & state)
{
    HRESULT  hr      = S_OK;
    bool     isMatch = false;



    hr = WaitForPending();
    CHR (hr);

    isMatch = index < m_arena.GetCount()
              && state.size() == m_arena.GetInfo (index).stateBytes
              && KeyframeCodec::ComputeChecksum (state.data(), state.size()) == m_arena.GetInfo (index).checksum;

Error:
    return isMatch;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MarkNewestGap
//
//  Recording paused at gapStart and resumed at the newest keyframe, so the
//  positions between are not in history and a replay loads that keyframe
//  rather than running to it.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::MarkNewestGap (uint64_t gapStart)
{
    KeyframeInfo  & info = m_arena.GetNewestInfo();



    info.isBoundary   = true;
    info.hasGapBefore = true;
    info.gapStart     = gapStart;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TruncateAfter
//
//  Drops every keyframe taken after cycle: the history after a change from
//  the user, or history past the last keyframe a replay matched. The next
//  keyframe falls due on the first boundary after the newest one left, or
//  at once when none is left. Waits for the keyframes in flight first,
//  since the newest whole snapshot may be among them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::TruncateAfter (uint64_t cycle)
{
    return DropNewerThan (false, cycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DropAfterPosition
//
//  TruncateAfter by position, which unlike the cycle counter never restarts:
//  after a power cycle every keyframe from before it has a higher cycle than
//  the machine, and truncating by cycle would drop them all.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::DropAfterPosition (uint64_t position)
{
    return DropNewerThan (true, position);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DropNewerThan
//
//  The newest keyframes whose position, or cycle, is past limit go, and the
//  newest group left is reloaded.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::DropNewerThan (
    bool      isByPosition,
    uint64_t  limit)
{
    HRESULT  hr        = S_OK;
    bool     isDropped = false;



    hr = WaitForPending();
    CHR (hr);

    isDropped = m_arena.DropNewerThan (isByPosition, limit);
    BAIL_OUT_IF (!isDropped, S_OK);

    if (m_arena.GetCount() == 0)
    {
        Clear();
        BAIL_OUT_IF (true, S_OK);
    }

    hr = ReloadLatestWhole();
    CHR (hr);

    ScheduleAfter (m_arena.GetNewestInfo().cycle);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReloadLatestWhole
//
//  Unpacks the newest group's whole snapshot into the codec after a
//  truncation, and recounts that group.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::ReloadLatestWhole()
{
    HRESULT  hr    = S_OK;
    size_t   count = m_arena.GetCount();



    CBRA (count > 0);

    hr = m_packQueue.GetCodec().ReloadLatest (m_arena.GetPacked (m_arena.FindGroupStart (count - 1)));
    CHR (hr);

    m_arena.RecountNewestGroup();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NotifyDrop
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::NotifyDrop (KeyframeDrop drop)
{
    for (const auto & [owner, listener] : m_dropListeners)
    {
        listener (drop);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSide
//
//  The side bytes stored with keyframe index: in the arena after its packed
//  snapshot once placed, and in its job while it is in flight. Empty when
//  it was given none.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::GetSide (
    size_t          index,
    const Byte  *&  outData,
    size_t        & outSize) const
{
    size_t                    placed = m_arena.GetPlacedCount();
    const Job               * job    = nullptr;



    outData = nullptr;
    outSize = m_arena.GetInfo (index).sideBytes;

    if (outSize == 0)
    {
        return;
    }

    if (index < placed)
    {
        m_arena.GetSide (index, outData, outSize);
        return;
    }

    job     = &m_packQueue.GetPending (index - placed);
    outData = job->side.data();
    outSize = job->side.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScheduleAfter
//
//  The next keyframe falls due on the first multiple of the interval after
//  cycle, so keyframes land on the same boundaries however late within an
//  instruction each one was taken.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::ScheduleAfter (uint64_t cycle)
{
    uint64_t  interval = std::max<uint64_t> (m_settings.intervalCycles, 1);



    m_nextDueCycle = (cycle / interval + 1) * interval;
}
