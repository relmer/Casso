#include "Pch.h"

#include "Debugger/Reverse/KeyframeStore.h"
#include "Debugger/Reverse/KeyframeUnpacker.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Fork
//
//  The keyframes at and after position, and the journal's records from
//  journalIndex on, become a branch left behind; cycle is the history cycle
//  at position. With no keyframe there, no branch is made, and the
//  records go as a cut drops them. More than kMaxBranches branches then
//  drops the oldest.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::Fork (
    uint64_t         position,
    uint64_t         cycle,
    InputJournal   & journal,
    size_t           journalIndex)
{
    HRESULT  hr = S_OK;



    hr = MoveTailToBranch (position, cycle, journal, journalIndex);
    CHR (hr);

    CapBranches();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SwitchTo
//
//  The branch's segments, from the one forked from the active path down to
//  the branch itself, each moved onto the active path in turn: the active
//  path forks where the segment begins, then takes its keyframes and
//  records. The tail of a segment forked from again goes back to a branch,
//  which the user left when the segment was left. PathChanged then goes to
//  the listeners, with the first fork position.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::SwitchTo (
    uint32_t         branchId,
    InputJournal   & journal)
{
    HRESULT                hr      = S_OK;
    size_t                 index   = FindBranch (branchId);
    uint32_t               id      = branchId;
    uint64_t               leftAt  = 0;
    bool                   isFirst = true;
    bool                   isFound = index < m_branches.size();
    uint32_t               nextId  = 0;
    std::vector<uint32_t>  path;



    CBRAEx (isFound, E_INVALIDARG);

    hr = WaitForPending();
    CHR (hr);

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

        hr = MoveTailToBranch (m_branches[index].info.forkPosition, m_branches[index].info.forkCycle, journal, m_branches[index].firstJournalIndex);
        CHR (hr);

        if (!isFirst && m_nextBranchId != nextId)
        {
            m_branches.back().info.leftAtWallTime = leftAt;
        }

        index  = FindBranch (segment);
        leftAt = m_branches[index].info.leftAtWallTime;

        hr = MoveBranchOnPath (index, journal);
        CHR (hr);

        isFirst = false;
    }

    NotifyDrop (KeyframeDrop::PathChanged);

    CapBranches();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBranches
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::GetBranches (std::vector<KeyframeBranch> & outBranches) const
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
//  MoveTailToBranch
//
//  The active path's keyframes at and after position move into a new
//  branch, newest first, each sent to the listeners as the newest dropped,
//  and the journal's records from journalIndex with them. Branches forked
//  from the active path after position now fork from the new branch. The
//  newest group left is reloaded, as after a cut.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::MoveTailToBranch (
    uint64_t         position,
    uint64_t         cycle,
    InputJournal   & journal,
    size_t           journalIndex)
{
    HRESULT  hr     = S_OK;
    size_t   moved  = 0;
    size_t   i      = 0;
    Branch   branch;



    hr = WaitForPending();
    CHR (hr);

    while (moved < m_count && GetEntry (m_count - 1 - moved).info.position >= position)
    {
        moved++;
    }

    if (moved == 0)
    {
        journal.Truncate (journalIndex);
        BAIL_OUT_IF (true, S_OK);
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
    branch.info.leftAtWallTime  = m_wallClock();
    branch.firstJournalIndex    = journalIndex;

    journal.MoveAfter (journalIndex, branch.records);

    for (Branch & each : m_branches)
    {
        if (each.info.parent == 0 && each.info.forkPosition > position)
        {
            each.info.parent = branch.info.id;
        }
    }

    m_branches.push_back (std::move (branch));

    if (m_count == 0)
    {
        ResetGroup();
        BAIL_OUT_IF (true, S_OK);
    }

    hr = ReloadLatestWhole();
    CHR (hr);

    ScheduleAfter (GetEntry (m_count - 1).info.cycle);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MoveBranchOnPath
//
//  A branch forked from the active path at its newest keyframe's end: its
//  keyframes go after the active path's, its records after the journal's,
//  and the branches forked from it now fork from the active path. The
//  table grows to fit, with nothing dropped.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::MoveBranchOnPath (
    size_t           branchIndex,
    InputJournal   & journal)
{
    HRESULT   hr       = S_OK;
    Branch  & branch   = m_branches[branchIndex];
    size_t    needed   = m_count + branch.entries.size();
    bool      isAtFork = journal.GetEndIndex() == branch.firstJournalIndex;



    CBRA (isAtFork);

    if (needed > m_entries.size())
    {
        hr = Relayout (needed + GetGroupSlots(), m_arenaBytes, UINT64_MAX, false);
        CHR (hr);
    }

    for (const Entry & entry : branch.entries)
    {
        GetEntry (m_count) = entry;

        m_storedBytes += entry.info.storedBytes;
        m_sideBytes   += entry.info.sideBytes;
        m_branchBytes -= entry.info.storedBytes;
        m_count++;
    }

    journal.Append (branch.records);

    for (Branch & each : m_branches)
    {
        if (each.info.parent == branch.info.id)
        {
            each.info.parent = 0;
        }
    }

    m_branches.erase (m_branches.begin() + (ptrdiff_t) branchIndex);

    hr = ReloadLatestWhole();
    CHR (hr);

    ScheduleAfter (GetEntry (m_count - 1).info.cycle);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryTrimBranch
//
//  The oldest group of the oldest branch with none forked from it goes; a
//  branch of one group goes whole. The branch's keyframe after the group
//  then begins a gap from the fork, since nothing before it can be replayed
//  to; its records stay, so the journal's indices along it stay valid.
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeStore::TryTrimBranch()
{
    size_t    index     = FindOldestLeaf();
    size_t    nextGroup = 1;
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

    for (size_t i = 0; i < nextGroup; i++)
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
//  DropBranch
//
//  Every keyframe of the branch goes, with its records. Branches forked
//  from it fork from its parent instead.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::DropBranch (size_t branchIndex)
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
//  CapBranches
//
//  More than kMaxBranches branches drops the oldest with none forked from
//  it, whole, until kMaxBranches are left.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::CapBranches()
{
    while (m_branches.size() > kMaxBranches)
    {
        DropBranch (FindOldestLeaf());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasChildBranch
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeStore::HasChildBranch (uint32_t id) const
{
    return std::any_of (m_branches.begin(), m_branches.end(), [id] (const Branch & each) { return each.info.parent == id; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindOldestLeaf
//
//  The index of the branch the user left longest ago with none forked from
//  it, or the branch count with none left. Every tree of branches has one.
//
////////////////////////////////////////////////////////////////////////////////

size_t KeyframeStore::FindOldestLeaf() const
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
//  FindBranch
//
//  The index of the branch with id, or the branch count with none.
//
////////////////////////////////////////////////////////////////////////////////

size_t KeyframeStore::FindBranch (uint32_t id) const
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
//  ResetGroup
//
//  No keyframe is left on the active path: the next one opens a group.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStore::ResetGroup()
{
    m_latestWhole.clear();

    m_groupLength  = 0;
    m_wholeBytes   = 0;
    m_groupWhole   = 0;
    m_groupDiffs   = 0;
    m_nextDueCycle = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CopyBranchEnd
//
//  As CopyPacked, for the live end of a branch left behind: the newest of
//  its keyframes, against the whole snapshot of its group, in the branch or
//  on the active path before the fork. Gone for a branch not in the store, or
//  one whose group's whole snapshot is in another branch.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeStore::CopyBranchEnd (
    uint32_t                  branchId,
    const KeyframeUnpacker  & unpacker,
    PackedKeyframe          & outPacked,
    KeyframeCopy            & outCopy)
{
    HRESULT          hr       = S_OK;
    size_t           index    = FindBranch (branchId);
    size_t           k        = 0;
    size_t           before   = 0;
    bool             isOnPath = false;
    const Branch   * branch   = nullptr;
    const Entry    * entry    = nullptr;
    const Entry    * whole    = nullptr;
    const Byte     * bytes    = nullptr;



    outCopy = KeyframeCopy::Gone;

    BAIL_OUT_IF (index >= m_branches.size() || m_branches[index].entries.empty(), S_OK);

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

    BAIL_OUT_IF (whole == nullptr || whole->info.stateBytes != entry->info.stateBytes, S_OK);

    outCopy = KeyframeCopy::Copied;

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
