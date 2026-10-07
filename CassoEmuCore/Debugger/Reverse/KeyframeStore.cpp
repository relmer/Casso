#include "Pch.h"

#include "Debugger/Reverse/KeyframeStore.h"
#include "Core/StateHash.h"
#include "Core/StateWriter.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ~KeyframeStore
//
//  Waits for the work in flight, which writes into this store's buffers.
//
////////////////////////////////////////////////////////////////////////////////

KeyframeStore::~KeyframeStore()
{
    if (m_queue != nullptr && m_pendingCount != 0)
    {
        m_queue->WaitAll();
    }
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
//  until what is held fits the new table and arena, then the keyframes kept
//  are copied, oldest first, into memory sized from the new budget: smaller
//  or larger, the store takes its new size now.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::ChangeBudget (size_t budgetBytes)
{
    HRESULT  hr         = S_OK;
    bool     isReserved = !m_entries.empty();
    size_t   slotCount  = 0;
    size_t   arenaBytes = 0;



    m_settings.budgetBytes = budgetBytes;

    BAIL_OUT_IF (!isReserved, S_OK);

    hr = WaitForPending();
    CHR (hr);

    // A new budget starts with room to spare until it has to drop again.
    m_isFull = false;

    ComputeLayout (m_wholeBytes, slotCount, arenaBytes);

    hr = Relayout (slotCount, arenaBytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Relayout
//
//  Moves the store into a new table and arena of the sizes given: the oldest
//  groups are dropped until what is held fits them and the budget, then the
//  keyframes kept are copied, oldest first, to the start of the new arena.
//  Call it with nothing in flight.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::Relayout (
    size_t  slotCount,
    size_t  arenaBytes)
{
    HRESULT                  hr         = S_OK;
    bool                     isDropped  = true;
    size_t                   end        = 0;
    Byte                   * buffer     = nullptr;
    std::vector<Entry>       entries;
    std::unique_ptr<Byte[]>  arena;



    CBRA (m_pendingCount == 0);

    while (isDropped && (GetByteCount() > m_settings.budgetBytes || m_storedBytes > arenaBytes || m_count > slotCount))
    {
        isDropped = TryDropOldestGroup();
    }

    buffer = new (std::nothrow) Byte[arenaBytes];
    CPRA (buffer);

    arena.reset (buffer);

    entries.resize (slotCount);

    for (size_t i = 0; i < m_count; i++)
    {
        entries[i]        = GetEntry (i);
        entries[i].offset = end;

        if (entries[i].info.storedBytes != 0)
        {
            memcpy (arena.get() + end, m_arena.get() + GetEntry (i).offset, entries[i].info.storedBytes);
        }

        end += entries[i].info.storedBytes;
    }

    m_entries.swap (entries);
    m_arena.swap (arena);

    m_arenaBytes = arenaBytes;
    m_arenaEnd   = end;
    m_first      = 0;

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
    if (m_queue != nullptr && m_pendingCount != 0)
    {
        m_queue->WaitAll();
    }

    if (m_count > 0)
    {
        NotifyDrop (KeyframeDrop::All);
    }

    m_pendingCount = 0;

    m_first = 0;
    m_count = 0;

    m_latestWhole.clear();

    m_arenaEnd     = 0;
    m_storedBytes  = 0;
    m_sideBytes    = 0;
    m_groupLength  = 0;
    m_wholeBytes   = 0;
    m_groupWhole   = 0;
    m_groupDiffs   = 0;
    m_nextDueCycle = 0;
    m_isFull       = false;
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

    m_entries = std::vector<Entry>();
    m_arena.reset();
    m_arenaBytes = 0;

    m_latestWhole = std::vector<Byte>();
    m_scratch     = std::vector<Byte>();
    m_olderWhole  = std::vector<Byte>();

    for (Job & job : m_jobs)
    {
        job.state  = std::vector<Byte>();
        job.packed = std::vector<Byte>();
        job.side   = std::vector<Byte>();
    }
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

    m_queue = queue;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Add
//
//  Stores one machine state taken at the given position and cycle, which
//  must be later than the newest keyframe's, with the input journal's end
//  index at that moment, where a replay from it starts reading inputs. It
//  is stored whole when it opens a group (the store is empty, IsGroupDone,
//  or the state's size changed) and as
//  a difference otherwise. The side bytes, if any, are kept with it.
//  Then the next keyframe is scheduled and the budget enforced.
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
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Add (from a writer)
//
//  As Add, with the state a writer holds, flattened straight into the job's
//  buffer, so a sharing save needs no copy of its own.
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

    hr = machine.SaveState (writer);
    CHR (hr);

    hr = Add (position, cpu->GetTotalCycles(), machine.GetInputJournal().GetEndIndex(), writer);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reserve
//
//  Takes all the memory the store will use, once, sized from the budget and
//  the first state: the table, at one slot per kBudgetPerEntry bytes of
//  budget; the arena, which is the rest of the budget after the table and
//  the unpacked newest whole snapshot; and the work buffers, with room for
//  a state that grows a little. The arena is never smaller than the newest
//  group, plus a keyframe in flight and the room lost where the arena wraps,
//  can need at the packer's worst case, so a store whose budget is too small
//  for that holds more than its budget.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::Reserve (size_t stateBytes)
{
    HRESULT   hr          = S_OK;
    bool      isReserved  = !m_entries.empty();
    size_t    slotCount   = 0;
    size_t    stateRoom   = 0;
    size_t    packedBound = 0;
    size_t    arenaBytes  = 0;
    Byte    * arena       = nullptr;



    BAIL_OUT_IF (isReserved, S_OK);

    stateRoom   = stateBytes + stateBytes / kStateGrowth;
    packedBound = stateRoom + stateRoom / kPackOverhead + kPackSlack;

    ComputeLayout (stateBytes, slotCount, arenaBytes);

    arena = new (std::nothrow) Byte[arenaBytes];
    CPRA (arena);

    m_arena.reset (arena);

    m_arenaBytes = arenaBytes;
    m_entries.resize (slotCount);

    m_latestWhole.reserve (stateRoom);
    m_scratch.reserve     (stateRoom);
    m_olderWhole.reserve  (stateRoom);

    for (Job & job : m_jobs)
    {
        job.state.reserve  (stateRoom);
        job.packed.reserve (packedBound);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeLayout
//
//  The table's slots and the arena's bytes the budget gives for a state of
//  stateBytes; Reserve describes how.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::ComputeLayout (
    size_t    stateBytes,
    size_t  & outSlotCount,
    size_t  & outArenaBytes) const
{
    size_t  groupSlots   = std::max (GetGroupSlots(), m_groupLength + kBufferCount + 2);
    size_t  minimumArena = GetLeastArena (stateBytes);
    size_t  overhead     = 0;



    // The newest group, however long, stays within half the table.
    outSlotCount  = std::max (m_settings.budgetBytes / kBudgetPerEntry, 2 * groupSlots);
    overhead      = outSlotCount * sizeof (Entry) + stateBytes;
    outArenaBytes = (m_settings.budgetBytes > overhead) ? m_settings.budgetBytes - overhead : 0;
    outArenaBytes = std::max (outArenaBytes, minimumArena);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetGroupSlots
//
//  The keyframes the newest group can need at once: a group of wholeEvery, every
//  keyframe in flight, and two more.
//
////////////////////////////////////////////////////////////////////////////////

size_t KeyframeStore::GetGroupSlots() const
{
    return static_cast<size_t> (m_settings.wholeEvery) + kBufferCount + 2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsGroupDone
//
//  Whether the next keyframe starts a new group: the newest holds
//  longestGroup keyframes, or it holds wholeEvery and the differences
//  collected in it pack to at least its whole snapshot, so another whole
//  snapshot costs no more than the group already does. A group also ends
//  before it fills half the table, so a full table always holds an older
//  group to drop.
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeStore::IsGroupDone() const
{
    size_t  halfTable = m_entries.size() / 2;
    size_t  inFlight  = kBufferCount + 2;
    bool    isLongest = m_groupLength >= m_settings.longestGroup || m_groupLength + inFlight >= halfTable;
    bool    isLeast   = m_groupLength >= m_settings.wholeEvery;
    bool    isPaid    = m_groupDiffs >= m_groupWhole;



    return isLongest || (isLeast && isPaid);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLeastArena
//
//  The smallest arena that always fits the newest group at the packer's
//  worst case for a state of stateBytes: its first wholeEvery keyframes,
//  every keyframe in flight, and two more for the room lost where the arena
//  wraps. Past wholeEvery its differences together pack smaller than its
//  whole snapshot until the last one, which adds two more.
//
////////////////////////////////////////////////////////////////////////////////

size_t KeyframeStore::GetLeastArena (size_t stateBytes) const
{
    constexpr size_t  kPastLeast  = 2;
    size_t            stateRoom   = stateBytes + stateBytes / kStateGrowth;
    size_t            packedBound = stateRoom + stateRoom / kPackOverhead + kPackSlack;
    size_t            slots       = static_cast<size_t> (m_settings.wholeEvery) + kBufferCount + 2 + kPastLeast;



    return slots * packedBound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeFitLayout
//
//  A table and arena that split the budget by the keyframes held so far: a
//  slot for every keyframe of their average packed size the budget holds,
//  and the rest for the arena. Never fewer slots than now, and never an
//  arena smaller than ComputeLayout's least.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::ComputeFitLayout (
    size_t  & outSlotCount,
    size_t  & outArenaBytes) const
{
    size_t  collected  = m_count - m_pendingCount;
    size_t  average    = (collected > 0) ? m_storedBytes / collected : kBudgetPerEntry;
    size_t  usable     = (m_settings.budgetBytes > m_wholeBytes) ? m_settings.budgetBytes - m_wholeBytes : 0;
    size_t  leastArena = GetLeastArena (m_wholeBytes);



    outSlotCount  = std::max (usable / (average + sizeof (Entry)), m_entries.size());
    outArenaBytes = (usable > outSlotCount * sizeof (Entry)) ? usable - outSlotCount * sizeof (Entry) : 0;
    outArenaBytes = std::max (outArenaBytes, leastArena);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryGrowTable
//
//  A table that fills while much of the arena is still free (keyframes
//  smaller than the budget sized the table for, as an idle machine's are)
//  takes slots from the arena, so the budget holds as many keyframes as their
//  bytes allow rather than as the table allows. outGrew is false when the
//  arena has too little to spare, or no more slots would fit.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::TryGrowTable (bool & outGrew)
{
    HRESULT  hr         = S_OK;
    size_t   usable     = (m_settings.budgetBytes > m_wholeBytes) ? m_settings.budgetBytes - m_wholeBytes : 0;
    size_t   room       = std::min (m_arenaBytes, usable);
    size_t   spare      = room - std::min (m_storedBytes, room);
    size_t   slotCount  = 0;
    size_t   arenaBytes = 0;



    outGrew = false;

    BAIL_OUT_IF (spare <= room / kArenaSpareParts, S_OK);

    ComputeFitLayout (slotCount, arenaBytes);
    BAIL_OUT_IF (slotCount <= m_entries.size(), S_OK);

    hr = Relayout (slotCount, arenaBytes);
    CHR (hr);

    outGrew = m_count < m_entries.size();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetUsedBytes
//
//  The bytes held against the budget, for a meter of how full it is: all of
//  it once the oldest have been dropped for room, since the store then holds
//  all it can, even where a group dropped whole or the room lost where the
//  arena wraps leaves the bytes held short of the budget. The table counts
//  from the first keyframe on: its slots come out of the budget, so the
//  arena fills, and the oldest start being dropped, with the snapshots alone
//  short of the budget by the table's size.
//
////////////////////////////////////////////////////////////////////////////////

size_t KeyframeStore::GetUsedBytes() const
{
    size_t  tableBytes = (m_count > 0) ? m_entries.size() * sizeof (Entry) : 0;



    if (m_isFull)
    {
        return m_settings.budgetBytes;
    }

    return std::min (GetByteCount() + tableBytes, m_settings.budgetBytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetReservedBytes
//
//  All the memory the store holds: the arena, the table, the unpacked
//  snapshots and the work buffers.
//
////////////////////////////////////////////////////////////////////////////////

size_t KeyframeStore::GetReservedBytes() const
{
    size_t  bytes = m_arenaBytes + m_entries.capacity() * sizeof (Entry);



    bytes += m_latestWhole.capacity() + m_scratch.capacity() + m_olderWhole.capacity();

    for (const Job & job : m_jobs)
    {
        bytes += job.state.capacity() + job.packed.capacity();
    }

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeJob
//
//  The next job, whose buffer the state is written into. Results already in
//  are collected first; when every job is still in flight, this waits for
//  them rather than allocate another buffer.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::TakeJob (Job *& outJob)
{
    HRESULT  hr = S_OK;



    hr = Collect();
    CHR (hr);

    if (m_pendingCount == m_jobs.size())
    {
        hr = WaitForPending();
        CHR (hr);
    }

    outJob        = &m_jobs[m_nextJob];
    outJob->store = this;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SubmitJob
//
//  Lists the keyframe with what is known now, whole or difference decided
//  here, and hands its packing over: to the queue, or run at once without
//  one. The checksum and packed bytes arrive when it is collected. A full
//  table gives up its oldest group first.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::SubmitJob (
    Job       & job,
    uint64_t    position,
    uint64_t    cycle,
    size_t      journalIndex)
{
    HRESULT   hr          = S_OK;
    Entry   * entry       = nullptr;
    bool      isEmpty     = m_count == 0;
    bool      isLater     = isEmpty || cycle > GetEntry (m_count - 1).info.cycle;
    bool      isSameSize  = job.state.size() == m_wholeBytes;
    bool      isGroupFull = IsGroupDone();
    bool      isDropped   = false;
    bool      isGrown     = false;



    CBRAEx (isLater, E_INVALIDARG);

    if (m_count == m_entries.size())
    {
        hr = WaitForPending();
        CHR (hr);

        hr = TryGrowTable (isGrown);
        CHR (hr);
    }

    if (m_count == m_entries.size())
    {
        isDropped = TryDropOldestGroup();
        CBRA (isDropped);
    }

    entry       = &GetEntry (m_count);
    *entry      = Entry();
    m_count++;

    entry->info.position     = position;
    entry->info.cycle        = cycle;
    entry->info.journalIndex = journalIndex;
    entry->info.wallTime     = m_wallClock();
    entry->info.stateBytes   = job.state.size();
    entry->info.sideBytes    = job.side.size();
    entry->info.isWhole      = isEmpty || !isSameSize || isGroupFull;

    if (entry->info.isWhole)
    {
        m_wholeBytes  = job.state.size();
        m_groupLength = 1;
    }
    else
    {
        m_groupLength++;
    }

    job.isWhole = entry->info.isWhole;
    job.isDone.store (false, std::memory_order_relaxed);

    m_nextJob = (m_nextJob + 1) % m_jobs.size();
    m_pendingCount++;

    ScheduleAfter (cycle);

    if (m_queue != nullptr)
    {
        hr = m_queue->Submit (RunJob, &job);
        CHR (hr);
    }
    else
    {
        RunJob (&job);
    }

    hr = Collect();
    CHR (hr);

    DropOldestGroups();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadWallClock
//
//  The host's clock as a UTC FILETIME, stamped on each keyframe so the
//  debugger can show when history begins. It is kept beside the state, never
//  in it, so replay and the state checksum do not depend on it.
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
//  RunJob
//
//  The work function a queue runs.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::RunJob (void * context)
{
    Job  * job = static_cast<Job *> (context);



    job->store->Pack (*job);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Pack
//
//  The work: the checksum of the state, then the state packed whole, which
//  becomes the newest whole snapshot, or its XOR against that snapshot
//  packed. Runs on the queue, one job at a time and in order, and touches
//  nothing the caller's thread reads while a job is in flight.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::Pack (Job & job)
{
    HRESULT  hr         = S_OK;
    bool     isSameSize = false;



    job.checksum = ComputeChecksum (job.state.data(), job.state.size());

    if (job.isWhole)
    {
        hr = m_compressor.Compress (job.state.data(), job.state.size(), job.packed);
        CHR (hr);

        // The job's buffer becomes the newest whole snapshot, and the old
        // one's buffer goes back to the job, so neither is copied.
        m_latestWhole.swap (job.state);
    }
    else
    {
        isSameSize = job.state.size() == m_latestWhole.size();
        CBRA (isSameSize);

        m_scratch.resize (job.state.size());

        XorBytes (job.state.data(), m_latestWhole.data(), m_scratch.data(), job.state.size());

        hr = m_compressor.Compress (m_scratch.data(), m_scratch.size(), job.packed);
        CHR (hr);
    }

Error:
    job.hr = hr;
    job.isDone.store (true, std::memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Collect
//
//  Lays the results of finished jobs, oldest first, into the arena for the
//  newest entries they belong to, each followed by its side bytes. Stops at
//  the first job still in flight, since jobs finish in the order they were
//  handed over. Making room may drop the oldest groups, which moves every
//  index, so a caller takes its keyframe indices after a wait, never before.
//  The side bytes are left out when the arena has no room for them, so a
//  keyframe is never lost to them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::Collect()
{
    HRESULT                    hr       = S_OK;
    Job                      * job      = nullptr;
    Entry                    * entry    = nullptr;
    size_t                     offset   = 0;
    bool                       isDone   = false;
    const std::vector<Byte>    noSide;



    while (m_pendingCount != 0)
    {
        job    = &m_jobs[(m_nextJob + m_jobs.size() - m_pendingCount) % m_jobs.size()];
        isDone = job->isDone.load (std::memory_order_acquire);

        if (!isDone)
        {
            break;
        }

        CHR (job->hr);

        hr = Place (job->packed, job->side, offset);

        if (hr == E_OUTOFMEMORY && !job->side.empty())
        {
            job->side.clear();

            hr = Place (job->packed, noSide, offset);
        }

        CHR (hr);

        entry = &GetEntry (m_count - m_pendingCount);

        entry->info.checksum    = job->checksum;
        entry->info.sideBytes   = job->side.size();
        entry->info.storedBytes = job->packed.size() + job->side.size();
        entry->offset           = offset;

        m_storedBytes += entry->info.storedBytes;
        m_sideBytes   += entry->info.sideBytes;
        m_pendingCount--;

        if (entry->info.isWhole)
        {
            m_groupWhole = job->packed.size();
            m_groupDiffs = 0;
        }
        else
        {
            m_groupDiffs += job->packed.size();
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Place
//
//  Copies packed, then side, into the arena after the newest packed
//  snapshot, dropping the oldest groups until they fit. The arena always
//  has room for the snapshots of the newest group, so only side bytes can
//  fail to fit, which is E_OUTOFMEMORY without an assert.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::Place (
    const std::vector<Byte>  & packed,
    const std::vector<Byte>  & side,
    size_t                   & outOffset)
{
    HRESULT  hr        = S_OK;
    size_t   size      = packed.size() + side.size();
    bool     hasRoom   = TryFindRoom (size, outOffset);
    bool     hasSide   = !side.empty();
    bool     isDropped = false;



    while (!hasRoom)
    {
        isDropped = TryDropOldestGroup();
        BAIL_OUT_IF (!isDropped && hasSide, E_OUTOFMEMORY);
        CBRAEx (isDropped, E_OUTOFMEMORY);

        hasRoom = TryFindRoom (size, outOffset);
    }

    if (!packed.empty())
    {
        memcpy (m_arena.get() + outOffset, packed.data(), packed.size());
    }

    if (hasSide)
    {
        memcpy (m_arena.get() + outOffset + packed.size(), side.data(), side.size());
    }

    m_arenaEnd = outOffset + size;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryFindRoom
//
//  Where size bytes can go: straight after the newest packed snapshot, or
//  at the start of the arena when the end has no room and the oldest one
//  does not start before size. The packed snapshots held run from the
//  oldest's offset to m_arenaEnd, wrapping around the end of the arena.
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeStore::TryFindRoom (
    size_t    size,
    size_t  & outOffset) const
{
    size_t  collected = m_count - m_pendingCount;
    size_t  head      = 0;
    size_t  tail      = m_arenaEnd;
    bool    isHolding = collected > 0 && m_storedBytes > 0;
    bool    isWrapped = false;
    bool    hasRoom   = false;



    if (isHolding)
    {
        head      = GetEntry (0).offset;
        isWrapped = tail <= head;
    }
    else
    {
        // Nothing held: anywhere the size fits, after the newest if it can.
        head = m_arenaBytes;
        tail = (collected > 0 && tail + size <= m_arenaBytes) ? tail : 0;
    }

    if (!isWrapped && m_arenaBytes - tail >= size)
    {
        outOffset = tail;
        hasRoom   = true;
    }
    else if (!isWrapped && head >= size)
    {
        outOffset = 0;
        hasRoom   = true;
    }
    else if (isWrapped && head - tail >= size)
    {
        outOffset = tail;
        hasRoom   = true;
    }

    return hasRoom;
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
    HRESULT  hr = S_OK;



    if (m_queue != nullptr && m_pendingCount != 0)
    {
        m_queue->WaitAll();
    }

    hr = Collect();
    CHR (hr);

    CBRA (m_pendingCount == 0);

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
    HRESULT        hr         = S_OK;
    size_t         count      = 0;
    size_t         groupStart = 0;
    bool           isNewest   = false;
    const Entry  * entry      = nullptr;
    const Entry  * whole      = nullptr;
    const Byte   * base       = nullptr;



    hr = WaitForPending();
    CHR (hr);

    count = m_count;
    CBRAEx (index < count, E_INVALIDARG);

    entry      = &GetEntry (index);
    groupStart = FindGroupStart (index);
    whole      = &GetEntry (groupStart);
    isNewest   = groupStart == FindGroupStart (count - 1);

    if (!isNewest)
    {
        hr = m_compressor.Decompress (m_arena.get() + whole->offset, GetPackedBytes (whole->info), whole->info.stateBytes, m_olderWhole);
        CHR (hr);
    }

    base = isNewest ? m_latestWhole.data() : m_olderWhole.data();

    if (entry->info.isWhole)
    {
        outState.assign (base, base + entry->info.stateBytes);
    }
    else
    {
        CBRA (entry->info.stateBytes == whole->info.stateBytes);

        hr = m_compressor.Decompress (m_arena.get() + entry->offset, GetPackedBytes (entry->info), entry->info.stateBytes, outState);
        CHR (hr);

        XorBytes (outState.data(), base, outState.data(), outState.size());
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RestoreAtPosition
//
//  Unpacks the keyframe taken at position into outState. For a caller that
//  holds a keyframe across time: its index moves as the oldest groups are
//  dropped, including while the keyframes in flight are collected, so the
//  index is found only after that wait. outIsFound is false, and outState
//  untouched, when no keyframe at position is held any more.
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

    outIsFound = TryFindByPosition (position, index) && GetEntry (index).info.position == position;
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
//  Copies the keyframe taken at position out of the store still packed, for
//  unpacker to unpack on another thread: the difference, and its group's
//  whole snapshot unless unpacker holds it already. Only the packed bytes
//  are copied, a few kilobytes, so the thread that runs the machine never
//  unpacks. Keyframes in flight are collected when they are done but never
//  waited for: a keyframe still being packed is Pending, to be asked for
//  again later, and one no longer held is Gone. Collecting can drop the
//  oldest groups, so the index is found after it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::CopyPacked (
    uint64_t                  position,
    const KeyframeUnpacker  & unpacker,
    PackedKeyframe          & outPacked,
    KeyframeCopy            & outCopy)
{
    HRESULT        hr        = S_OK;
    size_t         index     = 0;
    size_t         collected = 0;
    bool           isHeld    = false;
    const Entry  * entry     = nullptr;
    const Entry  * whole     = nullptr;
    const Byte   * bytes     = nullptr;



    outCopy = KeyframeCopy::Gone;

    hr = Collect();
    CHR (hr);

    isHeld = TryFindByPosition (position, index) && GetEntry (index).info.position == position;
    BAIL_OUT_IF (!isHeld, S_OK);

    collected = m_count - m_pendingCount;
    outCopy   = (index < collected) ? KeyframeCopy::Copied : KeyframeCopy::Pending;
    BAIL_OUT_IF (outCopy == KeyframeCopy::Pending, S_OK);

    entry = &GetEntry (index);
    whole = &GetEntry (FindGroupStart (index));

    CBRA (entry->info.stateBytes == whole->info.stateBytes);

    outPacked.wholePosition = whole->info.position;
    outPacked.wholeChecksum = whole->info.checksum;
    outPacked.stateBytes    = entry->info.stateBytes;
    outPacked.isWhole       = entry->info.isWhole;

    outPacked.whole.clear();
    outPacked.difference.clear();

    if (!unpacker.HoldsWhole (whole->info.position, whole->info.checksum))
    {
        bytes = m_arena.get() + whole->offset;
        outPacked.whole.assign (bytes, bytes + GetPackedBytes (whole->info));
    }

    if (!entry->info.isWhole)
    {
        bytes = m_arena.get() + entry->offset;
        outPacked.difference.assign (bytes, bytes + GetPackedBytes (entry->info));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryFindAtOrBefore
//
//  The newest keyframe taken at or before cycle; false when every keyframe
//  is later, or there are none.
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeStore::TryFindAtOrBefore (
    uint64_t   cycle,
    size_t   & outIndex) const
{
    size_t  low   = 0;
    size_t  high  = m_count;
    size_t  mid   = 0;
    bool    found = false;



    while (low < high)
    {
        mid = low + (high - low) / 2;

        if (GetEntry (mid).info.cycle <= cycle)
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





////////////////////////////////////////////////////////////////////////////////
//
//  TryFindByPosition
//
//  The newest keyframe taken at or before position; false when every
//  keyframe is later, or there are none.
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeStore::TryFindByPosition (
    uint64_t   position,
    size_t   & outIndex) const
{
    size_t  low   = 0;
    size_t  high  = m_count;
    size_t  mid   = 0;
    bool    found = false;



    while (low < high)
    {
        mid = low + (high - low) / 2;

        if (GetEntry (mid).info.position <= position)
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

    isMatch = index < m_count
              && state.size() == GetEntry (index).info.stateBytes
              && ComputeChecksum (state.data(), state.size()) == GetEntry (index).info.checksum;

Error:
    return isMatch;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MarkNewestGap
//
//  Recording paused at gapStart and resumed at the newest keyframe, so the
//  positions between are not in history and a replay loads that keyframe
//  rather than reaching it.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::MarkNewestGap (uint64_t gapStart)
{
    KeyframeInfo  & info = GetEntry (m_count - 1).info;



    info.isBoundary   = true;
    info.hasGapBefore = true;
    info.gapStart     = gapStart;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TruncateAfter
//
//  Drops every keyframe taken after cycle: the recorded future once the user
//  changes something, or history past the last keyframe a replay matched.
//  The next keyframe falls due on the first boundary after the newest one
//  left, or at once when none is left. Waits for the keyframes in flight
//  first, since the newest whole snapshot may be among them.
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
//  The newest keyframes whose position, or cycle, is past limit go.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::DropNewerThan (
    bool      isByPosition,
    uint64_t  limit)
{
    HRESULT        hr      = S_OK;
    bool           dropped = false;
    const Entry  * newest  = nullptr;



    hr = WaitForPending();
    CHR (hr);

    while (m_count > 0 && (isByPosition ? GetEntry (m_count - 1).info.position : GetEntry (m_count - 1).info.cycle) > limit)
    {
        NotifyDrop (KeyframeDrop::Newest);

        m_storedBytes -= GetEntry (m_count - 1).info.storedBytes;
        m_sideBytes   -= GetEntry (m_count - 1).info.sideBytes;
        m_count--;
        dropped = true;
    }

    BAIL_OUT_IF (!dropped, S_OK);

    // The newest dropped leave room again.
    m_isFull = false;

    if (m_count == 0)
    {
        Clear();
        BAIL_OUT_IF (true, S_OK);
    }

    newest     = &GetEntry (m_count - 1);
    m_arenaEnd = newest->offset + newest->info.storedBytes;

    hr = ReloadLatestWhole();
    CHR (hr);

    ScheduleAfter (newest->info.cycle);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReloadLatestWhole
//
//  Unpacks the newest group's whole snapshot into m_latestWhole after a
//  truncation, and recounts that group.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::ReloadLatestWhole()
{
    HRESULT        hr         = S_OK;
    size_t         count      = m_count;
    size_t         groupStart = 0;
    const Entry  * whole      = nullptr;



    CBRA (count > 0);

    groupStart = FindGroupStart (count - 1);
    whole      = &GetEntry (groupStart);

    hr = m_compressor.Decompress (m_arena.get() + whole->offset, GetPackedBytes (whole->info), whole->info.stateBytes, m_latestWhole);
    CHR (hr);

    m_groupLength = count - groupStart;
    m_wholeBytes  = whole->info.stateBytes;
    m_groupWhole  = GetPackedBytes (whole->info);
    m_groupDiffs  = 0;

    for (size_t i = groupStart + 1; i < count; i++)
    {
        m_groupDiffs += GetPackedBytes (GetEntry (i).info);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindGroupStart
//
//  The index of the whole snapshot keyframe index belongs to. The oldest
//  entry is always whole, because groups are dropped whole.
//
////////////////////////////////////////////////////////////////////////////////

size_t KeyframeStore::FindGroupStart (size_t index) const
{
    while (index > 0 && !GetEntry (index).info.isWhole)
    {
        index--;
    }

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryDropOldestGroup
//
//  Drops the oldest group, its whole snapshot and every difference against
//  it, when a collected whole snapshot follows it; false when none does, so
//  the newest group and every keyframe in flight stay.
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeStore::TryDropOldestGroup()
{
    size_t  collected = m_count - m_pendingCount;
    size_t  nextGroup = 1;
    bool    isDropped = false;



    while (nextGroup < collected && !GetEntry (nextGroup).info.isWhole)
    {
        nextGroup++;
    }

    isDropped = nextGroup < collected;

    for (; isDropped && nextGroup > 0; nextGroup--)
    {
        PopOldest();
    }

    if (isDropped)
    {
        m_isFull = true;
    }

    return isDropped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DropOldestGroups
//
//  While over the budget, drops the oldest group. The newest group stays
//  even over budget.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::DropOldestGroups()
{
    bool  isDropped = true;



    while (isDropped && GetByteCount() > m_settings.budgetBytes)
    {
        isDropped = TryDropOldestGroup();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PopOldest
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::PopOldest()
{
    NotifyDrop (KeyframeDrop::Oldest);

    m_storedBytes -= GetEntry (0).info.storedBytes;
    m_sideBytes   -= GetEntry (0).info.sideBytes;

    m_first = (m_first + 1) % m_entries.size();
    m_count--;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NotifyDrop
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::NotifyDrop (KeyframeDrop drop)
{
    if (m_onDrop)
    {
        m_onDrop (drop);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSide
//
//  The side bytes kept with keyframe index: in the arena after its packed
//  snapshot once collected, and in its job while it is still in flight.
//  Empty when it was given none.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::GetSide (
    size_t          index,
    const Byte  *&  outData,
    size_t        & outSize) const
{
    size_t         collected = m_count - m_pendingCount;
    const Entry  & entry     = GetEntry (index);
    const Job    * job       = nullptr;



    outData = nullptr;
    outSize = entry.info.sideBytes;

    if (outSize == 0)
    {
        return;
    }

    if (index < collected)
    {
        outData = m_arena.get() + entry.offset + GetPackedBytes (entry.info);
        return;
    }

    job     = &GetPendingJob (index - collected);
    outData = job->side.data();
    outSize = job->side.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPendingJob
//
//  The job of the pendingIndex-th keyframe still in flight, oldest first.
//
////////////////////////////////////////////////////////////////////////////////

const KeyframeStore::Job & KeyframeStore::GetPendingJob (size_t pendingIndex) const
{
    return m_jobs[(m_nextJob + m_jobs.size() - m_pendingCount + pendingIndex) % m_jobs.size()];
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





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeChecksum
//
//  StateHash over the whole state. It detects a replay that diverged, not
//  tampering.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t KeyframeStore::ComputeChecksum (
    const Byte  * data,
    size_t        size)
{
    return StateHash::Hash (data, size);
}





////////////////////////////////////////////////////////////////////////////////
//
//  XorBytes
//
//  out = a XOR b over count bytes, eight at a time; out may be a. Through
//  raw pointers and words, so the loop does not reload a vector's pointers
//  after every byte it stores.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::XorBytes (
    const Byte  * a,
    const Byte  * b,
    Byte        * out,
    size_t        count)
{
    uint64_t  wordA = 0;
    uint64_t  wordB = 0;
    size_t    i     = 0;



    for (i = 0; i + sizeof (wordA) <= count; i += sizeof (wordA))
    {
        memcpy (&wordA, a + i, sizeof (wordA));
        memcpy (&wordB, b + i, sizeof (wordB));

        wordA ^= wordB;

        memcpy (out + i, &wordA, sizeof (wordA));
    }

    for (; i < count; i++)
    {
        out[i] = static_cast<Byte> (a[i] ^ b[i]);
    }
}





