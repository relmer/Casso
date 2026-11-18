#include "Pch.h"

#include "Debugger/Reverse/KeyframeArena.h"
#include "Debugger/Reverse/InputJournal.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::MoveTailToBranch
//
//  The active path's keyframes at and after position move into a new
//  branch, newest first, each sent to the drop callback as the newest
//  dropped, and the journal's records from journalIndex with them. Branches
//  forked from the active path after position now fork from the new branch.
//  The branch's journal entries start at the journal's first index when that
//  is after journalIndex. With nothing unplaced.
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeArena::MoveTailToBranch (
    uint64_t         position,
    uint64_t         cycle,
    InputJournal   & journal,
    size_t           journalIndex,
    uint64_t         wallTime)
{
    size_t  moved = 0;
    size_t  i     = 0;
    Branch  branch;



    while (moved < m_count && GetEntry (m_count - 1 - moved).info.position >= position)
    {
        moved++;
    }

    if (moved == 0)
    {
        journal.Truncate (journalIndex);
        return false;
    }

    branch.entries.resize (moved);

    for (i = moved; i > 0; i--)
    {
        const Entry  & newest = GetEntry (m_count - 1);



        NotifyDrop (KeyframeDrop::Newest);

        branch.entries[i - 1] = newest;

        m_storedBytes -= newest.info.storedBytes;
        m_sideBytes   -= newest.info.sideBytes;
        m_branchBytes += newest.info.storedBytes;
        m_count--;
    }

    branch.info.id              = m_nextBranchId++;
    branch.info.parent          = 0;
    branch.info.forkPosition    = position;
    branch.info.forkCycle       = cycle;
    branch.info.liveEndPosition = branch.entries.back().info.position;
    branch.info.liveEndCycle    = branch.entries.back().info.cycle;
    branch.info.leftAtWallTime  = wallTime;
    branch.firstJournalIndex    = std::max (journalIndex, journal.GetBeginIndex());

    journal.MoveAfter (journalIndex, branch.records);

    for (Branch & each : m_branches)
    {
        if (each.info.parent == 0 && each.info.forkPosition > position)
        {
            each.info.parent = branch.info.id;
        }
    }

    m_branches.push_back (std::move (branch));

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::SwitchTo
//
//  The branch's segments, from the one forked from the active path down to
//  the branch itself, each moved onto the active path in turn: the active
//  path forks where the segment begins, then takes its keyframes and
//  records. The tail of a segment forked from again goes back to a branch,
//  which the user left when the segment was left. The caller reloads the
//  newest group after.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeArena::SwitchTo (
    uint32_t         branchId,
    InputJournal   & journal,
    size_t           spareSlots,
    uint64_t         wallTime)
{
    HRESULT                hr      = S_OK;
    size_t                 index   = FindBranch (branchId);
    uint32_t               id      = branchId;
    uint64_t               leftAt  = 0;
    bool                   isFirst = true;
    bool                   isFound = index < m_branches.size();
    bool                   isMoved = false;
    uint32_t               nextId  = 0;
    std::vector<uint32_t>  path;



    CBRAEx (isFound, E_INVALIDARG);
    CBRA (m_unplaced == 0);

    while (id != 0)
    {
        index   = FindBranch (id);
        isFound = index < m_branches.size();
        CBRA (isFound);

        path.insert (path.begin(), id);
        id = m_branches[index].info.parent;
    }

    m_pathChangePosition = m_branches[FindBranch (path.front())].info.forkPosition;

    for (uint32_t segment : path)
    {
        index   = FindBranch (segment);
        nextId  = m_nextBranchId;
        isFound = index < m_branches.size();

        CBRA (isFound);

        isMoved = MoveTailToBranch (m_branches[index].info.forkPosition, m_branches[index].info.forkCycle, journal, m_branches[index].firstJournalIndex, wallTime);
        IGNORE_RETURN_VALUE (isMoved, false);

        if (!isFirst && m_nextBranchId != nextId)
        {
            m_branches.back().info.leftAtWallTime = leftAt;
        }

        index  = FindBranch (segment);
        leftAt = m_branches[index].info.leftAtWallTime;

        hr = MoveBranchOnPath (index, journal, spareSlots);
        CHR (hr);

        isFirst = false;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::MoveBranchOnPath
//
//  A branch forked from the active path at its newest keyframe's end: its
//  keyframes go after the active path's, its records after the journal's,
//  and the branches forked from it now fork from the active path. The
//  table grows to fit, with nothing dropped.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeArena::MoveBranchOnPath (
    size_t           branchIndex,
    InputJournal   & journal,
    size_t           spareSlots)
{
    HRESULT   hr       = S_OK;
    Branch  * branch   = &m_branches[branchIndex];
    size_t    needed   = m_count + branch->entries.size();
    bool      isAtFork = journal.GetEndIndex() == branch->firstJournalIndex;
    uint32_t  id       = branch->info.id;



    CBRA (isAtFork);

    if (needed > m_entries.size())
    {
        hr = Relayout (needed + spareSlots, m_arenaBytes, SIZE_MAX, UINT64_MAX, false);
        CHR (hr);

        branch = &m_branches[branchIndex];
    }

    for (const Entry & entry : branch->entries)
    {
        GetEntry (m_count) = entry;

        m_storedBytes += entry.info.storedBytes;
        m_sideBytes   += entry.info.sideBytes;
        m_branchBytes -= entry.info.storedBytes;
        m_count++;
    }

    journal.Append (branch->records);

    for (Branch & each : m_branches)
    {
        if (each.info.parent == id)
        {
            each.info.parent = 0;
        }
    }

    m_branches.erase (m_branches.begin() + (ptrdiff_t) branchIndex);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::TryTrimBranch
//
//  The oldest group of the oldest branch with none forked from it goes; a
//  branch of one group goes whole. The branch's keyframe after the group
//  then begins a gap from the fork, since nothing before it can be replayed
//  to; its records stay, so the journal's indices along it stay valid.
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeArena::TryTrimBranch()
{
    size_t    index     = FindOldestLeaf();
    size_t    nextGroup = 1;
    size_t    i         = 0;
    Branch  * branch    = nullptr;



    if (index >= m_branches.size())
    {
        return false;
    }

    branch = &m_branches[index];

    while (nextGroup < branch->entries.size() && !branch->entries[nextGroup].info.isWhole)
    {
        nextGroup++;
    }

    m_isFull = true;

    if (nextGroup >= branch->entries.size())
    {
        DropBranch (index);
        return true;
    }

    for (i = 0; i < nextGroup; i++)
    {
        m_extents.Free (branch->entries[i].offset, branch->entries[i].info.storedBytes);
        m_branchBytes -= branch->entries[i].info.storedBytes;
    }

    branch->entries.erase (branch->entries.begin(), branch->entries.begin() + (ptrdiff_t) nextGroup);

    branch->entries.front().info.isBoundary   = true;
    branch->entries.front().info.hasGapBefore = true;
    branch->entries.front().info.gapStart     = branch->info.forkPosition;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::DropBranch
//
//  Every keyframe of the branch goes, with its records. Branches forked
//  from it fork from its parent instead.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeArena::DropBranch (size_t branchIndex)
{
    Branch  & branch = m_branches[branchIndex];



    for (const Entry & entry : branch.entries)
    {
        m_extents.Free (entry.offset, entry.info.storedBytes);
        m_branchBytes -= entry.info.storedBytes;
    }

    for (Branch & each : m_branches)
    {
        if (each.info.parent == branch.info.id)
        {
            each.info.parent = branch.info.parent;
        }
    }

    m_branches.erase (m_branches.begin() + (ptrdiff_t) branchIndex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::DropAllBranches
//
//  The table is the active path's alone, so dropping a branch doesn't free a slot:
//  every branch goes before the active path's oldest group can.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeArena::DropAllBranches()
{
    while (!m_branches.empty())
    {
        DropBranch (FindOldestLeaf());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::DropBranchesForkedAfter
//
//  Every branch forked from the path after position goes, with each forked
//  from one of those, since its fork is no longer on the path. A dropped
//  branch's children are moved to its parent and fork after its fork, so the
//  scan starts again until none is left past position.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeArena::DropBranchesForkedAfter (uint64_t position)
{
    size_t  i = 0;



    while (i < m_branches.size())
    {
        if (m_branches[i].info.parent == 0 && m_branches[i].info.forkPosition > position)
        {
            DropBranch (i);
            i = 0;
        }
        else
        {
            i++;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::CapBranches
//
//  More than kMaxBranches branches drops the oldest with none forked from
//  it, whole, until kMaxBranches are left.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeArena::CapBranches()
{
    while (m_branches.size() > kMaxBranches)
    {
        DropBranch (FindOldestLeaf());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::GetBranches
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeArena::GetBranches (std::vector<KeyframeBranch> & outBranches) const
{
    outBranches.clear();

    for (const Branch & branch : m_branches)
    {
        outBranches.push_back (branch.info);
        outBranches.back().keyframeCount = branch.entries.size();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::HasChildBranch
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeArena::HasChildBranch (uint32_t id) const
{
    return std::any_of (m_branches.begin(), m_branches.end(), [id] (const Branch & each) { return each.info.parent == id; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::FindOldestLeaf
//
//  The index of the branch the user left longest ago with none forked from
//  it, or the branch count with none left. Every tree of branches has one.
//
////////////////////////////////////////////////////////////////////////////////

size_t KeyframeArena::FindOldestLeaf() const
{
    size_t  oldest = m_branches.size();
    size_t  i      = 0;



    for (i = 0; i < m_branches.size(); i++)
    {
        if (HasChildBranch (m_branches[i].info.id))
        {
            continue;
        }

        if (oldest == m_branches.size() || m_branches[i].info.leftAtWallTime < m_branches[oldest].info.leftAtWallTime)
        {
            oldest = i;
        }
    }

    return oldest;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::FindBranch
//
//  The index of the branch with id, or the branch count with none.
//
////////////////////////////////////////////////////////////////////////////////

size_t KeyframeArena::FindBranch (uint32_t id) const
{
    size_t  i = 0;



    while (i < m_branches.size() && m_branches[i].info.id != id)
    {
        i++;
    }

    return i;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::TryGetBranchEnd
//
//  The whole snapshot is the branch's newest whole keyframe, or for a
//  branch whose first group began before the fork, the whole snapshot of
//  the active path's group just before the fork.
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeArena::TryGetBranchEnd (
    uint32_t                 branchId,
    KeyframeInfo           & outWhole,
    KeyframeCodec::Packed  & outWholePacked,
    KeyframeInfo           & outEnd,
    KeyframeCodec::Packed  & outEndPacked) const
{
    size_t           index    = FindBranch (branchId);
    size_t           k        = 0;
    size_t           before   = 0;
    bool             isOnPath = false;
    const Branch   * branch   = nullptr;
    const Entry    * entry    = nullptr;
    const Entry    * whole    = nullptr;



    if (index >= m_branches.size() || m_branches[index].entries.empty())
    {
        return false;
    }

    branch = &m_branches[index];
    k      = branch->entries.size() - 1;
    entry  = &branch->entries[k];

    while (k > 0 && !branch->entries[k].info.isWhole)
    {
        k--;
    }

    if (branch->entries[k].info.isWhole)
    {
        whole = &branch->entries[k];
    }
    else if (branch->info.parent == 0)
    {
        isOnPath = branch->info.forkPosition > 0 && TryFindByPosition (branch->info.forkPosition - 1, before);
        whole    = isOnPath ? &GetEntry (FindGroupStart (before)) : nullptr;
    }

    if (whole == nullptr || whole->info.stateBytes != entry->info.stateBytes)
    {
        return false;
    }

    outWhole       = whole->info;
    outWholePacked = GetEntryPacked (*whole);
    outEnd         = entry->info;
    outEndPacked   = GetEntryPacked (*entry);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::Export
//
//  The active path, then each branch left behind. Every keyframe's stored
//  bytes are copied end to end, so the file has none of the arena's free
//  space.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeArena::Export (SavedKeyframes & outSaved) const
{
    size_t                i = 0;
    SavedKeyframeSegment  segment;



    outSaved.nextBranchId = m_nextBranchId;
    outSaved.arena.reserve (m_storedBytes + m_branchBytes);

    for (i = 0; i < m_count; i++)
    {
        ExportEntry (GetEntry (i), segment, outSaved.arena);
    }

    outSaved.segments.push_back (std::move (segment));

    for (const Branch & branch : m_branches)
    {
        segment                   = SavedKeyframeSegment();
        segment.info              = branch.info;
        segment.firstJournalIndex = branch.firstJournalIndex;
        segment.records           = branch.records;

        for (const Entry & entry : branch.entries)
        {
            ExportEntry (entry, segment, outSaved.arena);
        }

        outSaved.segments.push_back (std::move (segment));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::ExportEntry
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeArena::ExportEntry (
    const Entry           & entry,
    SavedKeyframeSegment  & segment,
    std::vector<Byte>     & arena) const
{
    const Byte  * start = m_arena.get() + entry.offset;



    segment.infos.push_back   (entry.info);
    segment.offsets.push_back (arena.size());

    arena.insert (arena.end(), start, start + entry.info.storedBytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::Import
//
//  The saved arena becomes the arena, the active path the table and the
//  rest the branches, all of it allocated. Call it on a cleared arena, with
//  a saved history KeyframeStore::IsValidSaved accepts.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeArena::Import (const SavedKeyframes & saved)
{
    HRESULT   hr        = S_OK;
    size_t    offset    = 0;
    size_t    i         = 0;
    size_t    s         = 0;
    bool      isPlaced  = false;
    bool      isCleared = m_count == 0 && m_branches.empty();
    Byte    * arena     = nullptr;
    Entry     entry;
    Branch    branch;



    CBRA (isCleared);

    m_arena = AllocateArena (saved.arena.size());
    arena   = m_arena.get();
    CPRA (arena);

    memcpy (arena, saved.arena.data(), saved.arena.size());

    m_arenaBytes = saved.arena.size();

    m_entries.assign (saved.segments[0].infos.size(), Entry());
    m_first = 0;

    for (i = 0; i < saved.segments[0].infos.size(); i++)
    {
        entry.info   = saved.segments[0].infos[i];
        entry.offset = saved.segments[0].offsets[i];

        m_entries[i]   = entry;
        m_storedBytes += entry.info.storedBytes;
        m_sideBytes   += entry.info.sideBytes;
    }

    m_count = saved.segments[0].infos.size();

    for (s = 1; s < saved.segments.size(); s++)
    {
        const SavedKeyframeSegment  & segment = saved.segments[s];



        branch                    = Branch();
        branch.info               = segment.info;
        branch.info.keyframeCount = segment.infos.size();
        branch.firstJournalIndex  = segment.firstJournalIndex;
        branch.records            = segment.records;

        for (i = 0; i < segment.infos.size(); i++)
        {
            entry.info   = segment.infos[i];
            entry.offset = segment.offsets[i];

            branch.entries.push_back (entry);
            m_branchBytes += entry.info.storedBytes;
        }

        m_branches.push_back (std::move (branch));
    }

    m_nextBranchId = saved.nextBranchId;

    m_extents.Reset (m_arenaBytes, GetExtentRoom());

    isPlaced = m_extents.TryAllocate (m_arenaBytes, offset);
    CBRA (isPlaced && offset == 0);

Error:
    return hr;
}
