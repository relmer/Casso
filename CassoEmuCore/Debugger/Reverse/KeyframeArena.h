#pragma once

#include "Pch.h"

#include "Debugger/Reverse/ArenaExtents.h"
#include "Debugger/Reverse/KeyframeCodec.h"
#include "Debugger/Reverse/KeyframeTypes.h"

class InputJournal;





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena
//
//  The keyframes a store has: a table of their facts used as a ring, oldest
//  first, and one arena their packed snapshots are laid into, each in the
//  first free extent it fits (ArenaExtents) and followed by its side bytes.
//  The newest entries can be listed before their bytes arrive (List);
//  PlaceNext lays the bytes in for the oldest of those, in the order they
//  were listed. A group is a whole snapshot and the differences against it,
//  and is dropped whole, oldest first, since its differences cannot be
//  restored without it. The newest group is never dropped.
//
//  History branches. The table is the active path, the keyframes a step or
//  a seek moves through, and every index is of a keyframe on it. A fork at
//  a position moves the keyframes from there on, and the input journal's
//  records from the fork's journal index on, into a branch left behind;
//  their packed bytes stay where they are in the arena, and a difference in
//  the branch stays against its group's whole snapshot, which is on the path
//  before the fork. A switch to a branch moves the branch's keyframes and
//  records back onto the active path. Room is made by trimming the oldest
//  group of the oldest branch with none forked from it, before any keyframe
//  on the active path; the active path's oldest group is dropped only with
//  no branch left, so every branch's whole snapshots stay. More than
//  kMaxBranches branches drops the oldest whole.
//
//  Touched by the caller's thread only.
//
////////////////////////////////////////////////////////////////////////////////

class KeyframeArena
{
public:
    //  Called before the oldest or the newest keyframe is dropped, or before
    //  every one is, while its side bytes can be read.
    using DropCallback = std::function<void (KeyframeDrop drop)>;

    KeyframeArena();
    ~KeyframeArena();

    KeyframeArena (const KeyframeArena &)             = delete;
    KeyframeArena & operator= (const KeyframeArena &) = delete;

    void     SetDropCallback (DropCallback callback) { m_onDrop = std::move (callback); }

    //  The table and arena, taken once; IsAllocated is false until then.
    HRESULT  Allocate    (size_t slotCount, size_t arenaBytes);
    bool     IsAllocated () const { return !m_entries.empty(); }

    //  Moves the keyframes into a table and arena of the sizes given, the
    //  oldest groups dropped first until what is stored fits them and the
    //  budget; without isDropping nothing is dropped, and the table and
    //  arena grow to fit. With nothing unplaced.
    HRESULT  Relayout (size_t slotCount, size_t arenaBytes, size_t budgetBytes, uint64_t keepPosition = UINT64_MAX, bool isDropping = true);

    //  Lists the next keyframe, its bytes to come. A whole one opens the
    //  newest group.
    KeyframeInfo  & List (const KeyframeInfo & info);

    //  Lays the packed bytes and side bytes in for the oldest unplaced
    //  keyframe, dropping the oldest groups for room. The side bytes are left
    //  out when nothing more can be dropped for them, so a keyframe is never
    //  lost to them.
    HRESULT  PlaceNext (const std::vector<Byte> & packed, const std::vector<Byte> & side, uint64_t checksum, bool & outSideDropped);

    //  The unplaced keyframes go, after their packing failed.
    void     DropUnplaced();

    bool     TryDropOldestGroup (uint64_t keepPosition = UINT64_MAX);
    void     DropOldestGroups   (size_t budgetBytes);
    bool     DropNewerThan      (bool isByPosition, uint64_t limit);

    //  The newest group's counts, worked out again after a drop of the
    //  newest keyframes; and set to none, for an active path left empty.
    void     RecountNewestGroup();
    void     ResetGroup        ();

    void     Clear  ();
    void     Free   ();

    size_t               GetCount         () const { return m_count; }
    size_t               GetUnplacedCount () const { return m_unplaced; }
    size_t               GetPlacedCount   () const { return m_count - m_unplaced; }
    const KeyframeInfo & GetInfo          (size_t index) const { return GetEntry (index).info; }
    KeyframeInfo       & GetNewestInfo    ()                   { return GetEntry (m_count - 1).info; }

    //  A placed keyframe's packed snapshot and side bytes.
    KeyframeCodec::Packed  GetPacked (size_t index) const;
    void                   GetSide   (size_t index, const Byte *& outData, size_t & outSize) const;

    bool     TryFindAtOrBefore (uint64_t cycle, size_t & outIndex) const;
    bool     TryFindByPosition (uint64_t position, size_t & outIndex) const;
    size_t   FindGroupStart    (size_t index) const;

    size_t   GetSideByteCount  () const { return m_sideBytes; }
    size_t   GetStoredBytes    () const { return m_storedBytes; }
    size_t   GetByteCount      () const { return m_storedBytes + m_branchBytes + m_wholeBytes; }
    size_t   GetBranchBytes    () const { return m_branchBytes; }
    size_t   GetUsedBytes      (size_t budgetBytes) const;
    bool     IsFull            () const { return m_isFull; }
    void     SetHasRoom        ()       { m_isFull = false; }
    size_t   GetCapacity       () const { return m_entries.size(); }
    size_t   GetTableBytes     () const { return m_entries.capacity() * sizeof (Entry); }
    size_t   GetArenaBytes     () const { return m_arenaBytes; }

    //  The newest group: its keyframes, the unpacked size of its whole
    //  snapshot, and the packed sizes of its whole snapshot and of the
    //  differences placed against it.
    size_t   GetGroupLength    () const { return m_groupLength; }
    size_t   GetWholeBytes     () const { return m_wholeBytes; }
    size_t   GetGroupWhole     () const { return m_groupWhole; }
    size_t   GetGroupDiffs     () const { return m_groupDiffs; }

    static constexpr size_t  GetEntryBytes() { return sizeof (Entry); }

    //  Branches. A fork at position moves the active path's keyframes from
    //  there on, with the journal's records from journalIndex on, into a
    //  branch left at wallTime; with no keyframe there, no branch is made,
    //  the records go as a cut drops them, and the result is false. A switch
    //  moves a branch onto the active path, segment by segment along its
    //  ancestors, growing the table to spareSlots past the keyframes moved; a tail
    //  moved off the path on the way is left at wallTime.
    static constexpr size_t  kMaxBranches = 8;

    bool     MoveTailToBranch        (uint64_t position, uint64_t cycle, InputJournal & journal, size_t journalIndex, uint64_t wallTime);
    HRESULT  SwitchTo                (uint32_t branchId, InputJournal & journal, size_t spareSlots, uint64_t wallTime);
    void     CapBranches             ();
    void     DropAllBranches         ();
    void     DropBranchesForkedAfter (uint64_t position);
    void     GetBranches             (std::vector<KeyframeBranch> & outBranches) const;
    size_t   GetBranchCount          () const { return m_branches.size(); }
    bool     HasBranch               (uint32_t id) const { return FindBranch (id) < m_branches.size(); }
    uint64_t GetPathChangePosition   () const { return m_pathChangePosition; }

    //  The live end of a branch left behind, the newest of its keyframes, and
    //  the whole snapshot of its group, in the branch or on the active path
    //  before the fork; false for a branch not here, or one whose group's
    //  whole snapshot is in another branch.
    bool     TryGetBranchEnd (uint32_t branchId, KeyframeInfo & outWhole, KeyframeCodec::Packed & outWholePacked, KeyframeInfo & outEnd, KeyframeCodec::Packed & outEndPacked) const;

    //  Saved history: the active path's keyframes and the branches, with
    //  every keyframe's stored bytes end to end; the journal's records along
    //  the active path are the store's to fill. Import lays the arena out as
    //  saved, with nothing dropped; it is laid out again from the budget
    //  after.
    void     Export (SavedKeyframes & outSaved) const;
    HRESULT  Import (const SavedKeyframes & saved);

private:
    struct ArenaFree
    {
        void  operator() (Byte * arena) const noexcept { VirtualFree (arena, 0, MEM_RELEASE); }
    };

    using ArenaPtr = std::unique_ptr<Byte[], ArenaFree>;

    struct Entry
    {
        KeyframeInfo  info;
        size_t        offset = 0;      // where its packed bytes start in the arena
    };

    Entry        & GetEntry (size_t index)       { return m_entries[(m_first + index) % m_entries.size()]; }
    const Entry  & GetEntry (size_t index) const { return m_entries[(m_first + index) % m_entries.size()]; }

    //  A branch left behind: its keyframes, oldest first, and the journal's
    //  records from its fork on, the first at firstJournalIndex.
    struct Branch
    {
        KeyframeBranch             info;
        std::vector<Entry>         entries;
        std::vector<InputRecord>   records;
        size_t                     firstJournalIndex = 0;
    };

    HRESULT  Place            (const std::vector<Byte> & packed, const std::vector<Byte> & side, size_t & outOffset);
    void     PopOldest        ();
    void     NotifyDrop       (KeyframeDrop drop);
    size_t   GetExtentRoom    () const { return 2 * m_entries.size(); }
    HRESULT  MoveBranchOnPath (size_t branchIndex, InputJournal & journal, size_t spareSlots);
    bool     TryTrimBranch    ();
    void     DropBranch       (size_t branchIndex);
    bool     HasChildBranch   (uint32_t id) const;
    size_t   FindOldestLeaf   () const;
    size_t   FindBranch       (uint32_t id) const;
    void     ExportEntry      (const Entry & entry, SavedKeyframeSegment & segment, std::vector<Byte> & arena) const;

    KeyframeCodec::Packed  GetEntryPacked (const Entry & entry) const;

    static size_t    GetPackedBytes (const KeyframeInfo & info) { return info.storedBytes - info.sideBytes; }
    static ArenaPtr  AllocateArena  (size_t bytes);

    std::vector<Entry>  m_entries;              // the table, used as a ring
    size_t              m_first       = 0;      // slot of the oldest keyframe
    size_t              m_count       = 0;
    size_t              m_unplaced    = 0;      // the newest keyframes, listed without their bytes
    ArenaPtr            m_arena;
    size_t              m_arenaBytes  = 0;
    ArenaExtents        m_extents;              // the arena's free space
    size_t              m_storedBytes = 0;
    size_t              m_sideBytes   = 0;      // of m_storedBytes, the side bytes
    size_t              m_groupLength = 0;
    size_t              m_wholeBytes  = 0;
    size_t              m_groupWhole  = 0;
    size_t              m_groupDiffs  = 0;
    bool                m_isFull      = false;  // the oldest were dropped for room since the arena last had room to spare
    DropCallback        m_onDrop;

    std::vector<Branch>  m_branches;                     // the branches left behind, oldest first
    size_t               m_branchBytes        = 0;       // the stored bytes of their keyframes
    uint32_t             m_nextBranchId       = 1;
    uint64_t             m_pathChangePosition = 0;       // where the active path changed at the last switch
};
