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
//  Takes new settings and empties the store, since keyframes taken on the old
//  interval do not fall on the new one's boundaries.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::Configure (const KeyframeSettings & settings)
{
    m_settings = settings;

    Clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Clear
//
//  Waits for the work in flight and drops it with everything else.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::Clear()
{
    if (m_queue != nullptr && m_pendingCount != 0)
    {
        m_queue->WaitAll();
    }

    m_pendingCount = 0;

    m_entries.clear();
    m_latestWhole.clear();
    m_latestWhole.shrink_to_fit();

    m_storedBytes  = 0;
    m_groupLength  = 0;
    m_wholeBytes   = 0;
    m_nextDueCycle = 0;
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
//  is stored whole when it opens a group (the store is empty, the group
//  holds wholeEvery keyframes already, or the state's size changed) and as
//  a difference otherwise.
//  Then the next keyframe is scheduled and the budget enforced.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::Add (
    uint64_t                   position,
    uint64_t                   cycle,
    size_t                     journalIndex,
    const std::vector<Byte>  & state)
{
    HRESULT   hr  = S_OK;
    Job     * job = nullptr;



    hr = TakeJob (job);
    CHR (hr);

    job->state.assign (state.begin(), state.end());

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
    uint64_t              position,
    uint64_t              cycle,
    size_t                journalIndex,
    const StateWriter   & writer)
{
    HRESULT   hr  = S_OK;
    Job     * job = nullptr;



    hr = TakeJob (job);
    CHR (hr);

    writer.FlattenInto (job->state);

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
//  one. The checksum and packed bytes arrive when it is collected.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::SubmitJob (
    Job       & job,
    uint64_t    position,
    uint64_t    cycle,
    size_t      journalIndex)
{
    HRESULT  hr          = S_OK;
    Entry    entry;
    bool     isEmpty     = m_entries.empty();
    bool     isLater     = isEmpty || cycle > m_entries.back().info.cycle;
    bool     isSameSize  = job.state.size() == m_wholeBytes;
    bool     isGroupFull = m_groupLength >= m_settings.wholeEvery;



    CBRAEx (isLater, E_INVALIDARG);

    entry.info.position     = position;
    entry.info.cycle        = cycle;
    entry.info.journalIndex = journalIndex;
    entry.info.stateBytes   = job.state.size();
    entry.info.isWhole      = isEmpty || !isSameSize || isGroupFull;

    if (entry.info.isWhole)
    {
        m_wholeBytes  = job.state.size();
        m_groupLength = 1;
    }
    else
    {
        m_groupLength++;
    }

    m_entries.push_back (std::move (entry));

    job.isWhole = m_entries.back().info.isWhole;
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
//  Moves the results of finished jobs, oldest first, into the newest entries
//  they belong to. Stops at the first job still in flight, since jobs finish
//  in the order they were handed over. It drops nothing, so a wait never
//  moves the index of a keyframe a caller holds; only Add enforces the
//  budget.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::Collect()
{
    HRESULT   hr     = S_OK;
    Job     * job    = nullptr;
    Entry   * entry  = nullptr;
    bool      isDone = false;



    while (m_pendingCount != 0)
    {
        job    = &m_jobs[(m_nextJob + m_jobs.size() - m_pendingCount) % m_jobs.size()];
        isDone = job->isDone.load (std::memory_order_acquire);

        if (!isDone)
        {
            break;
        }

        entry = &m_entries[m_entries.size() - m_pendingCount];

        entry->info.checksum    = job->checksum;
        entry->info.storedBytes = job->packed.size();
        entry->packed           = std::move (job->packed);

        m_storedBytes += entry->info.storedBytes;
        m_pendingCount--;

        CHR (job->hr);
    }

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
//  difference XORed onto it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::Restore (
    size_t               index,
    std::vector<Byte>  & outState)
{
    HRESULT              hr          = S_OK;
    size_t               count       = 0;
    size_t               groupStart  = 0;
    bool                 isNewest    = false;
    std::vector<Byte>    olderWhole;
    const Entry        * entry       = nullptr;
    const Entry        * whole       = nullptr;
    const Byte         * base        = nullptr;



    hr = WaitForPending();
    CHR (hr);

    count = m_entries.size();
    CBRAEx (index < count, E_INVALIDARG);

    entry      = &m_entries[index];
    groupStart = FindGroupStart (index);
    whole      = &m_entries[groupStart];
    isNewest   = groupStart == FindGroupStart (count - 1);

    if (!isNewest)
    {
        hr = m_compressor.Decompress (whole->packed, whole->info.stateBytes, olderWhole);
        CHR (hr);
    }

    base = isNewest ? m_latestWhole.data() : olderWhole.data();

    if (entry->info.isWhole)
    {
        outState.assign (base, base + entry->info.stateBytes);
    }
    else
    {
        CBRA (entry->info.stateBytes == whole->info.stateBytes);

        hr = m_compressor.Decompress (entry->packed, entry->info.stateBytes, outState);
        CHR (hr);

        XorBytes (outState.data(), base, outState.data(), outState.size());
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
    std::deque<Entry>::const_iterator  after = std::upper_bound (m_entries.begin(),
                                                                 m_entries.end(),
                                                                 cycle,
                                                                 [] (uint64_t target, const Entry & entry) { return target < entry.info.cycle; });
    bool                               found = after != m_entries.begin();



    if (found)
    {
        outIndex = static_cast<size_t> (std::distance (m_entries.begin(), after)) - 1;
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

    isMatch = index < m_entries.size()
              && state.size() == m_entries[index].info.stateBytes
              && ComputeChecksum (state.data(), state.size()) == m_entries[index].info.checksum;

Error:
    return isMatch;
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
    HRESULT  hr      = S_OK;
    bool     dropped = false;
    bool     isEmpty = false;



    hr = WaitForPending();
    CHR (hr);

    while (!m_entries.empty() && m_entries.back().info.cycle > cycle)
    {
        m_storedBytes -= m_entries.back().info.storedBytes;
        m_entries.pop_back();
        dropped = true;
    }

    BAIL_OUT_IF (!dropped, S_OK);

    isEmpty = m_entries.empty();

    if (isEmpty)
    {
        Clear();
    }
    else
    {
        hr = ReloadLatestWhole();
        CHR (hr);

        ScheduleAfter (m_entries.back().info.cycle);
    }

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
    size_t         count      = m_entries.size();
    size_t         groupStart = 0;
    const Entry  * whole      = nullptr;



    CBRA (count > 0);

    groupStart = FindGroupStart (count - 1);
    whole      = &m_entries[groupStart];

    hr = m_compressor.Decompress (whole->packed, whole->info.stateBytes, m_latestWhole);
    CHR (hr);

    m_groupLength = count - groupStart;
    m_wholeBytes  = whole->info.stateBytes;

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
    while (index > 0 && !m_entries[index].info.isWhole)
    {
        index--;
    }

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DropOldestGroups
//
//  While over the budget, drops the oldest group: its whole snapshot and
//  every difference against it. The newest group stays even over budget.
//  Only a group followed by a collected whole snapshot goes, so no keyframe
//  in flight is ever dropped.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::DropOldestGroups()
{
    size_t  nextGroup = 0;
    size_t  collected = 0;



    while (GetByteCount() > m_settings.budgetBytes)
    {
        nextGroup = 1;
        collected = m_entries.size() - m_pendingCount;

        while (nextGroup < collected && !m_entries[nextGroup].info.isWhole)
        {
            nextGroup++;
        }

        if (nextGroup >= collected)
        {
            break;
        }

        for (; nextGroup > 0; nextGroup--)
        {
            m_storedBytes -= m_entries.front().info.storedBytes;
            m_entries.pop_front();
        }
    }
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




