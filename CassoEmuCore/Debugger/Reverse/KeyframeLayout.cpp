#include "Pch.h"

#include "Debugger/Reverse/KeyframeLayout.h"
#include "Debugger/Reverse/KeyframeStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeLayout::GetStateRoom
//
////////////////////////////////////////////////////////////////////////////////

size_t KeyframeLayout::GetStateRoom (size_t stateBytes)
{
    return stateBytes + stateBytes / kStateGrowth;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeLayout::GetPackedBound
//
////////////////////////////////////////////////////////////////////////////////

size_t KeyframeLayout::GetPackedBound (size_t stateBytes)
{
    size_t  stateRoom = GetStateRoom (stateBytes);



    return stateRoom + stateRoom / kPackOverhead + kPackSlack;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeLayout::GetGroupSlots
//
////////////////////////////////////////////////////////////////////////////////

size_t KeyframeLayout::GetGroupSlots (const KeyframeSettings & settings)
{
    return static_cast<size_t> (settings.wholeEvery) + kInFlight;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeLayout::GetLeastArena
//
//  The newest group's first wholeEvery keyframes, every keyframe in flight,
//  and two more for the room lost to the gaps between extents. Past wholeEvery its
//  differences together pack smaller than its whole snapshot until the last
//  one, which adds two more.
//
////////////////////////////////////////////////////////////////////////////////

size_t KeyframeLayout::GetLeastArena (const KeyframeSettings & settings, size_t stateBytes)
{
    constexpr size_t  kPastLeast = 2;
    size_t            slots      = GetGroupSlots (settings) + kPastLeast;



    return slots * GetPackedBound (stateBytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeLayout::ComputeLayout
//
//  The table has one slot per kBudgetPerEntry bytes of budget, and room for
//  the newest group, however long, within half of it. The arena is the rest
//  of the budget after the table and the unpacked newest whole snapshot, and
//  never smaller than the least arena, so a store whose budget is too small
//  for that has more than its budget.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeLayout::ComputeLayout (
    const KeyframeSettings  & settings,
    size_t                    stateBytes,
    size_t                    groupLength,
    size_t                    entryBytes,
    size_t                  & outSlotCount,
    size_t                  & outArenaBytes)
{
    size_t  groupSlots   = std::max (GetGroupSlots (settings), groupLength + kInFlight);
    size_t  minimumArena = GetLeastArena (settings, stateBytes);
    size_t  overhead     = 0;



    outSlotCount  = std::max (settings.budgetBytes / kBudgetPerEntry, 2 * groupSlots);
    overhead      = outSlotCount * entryBytes + stateBytes;
    outArenaBytes = (settings.budgetBytes > overhead) ? settings.budgetBytes - overhead : 0;
    outArenaBytes = std::max (outArenaBytes, minimumArena);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeLayout::ComputeFitLayout
//
//  A slot for every keyframe of the average packed size the budget fits,
//  and the rest for the arena. Never fewer slots than slotCount, and never
//  an arena smaller than the least arena.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeLayout::ComputeFitLayout (
    const KeyframeSettings  & settings,
    size_t                    collected,
    size_t                    storedBytes,
    size_t                    wholeBytes,
    size_t                    slotCount,
    size_t                    entryBytes,
    size_t                  & outSlotCount,
    size_t                  & outArenaBytes)
{
    size_t  average    = (collected > 0) ? storedBytes / collected : kBudgetPerEntry;
    size_t  usable     = (settings.budgetBytes > wholeBytes) ? settings.budgetBytes - wholeBytes : 0;
    size_t  leastArena = GetLeastArena (settings, wholeBytes);



    outSlotCount  = std::max (usable / (average + entryBytes), slotCount);
    outArenaBytes = (usable > outSlotCount * entryBytes) ? usable - outSlotCount * entryBytes : 0;
    outArenaBytes = std::max (outArenaBytes, leastArena);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeLayout::IsGroupDone
//
//  The newest group has longestGroup keyframes, or it has wholeEvery and the
//  differences collected in it pack to at least its whole snapshot, so
//  another whole snapshot costs no more than the group already does. A group
//  also ends before it fills half the table, so a full table always has an
//  older group to drop.
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeLayout::IsGroupDone (
    const KeyframeSettings  & settings,
    size_t                    groupLength,
    size_t                    slotCount,
    size_t                    groupDiffs,
    size_t                    groupWhole)
{
    size_t  halfTable = slotCount / 2;
    bool    isLongest = groupLength >= settings.longestGroup || groupLength + kInFlight >= halfTable;
    bool    isLeast   = groupLength >= settings.wholeEvery;
    bool    isPaid    = groupDiffs >= groupWhole;



    return isLongest || (isLeast && isPaid);
}
