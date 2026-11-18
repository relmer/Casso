#pragma once

#include "Pch.h"

struct KeyframeSettings;





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeLayout
//
//  How a keyframe store's budget is split between its table and its arena,
//  and when a group of keyframes ends. Pure functions of the settings and
//  the sizes the store has now.
//
////////////////////////////////////////////////////////////////////////////////

class KeyframeLayout
{
public:
    //  The keyframes packing at once: one in the work queue, one more while
    //  the next is captured.
    static constexpr size_t  kJobCount = 2;

    //  One slot of the table per kBudgetPerEntry bytes of budget.
    static constexpr size_t  kBudgetPerEntry = 1024;

    //  The slots for the newest group beyond wholeEvery: every keyframe in
    //  flight, and two more.
    static constexpr size_t  kInFlight = kJobCount + 2;

    //  A state's buffers, with room for a state that grows a little, and the
    //  packer's worst case for it.
    static size_t  GetStateRoom   (size_t stateBytes);
    static size_t  GetPackedBound (size_t stateBytes);

    //  The slots for the newest group at once: a group of wholeEvery and
    //  the keyframes in flight.
    static size_t  GetGroupSlots (const KeyframeSettings & settings);

    //  The smallest arena that always fits the newest group at the packer's
    //  worst case.
    static size_t  GetLeastArena (const KeyframeSettings & settings, size_t stateBytes);

    //  The table's slots and the arena's bytes from the budget for a state
    //  of stateBytes and a newest group of groupLength.
    static void  ComputeLayout (const KeyframeSettings & settings,
                                size_t                   stateBytes,
                                size_t                   groupLength,
                                size_t                   entryBytes,
                                size_t                 & outSlotCount,
                                size_t                 & outArenaBytes);

    //  A table and arena that split the budget by the average packed size of
    //  the keyframes stored so far.
    static void  ComputeFitLayout (const KeyframeSettings & settings,
                                   size_t                   collected,
                                   size_t                   storedBytes,
                                   size_t                   wholeBytes,
                                   size_t                   slotCount,
                                   size_t                   entryBytes,
                                   size_t                 & outSlotCount,
                                   size_t                 & outArenaBytes);

    //  Whether the next keyframe starts a new group.
    static bool  IsGroupDone (const KeyframeSettings & settings,
                              size_t                   groupLength,
                              size_t                   slotCount,
                              size_t                   groupDiffs,
                              size_t                   groupWhole);

private:
    static constexpr size_t  kPackSlack    = 4096;
    static constexpr size_t  kPackOverhead = 8;
    static constexpr size_t  kStateGrowth  = 16;
};
