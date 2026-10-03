#pragma once

#include "Pch.h"

#include "Core/IWorkQueue.h"
#include "Debugger/Reverse/SnapshotCompressor.h"

class MachineHost;
class StateWriter;





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeSettings
//
//  How often a keyframe is taken, how many keyframes share one whole
//  snapshot, and how much memory the store may hold.
//
////////////////////////////////////////////////////////////////////////////////

struct KeyframeSettings
{
    static constexpr uint64_t  kFrameCycles        = 17030;
    static constexpr uint64_t  kDefaultFrames      = 10;
    static constexpr uint32_t  kDefaultWholeEvery  = 30;
    static constexpr size_t    kDefaultBudgetBytes = 64ull * 1024 * 1024;

    uint64_t  intervalCycles = kFrameCycles * kDefaultFrames;
    uint32_t  wholeEvery     = kDefaultWholeEvery;
    size_t    budgetBytes    = kDefaultBudgetBytes;
};





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeInfo
//
//  What the store knows about one keyframe without unpacking it.
//
////////////////////////////////////////////////////////////////////////////////

struct KeyframeInfo
{
    uint64_t  position     = 0;      // instructions retired when it was taken
    uint64_t  cycle        = 0;      // CPU cycle count when it was taken
    size_t    journalIndex = 0;      // input journal end index when it was taken
    uint64_t  checksum     = 0;      // of the whole state: RAM, CPU, devices, disk media
    size_t    stateBytes   = 0;      // unpacked size
    size_t    storedBytes  = 0;      // packed size held in memory
    uint64_t  gapStart     = 0;      // with hasGapBefore: where recording paused before it
    bool      isWhole      = false;  // false: XOR difference from its group's whole snapshot
    bool      isBoundary   = false;  // the state does not follow from the one before: a replay loads it
    bool      hasGapBefore = false;  // recording paused between gapStart and this keyframe
};





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeStore
//
//  Whole-machine snapshots taken at fixed cycle boundaries: the whole of
//  reverse execution's history. Any position is reached by restoring the
//  nearest earlier keyframe and replaying forward.
//
//  Snapshots are kept in groups. The first of a group is stored whole; each
//  later one is stored as its byte-wise XOR against that whole snapshot, so
//  everything that did not change, most RAM and every untouched disk track,
//  becomes zero. Every stored snapshot is then packed with SnapshotCompressor.
//  A difference is taken against the group's whole snapshot rather than the
//  previous keyframe, so any restore is at most one unpack of the whole plus
//  one unpack and XOR of the difference. The newest whole snapshot is also
//  kept unpacked, since every difference in the open group needs it.
//
//  A snapshot whose size differs from its group's whole one (a disk mounted
//  or ejected) starts a new group.
//
//  The memory is taken once, on the first Add: a table of keyframes and one
//  arena the packed snapshots are laid into end to end, wrapping round, so
//  recording never allocates again. Over the byte budget, or out of room in
//  the table or the arena, the oldest group is dropped whole, oldest first,
//  because its differences cannot be restored without it. The newest group
//  is never dropped; the arena is never smaller than the newest group can
//  need, so it always fits.
//
//  Given a work queue, the checksum, the XOR and the packing run there: Add
//  copies the state into one of a few buffers kept at full size, lists the
//  keyframe at once with everything but its checksum and packed bytes, and
//  returns. The results are collected on the caller's thread, oldest first,
//  at the next Add or wait. When every buffer is in flight, Add waits for
//  them rather than allocate another. Restore, DoesStateMatch, TruncateAfter
//  and Clear wait for every keyframe in flight first. Without a queue the
//  work runs inside Add. Everything but the work itself is called on one
//  thread, the one that runs the machine.
//
////////////////////////////////////////////////////////////////////////////////

class KeyframeStore
{
public:
    static constexpr size_t  kBufferCount = 2;

              KeyframeStore     () = default;
              ~KeyframeStore    ();

              KeyframeStore     (const KeyframeStore &) = delete;
    KeyframeStore & operator=   (const KeyframeStore &) = delete;

    void      Configure         (const KeyframeSettings & settings);
    void      Clear             ();
    void      Release           ();
    void      SetWorkQueue      (IWorkQueue * queue);

    bool      IsDue             (uint64_t cycle) const { return cycle >= m_nextDueCycle; }

    HRESULT   Add               (uint64_t position, uint64_t cycle, const std::vector<Byte> & state) { return Add (position, cycle, 0, state); }
    HRESULT   Add               (uint64_t position, uint64_t cycle, size_t journalIndex, const std::vector<Byte> & state);
    HRESULT   Add               (uint64_t position, uint64_t cycle, size_t journalIndex, const StateWriter & writer);
    HRESULT   Capture           (const MachineHost & machine, uint64_t position);
    HRESULT   WaitForPending    ();

    HRESULT   Restore           (size_t index, std::vector<Byte> & outState);
    bool      TryFindAtOrBefore (uint64_t cycle, size_t & outIndex) const;
    bool      TryFindByPosition (uint64_t position, size_t & outIndex) const;
    bool      DoesStateMatch    (size_t index, const std::vector<Byte> & state);
    HRESULT   TruncateAfter     (uint64_t cycle);
    void      MarkNewestBoundary()                    { GetEntry (m_count - 1).info.isBoundary = true; }
    void      MarkNewestGap     (uint64_t gapStart);

    size_t                    GetCount        () const { return m_count; }
    size_t                    GetPendingCount () const { return m_pendingCount; }
    const KeyframeInfo      & GetInfo         (size_t index) const { return GetEntry (index).info; }
    size_t                    GetByteCount    () const { return m_storedBytes + m_wholeBytes; }
    size_t                    GetReservedBytes() const;
    size_t                    GetCapacity     () const { return m_entries.size(); }
    const KeyframeSettings  & GetSettings     () const { return m_settings; }
    uint64_t                  GetNextDueCycle () const { return m_nextDueCycle; }

    static uint64_t  ComputeChecksum (const Byte * data, size_t size);

private:
    //  Each table slot is charged this much of the budget, which sets how
    //  many keyframes the table holds: at the default budget, hours of them.
    static constexpr size_t  kBudgetPerEntry = 1024;

    struct Entry
    {
        KeyframeInfo  info;
        size_t        offset = 0;      // where its packed bytes start in the arena
    };

    //  One keyframe's work: the state handed over, and what packing it gave.
    struct Job
    {
        KeyframeStore      * store    = nullptr;
        std::vector<Byte>    state;
        std::vector<Byte>    packed;
        uint64_t             checksum = 0;
        bool                 isWhole  = false;
        HRESULT              hr       = S_OK;
        std::atomic<bool>    isDone   = false;
    };

    Entry       & GetEntry          (size_t index)       { return m_entries[(m_first + index) % m_entries.size()]; }
    const Entry & GetEntry          (size_t index) const { return m_entries[(m_first + index) % m_entries.size()]; }

    HRESULT   Reserve           (size_t stateBytes);
    HRESULT   TakeJob           (Job *& outJob);
    HRESULT   SubmitJob         (Job & job, uint64_t position, uint64_t cycle, size_t journalIndex);
    HRESULT   Collect           ();
    HRESULT   Place             (const std::vector<Byte> & packed, size_t & outOffset);
    bool      TryFindRoom       (size_t size, size_t & outOffset) const;
    void      Pack              (Job & job);
    HRESULT   ReloadLatestWhole ();
    size_t    FindGroupStart    (size_t index) const;
    bool      TryDropOldestGroup();
    void      DropOldestGroups  ();
    void      PopOldest         ();
    void      ScheduleAfter     (uint64_t cycle);

    static void  RunJob   (void * context);
    static void  XorBytes (const Byte * a, const Byte * b, Byte * out, size_t count);

    KeyframeSettings               m_settings;
    std::vector<Entry>             m_entries;              // the table, used as a ring; sized once by Reserve
    size_t                         m_first        = 0;     // slot of the oldest keyframe
    size_t                         m_count        = 0;
    std::unique_ptr<Byte[]>        m_arena;                // packed snapshots, end to end, wrapping round
    size_t                         m_arenaBytes   = 0;
    size_t                         m_arenaEnd     = 0;     // just past the newest packed snapshot
    size_t                         m_storedBytes  = 0;
    size_t                         m_groupLength  = 0;     // keyframes in the newest group
    size_t                         m_wholeBytes   = 0;     // size of the newest whole snapshot
    uint64_t                       m_nextDueCycle = 0;
    IWorkQueue                   * m_queue        = nullptr;
    std::array<Job, kBufferCount>  m_jobs;
    size_t                         m_nextJob      = 0;     // the job the next Add takes
    size_t                         m_pendingCount = 0;     // jobs handed over and not yet collected, the newest entries

    // Used by the work, so touched by the caller's thread only while none is in flight.
    std::vector<Byte>              m_latestWhole;          // unpacked copy of the newest whole snapshot
    std::vector<Byte>              m_scratch;
    std::vector<Byte>              m_olderWhole;           // an older group's whole snapshot, unpacked by Restore
    SnapshotCompressor             m_compressor;
};
