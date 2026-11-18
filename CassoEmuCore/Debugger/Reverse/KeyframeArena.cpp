#include "Pch.h"

#include "Debugger/Reverse/KeyframeArena.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::KeyframeArena
//
////////////////////////////////////////////////////////////////////////////////

KeyframeArena::KeyframeArena()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::~KeyframeArena
//
////////////////////////////////////////////////////////////////////////////////

KeyframeArena::~KeyframeArena()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::Allocate
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeArena::Allocate (size_t slotCount, size_t arenaBytes)
{
    HRESULT  hr = S_OK;



    m_arena = AllocateArena (arenaBytes);
    CPRA (m_arena);

    m_arenaBytes = arenaBytes;
    m_entries.resize (slotCount);
    m_extents.Reset (arenaBytes, GetExtentRoom());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::Relayout
//
//  The oldest groups are dropped until what is stored fits the sizes given
//  and the budget, then the keyframes kept are copied, oldest first, to the
//  start of the new arena. The newest group is never dropped, and its side
//  bytes are not bounded by the least arena, so the table and arena grow to
//  fit what is left when it is more than the sizes given, and so do the
//  groups left in for keepPosition. The branches' keyframes are copied after
//  the active path's. Without isDropping nothing is dropped, and the table
//  and arena grow to fit.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeArena::Relayout (
    size_t    slotCount,
    size_t    arenaBytes,
    size_t    budgetBytes,
    uint64_t  keepPosition,
    bool      isDropping)
{
    HRESULT             hr        = S_OK;
    bool                isDropped = true;
    bool                isPlaced  = false;
    size_t              end       = 0;
    size_t              offset    = 0;
    size_t              i         = 0;
    std::vector<Entry>  entries;
    ArenaPtr            arena;



    CBRA (m_unplaced == 0);

    while (isDropping && isDropped && (GetByteCount() > budgetBytes || m_storedBytes + m_branchBytes > arenaBytes || m_count > slotCount))
    {
        isDropped = TryDropOldestGroup (keepPosition);
    }

    arenaBytes = (std::max) (arenaBytes, m_storedBytes + m_branchBytes);
    slotCount  = (std::max) (slotCount, m_count);

    arena = AllocateArena (arenaBytes);
    CPRA (arena);

    entries.resize (slotCount);

    for (i = 0; i < m_count; i++)
    {
        entries[i]        = GetEntry (i);
        entries[i].offset = end;

        if (entries[i].info.storedBytes != 0)
        {
            memcpy (arena.get() + end, m_arena.get() + GetEntry (i).offset, entries[i].info.storedBytes);
        }

        end += entries[i].info.storedBytes;
    }

    //  The branches' keyframes follow the active path's.
    for (Branch & branch : m_branches)
    {
        for (Entry & entry : branch.entries)
        {
            if (entry.info.storedBytes != 0)
            {
                memcpy (arena.get() + end, m_arena.get() + entry.offset, entry.info.storedBytes);
            }

            entry.offset  = end;
            end          += entry.info.storedBytes;
        }
    }

    m_entries.swap (entries);
    m_arena.swap (arena);

    m_arenaBytes = arenaBytes;
    m_first      = 0;

    m_extents.Reset (arenaBytes, GetExtentRoom());

    if (end > 0)
    {
        isPlaced = m_extents.TryAllocate (end, offset);
        CBRA (isPlaced && offset == 0);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::List
//
//  The table has a free slot: the caller grows it or drops a group first.
//
////////////////////////////////////////////////////////////////////////////////

KeyframeInfo & KeyframeArena::List (const KeyframeInfo & info)
{
    Entry  & entry = GetEntry (m_count);



    entry      = Entry();
    entry.info = info;
    m_count++;
    m_unplaced++;

    if (info.isWhole)
    {
        m_wholeBytes  = info.stateBytes;
        m_groupLength = 1;
    }
    else
    {
        m_groupLength++;
    }

    return entry.info;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::PlaceNext
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeArena::PlaceNext (
    const std::vector<Byte>  & packed,
    const std::vector<Byte>  & side,
    uint64_t                   checksum,
    bool                     & outSideDropped)
{
    HRESULT                  hr     = S_OK;
    size_t                   offset = 0;
    Entry                  * entry  = nullptr;
    const std::vector<Byte>  noSide;



    outSideDropped = false;

    CBRA (m_unplaced > 0);

    hr = Place (packed, side, offset);

    if (hr == E_OUTOFMEMORY && !side.empty())
    {
        outSideDropped = true;

        hr = Place (packed, noSide, offset);
    }

    CHR (hr);

    entry = &GetEntry (m_count - m_unplaced);

    entry->info.checksum    = checksum;
    entry->info.sideBytes   = outSideDropped ? 0 : side.size();
    entry->info.storedBytes = packed.size() + entry->info.sideBytes;
    entry->offset           = offset;

    m_storedBytes += entry->info.storedBytes;
    m_sideBytes   += entry->info.sideBytes;
    m_unplaced--;

    if (entry->info.isWhole)
    {
        m_groupWhole = packed.size();
        m_groupDiffs = 0;
    }
    else
    {
        m_groupDiffs += packed.size();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::DropUnplaced
//
//  The newest keyframes, listed without their bytes, go: a failed packing,
//  and every one after it, which may be a difference against it.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeArena::DropUnplaced()
{
    while (m_unplaced > 0)
    {
        NotifyDrop (KeyframeDrop::Newest);

        m_count--;
        m_unplaced--;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::Place
//
//  Copies packed, then side, into the first free extent of the arena they
//  fit in, dropping the oldest groups until they fit. The arena always
//  has room for the snapshots of the newest group, so only side bytes can
//  fail to fit, which is E_OUTOFMEMORY without an assert.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeArena::Place (
    const std::vector<Byte>  & packed,
    const std::vector<Byte>  & side,
    size_t                   & outOffset)
{
    HRESULT  hr        = S_OK;
    size_t   size      = packed.size() + side.size();
    bool     hasRoom   = m_extents.TryAllocate (size, outOffset);
    bool     hasSide   = !side.empty();
    bool     isDropped = false;



    while (!hasRoom)
    {
        isDropped = TryDropOldestGroup();
        BAIL_OUT_IF (!isDropped && hasSide, E_OUTOFMEMORY);
        CBRAEx (isDropped, E_OUTOFMEMORY);

        hasRoom = m_extents.TryAllocate (size, outOffset);
    }

    if (!packed.empty())
    {
        memcpy (m_arena.get() + outOffset, packed.data(), packed.size());
    }

    if (hasSide)
    {
        memcpy (m_arena.get() + outOffset + packed.size(), side.data(), side.size());
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::TryDropOldestGroup
//
//  Drops the oldest group, its whole snapshot and every difference against
//  it, when a placed whole snapshot follows it; false when none does, so
//  the newest group and every unplaced keyframe stay. False too when the
//  next group starts past keepPosition, so the keyframe at or before it
//  stays. While any branch is left, a branch is trimmed instead, so the
//  whole snapshots the branches' differences are against stay.
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeArena::TryDropOldestGroup (uint64_t keepPosition)
{
    size_t  placed    = GetPlacedCount();
    size_t  nextGroup = 1;
    bool    isDropped = false;



    if (!m_branches.empty())
    {
        return TryTrimBranch();
    }

    while (nextGroup < placed && !GetEntry (nextGroup).info.isWhole)
    {
        nextGroup++;
    }

    isDropped = nextGroup < placed && GetEntry (nextGroup).info.position <= keepPosition;

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
//  KeyframeArena::DropOldestGroups
//
//  While over the budget, drops the oldest group. The newest group stays
//  even over budget.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeArena::DropOldestGroups (size_t budgetBytes)
{
    bool  isDropped = true;



    while (isDropped && GetByteCount() > budgetBytes)
    {
        isDropped = TryDropOldestGroup();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::DropNewerThan
//
//  The newest keyframes whose position, or cycle, is past limit go, and the
//  arena has room again. With every keyframe gone the arena is cleared.
//  Returns whether any went. With nothing unplaced.
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeArena::DropNewerThan (bool isByPosition, uint64_t limit)
{
    bool  isDropped = false;



    while (m_count > 0 && (isByPosition ? GetEntry (m_count - 1).info.position : GetEntry (m_count - 1).info.cycle) > limit)
    {
        NotifyDrop (KeyframeDrop::Newest);

        m_extents.Free (GetEntry (m_count - 1).offset, GetEntry (m_count - 1).info.storedBytes);

        m_storedBytes -= GetEntry (m_count - 1).info.storedBytes;
        m_sideBytes   -= GetEntry (m_count - 1).info.sideBytes;
        m_count--;
        isDropped = true;
    }

    if (!isDropped)
    {
        return false;
    }

    m_isFull = false;

    if (m_count == 0)
    {
        Clear();
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::RecountNewestGroup
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeArena::RecountNewestGroup()
{
    size_t  groupStart = FindGroupStart (m_count - 1);
    size_t  i          = 0;



    m_groupLength = m_count - groupStart;
    m_wholeBytes  = GetEntry (groupStart).info.stateBytes;
    m_groupWhole  = GetPackedBytes (GetEntry (groupStart).info);
    m_groupDiffs  = 0;

    for (i = groupStart + 1; i < m_count; i++)
    {
        m_groupDiffs += GetPackedBytes (GetEntry (i).info);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::ResetGroup
//
//  No keyframe is left on the active path: the next one opens a group.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeArena::ResetGroup()
{
    m_groupLength = 0;
    m_wholeBytes  = 0;
    m_groupWhole  = 0;
    m_groupDiffs  = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::Clear
//
//  Every keyframe goes, with every branch; the table and the arena stay
//  allocated.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeArena::Clear()
{
    if (m_count > 0)
    {
        NotifyDrop (KeyframeDrop::All);
    }

    m_extents.Clear();
    m_branches.clear();

    m_branchBytes        = 0;
    m_pathChangePosition = 0;

    m_first       = 0;
    m_count       = 0;
    m_unplaced    = 0;
    m_storedBytes = 0;
    m_sideBytes   = 0;
    m_groupLength = 0;
    m_wholeBytes  = 0;
    m_groupWhole  = 0;
    m_groupDiffs  = 0;
    m_isFull      = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::Free
//
//  Clears the arena and frees its memory.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeArena::Free()
{
    Clear();

    m_entries = std::vector<Entry>();
    m_arena.reset();
    m_arenaBytes = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::GetPacked
//
////////////////////////////////////////////////////////////////////////////////

KeyframeCodec::Packed KeyframeArena::GetPacked (size_t index) const
{
    const Entry  & entry = GetEntry (index);



    return GetEntryPacked (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::GetEntryPacked
//
////////////////////////////////////////////////////////////////////////////////

KeyframeCodec::Packed KeyframeArena::GetEntryPacked (const Entry & entry) const
{
    return { m_arena.get() + entry.offset, GetPackedBytes (entry.info), entry.info.stateBytes, entry.info.isWhole };
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::GetSide
//
//  The side bytes after a placed keyframe's packed snapshot; empty without
//  side bytes.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeArena::GetSide (
    size_t          index,
    const Byte  *&  outData,
    size_t        & outSize) const
{
    const Entry  & entry = GetEntry (index);



    outSize = entry.info.sideBytes;
    outData = (outSize != 0) ? m_arena.get() + entry.offset + GetPackedBytes (entry.info) : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::TryFindAtOrBefore
//
//  The newest keyframe taken at or before cycle; false when every keyframe
//  is later, or there are none.
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeArena::TryFindAtOrBefore (
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
//  KeyframeArena::TryFindByPosition
//
//  The newest keyframe taken at or before position; false when every
//  keyframe is later, or there are none.
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeArena::TryFindByPosition (
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
//  KeyframeArena::FindGroupStart
//
//  The index of the whole snapshot keyframe index belongs to. The oldest
//  entry is always whole, because groups are dropped whole.
//
////////////////////////////////////////////////////////////////////////////////

size_t KeyframeArena::FindGroupStart (size_t index) const
{
    while (index > 0 && !GetEntry (index).info.isWhole)
    {
        index--;
    }

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::GetUsedBytes
//
//  The bytes stored against the budget, for a meter of how full it is: all
//  of it once the oldest have been dropped for room, since the arena then
//  has all it can, even where a group dropped whole or the room lost to the
//  gaps between extents leaves the bytes stored short of the budget. The table
//  counts from the first keyframe on: its slots come out of the budget, so
//  the arena fills, and the oldest start being dropped, with the snapshots
//  alone short of the budget by the table's size.
//
////////////////////////////////////////////////////////////////////////////////

size_t KeyframeArena::GetUsedBytes (size_t budgetBytes) const
{
    size_t  tableBytes = (m_count > 0) ? m_entries.size() * sizeof (Entry) : 0;



    if (m_isFull)
    {
        return budgetBytes;
    }

    return std::min (GetByteCount() + tableBytes, budgetBytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::PopOldest
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeArena::PopOldest()
{
    NotifyDrop (KeyframeDrop::Oldest);

    m_extents.Free (GetEntry (0).offset, GetEntry (0).info.storedBytes);

    m_storedBytes -= GetEntry (0).info.storedBytes;
    m_sideBytes   -= GetEntry (0).info.sideBytes;

    m_first = (m_first + 1) % m_entries.size();
    m_count--;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::NotifyDrop
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeArena::NotifyDrop (KeyframeDrop drop)
{
    if (m_onDrop)
    {
        m_onDrop (drop);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeArena::AllocateArena
//
//  Committed pages from the system, or null when there are none to commit.
//  The system zeroes each page the first time it is touched; from the heap,
//  a debug build filled the whole arena, 64 MB by default, at every start of
//  recording.
//
////////////////////////////////////////////////////////////////////////////////

KeyframeArena::ArenaPtr KeyframeArena::AllocateArena (size_t bytes)
{
    return ArenaPtr (static_cast<Byte *> (VirtualAlloc (nullptr, (std::max) (bytes, (size_t) 1), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)));
}
