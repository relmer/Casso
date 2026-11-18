#pragma once

#include "Pch.h"

#include "Debugger/Reverse/KeyframeArena.h"
#include "Debugger/Reverse/KeyframePackQueue.h"
#include "Debugger/Reverse/KeyframeTypes.h"

class MachineHost;
class StateWriter;
class IWorkQueue;
struct PackedKeyframe;
class KeyframeUnpacker;





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
//  arena the packed snapshots are laid into end to end, wrapping around, so
//  recording seldom allocates again: only a table that fills while much of
//  the arena is free is laid out again, with slots sized to the keyframes
//  held, so small keyframes use the budget. Over the byte budget, or out of
//  room in the table or the arena, the oldest group is dropped whole, oldest first,
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
//  A keyframe can be given side bytes, kept just after its packed snapshot
//  in the arena and counted with it against the budget: the heat map keeps
//  there the accesses counted since the keyframe before. Each drop listener
//  is called before each keyframe goes, while its side bytes can still be
//  read.
//
////////////////////////////////////////////////////////////////////////////////

class KeyframeStore
{
public:
    //  Reads the host's clock as a UTC FILETIME; tests substitute their own.
    using WallClock = uint64_t (*)();

    //  Called before the oldest or the newest keyframe is dropped, or before
    //  every one is.
    using DropListener = std::function<void (KeyframeDrop drop)>;

    KeyframeStore ();
    ~KeyframeStore();

    KeyframeStore             (const KeyframeStore &) = delete;
    KeyframeStore & operator= (const KeyframeStore &) = delete;

    void      Configure         (const KeyframeSettings & settings);
    HRESULT   ChangeBudget      (size_t budgetBytes, uint64_t keepPosition = UINT64_MAX);
    void      Clear             ();
    void      Release           ();
    void      SetWorkQueue      (IWorkQueue * queue);
    void      SetWallClock      (WallClock clock) { m_wallClock = clock; }
    void      SetDropListener   (const void * owner, DropListener listener);

    bool      IsDue             (uint64_t cycle) const { return cycle >= m_nextDueCycle; }

    HRESULT   Add               (uint64_t position, uint64_t cycle, const std::vector<Byte> & state) { return Add (position, cycle, 0, state); }
    HRESULT   Add               (uint64_t position, uint64_t cycle, size_t journalIndex, const std::vector<Byte> & state, const std::vector<Byte> & side = {});
    HRESULT   Add               (uint64_t position, uint64_t cycle, size_t journalIndex, const StateWriter & writer, const std::vector<Byte> & side = {});
    HRESULT   Capture           (const MachineHost & machine, uint64_t position);
    HRESULT   WaitForPending    ();

    HRESULT   Restore           (size_t index, std::vector<Byte> & outState);
    HRESULT   RestoreAtPosition (uint64_t position, std::vector<Byte> & outState, bool & outIsFound);
    HRESULT   CopyPacked        (uint64_t position, const KeyframeUnpacker & unpacker, PackedKeyframe & outPacked, KeyframeCopy & outCopy);
    HRESULT   CopyBranchEnd     (uint32_t branchId, const KeyframeUnpacker & unpacker, PackedKeyframe & outPacked, KeyframeCopy & outCopy);
    bool      DoesStateMatch    (size_t index, const std::vector<Byte> & state);
    HRESULT   TruncateAfter     (uint64_t cycle);
    HRESULT   DropAfterPosition (uint64_t position);
    void      MarkNewestBoundary()                                 { m_arena.GetNewestInfo().isBoundary = true; }
    void      MarkNewestGap     (uint64_t gapStart);
    void      SetNewestActivity (const StretchActivity & activity) { m_arena.GetNewestInfo().activity = activity; }
    void      MarkNewestLiveEnd ()                                 { m_arena.GetNewestInfo().isLiveEnd = true; }

    //  Branches: a fork at position, with the journal's records from
    //  journalIndex on and the history cycle there; a switch to a branch,
    //  after which PathChanged goes to the listeners; and the branches forked
    //  after a cut, dropped. The arena has the branches left behind.
    static constexpr size_t  kMaxBranches = KeyframeArena::kMaxBranches;

    HRESULT   Fork                    (uint64_t position, uint64_t cycle, InputJournal & journal, size_t journalIndex);
    HRESULT   SwitchTo                (uint32_t branchId, InputJournal & journal);
    void      DropBranchesForkedAfter (uint64_t position) { m_arena.DropBranchesForkedAfter (position); }

    //  Saved history: the store and the input journal's entries laid out
    //  for a file, and the store and the journal rebuilt from them. Import
    //  uses every saved setting but the budget, and drops the oldest groups,
    //  branches first, until the history fits the budget.
    HRESULT   Export            (const InputJournal & journal, SavedKeyframes & outSaved);
    HRESULT   Import            (const SavedKeyframes & saved, InputJournal & journal);

    //  The keyframes stored, and their facts.
    const KeyframeArena     & GetArena        () const { return m_arena; }

    void                      GetSide         (size_t index, const Byte *& outData, size_t & outSize) const;
    size_t                    GetReservedBytes() const;
    const KeyframeSettings  & GetSettings     () const { return m_settings; }
    uint64_t                  GetNextDueCycle () const { return m_nextDueCycle; }

private:
    //  A full table regrows from the arena while more than this fraction of
    //  the arena (one part in kArenaSpareParts) is still free.
    static constexpr size_t  kArenaSpareParts = 4;

    using Job = KeyframePackQueue::Job;

    HRESULT   Reserve           (size_t stateBytes);
    void      ComputeLayout     (size_t stateBytes, size_t & outSlotCount, size_t & outArenaBytes) const;
    HRESULT   TryGrowTable      (bool & outGrew);
    HRESULT   TakeJob           (Job *& outJob);
    HRESULT   SubmitJob         (Job & job, uint64_t position, uint64_t cycle, size_t journalIndex);
    HRESULT   Collect           ();
    HRESULT   DropPending       ();
    HRESULT   ReloadLatestWhole ();
    HRESULT   DropNewerThan     (bool isByPosition, uint64_t limit);
    void      ScheduleAfter     (uint64_t cycle);
    void      NotifyDrop        (KeyframeDrop drop);
    HRESULT   FinishPathChange  ();

    static uint64_t  ReadWallClock();
    static bool      IsValidSaved  (const SavedKeyframes & saved);
    static bool      IsRootedSaved (const SavedKeyframes & saved, size_t s);

    KeyframeSettings                                     m_settings;
    KeyframeArena                                        m_arena;
    KeyframePackQueue                                    m_packQueue;
    uint64_t                                             m_nextDueCycle = 0;
    WallClock                                            m_wallClock    = &ReadWallClock;

    //  Each owner's drop listener, in the order they were set.
    std::vector<std::pair<const void *, DropListener>>   m_dropListeners;
};
