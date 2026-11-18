#pragma once

#include "Pch.h"

#include "Debugger/Reverse/InputJournal.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeSettings
//
//  How often a keyframe is taken, how many keyframes share one whole
//  snapshot, and how much memory the store may hold.
//
//  A group holds at least wholeEvery keyframes. Past that, it goes on while
//  its differences together pack smaller than its whole snapshot, up to
//  longestGroup keyframes, so a machine that changes little does not pay for
//  a whole snapshot, most of it unchanged disk tracks, every few seconds.
//
////////////////////////////////////////////////////////////////////////////////

struct KeyframeSettings
{
    static constexpr uint64_t  kFrameCycles         = 17030;
    static constexpr uint64_t  kDefaultFrames       = 10;
    static constexpr uint32_t  kDefaultWholeEvery   = 30;
    static constexpr uint32_t  kDefaultLongestGroup = 1800;
    static constexpr size_t    kDefaultBudgetBytes  = 64ull * 1024 * 1024;

    uint64_t  intervalCycles = kFrameCycles * kDefaultFrames;
    uint32_t  wholeEvery     = kDefaultWholeEvery;
    uint32_t  longestGroup   = kDefaultLongestGroup;
    size_t    budgetBytes    = kDefaultBudgetBytes;
};





////////////////////////////////////////////////////////////////////////////////
//
//  StretchActivity
//
//  What the live run did in the stretch that ends at a keyframe: a bit for
//  each 256-byte page an instruction started on, and the Disk II
//  controller's switch count and motor as the keyframe was taken. A reverse
//  search skips a stretch that no breakpoint could stop in. isKnown is false
//  for a keyframe with no stretch in history before it, which is never
//  skipped.
//
////////////////////////////////////////////////////////////////////////////////

struct StretchActivity
{
    std::array<uint64_t, 4>  pagesRun       = {};
    uint64_t                 diskSwitches   = 0;
    bool                     isDiskSpinning = false;
    bool                     isKnown        = false;
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
    uint64_t         position     = 0;      // instructions retired when it was taken
    uint64_t         cycle        = 0;      // CPU cycle count when it was taken
    size_t           journalIndex = 0;      // input journal end index when it was taken
    uint64_t         checksum     = 0;      // of the whole state: RAM, CPU, devices, disk media
    size_t           stateBytes   = 0;      // unpacked size
    size_t           storedBytes  = 0;      // packed size held in memory, side bytes included
    size_t           sideBytes    = 0;      // of storedBytes, what was kept beside the snapshot
    uint64_t         wallTime     = 0;      // host clock when it was taken, UTC FILETIME; not part of the state
    uint64_t         gapStart     = 0;      // with hasGapBefore: where recording paused before it
    bool             isWhole      = false;  // false: XOR difference from its group's whole snapshot
    bool             isBoundary   = false;  // the state does not follow from the one before: a replay loads it
    bool             isLiveEnd    = false;  // taken where the machine left live for history: a branch switch loads it
    bool             hasGapBefore = false;  // recording paused between gapStart and this keyframe
    StretchActivity  activity;              // in the stretch since the keyframe before
};





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeCopy
//
//  What KeyframeStore::CopyPacked found at a position: the keyframe copied,
//  one still being packed, or none held any more.
//
////////////////////////////////////////////////////////////////////////////////

enum class KeyframeCopy
{
    Copied,
    Pending,
    Gone,
};





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeDrop
//
//  Which keyframes KeyframeStore is about to drop: the oldest, the newest,
//  or every one; or that the active path has changed past
//  KeyframeArena::GetPathChangePosition, after a switch to another branch.
//
////////////////////////////////////////////////////////////////////////////////

enum class KeyframeDrop
{
    Oldest,
    Newest,
    All,
    PathChanged,
};





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeBranch
//
//  A branch of history left behind by a fork: where it forks from, by the
//  position and the history cycle there on the path it forks from (parent 0
//  for the active path, else another branch's id), its live end, and when
//  the user left it.
//
////////////////////////////////////////////////////////////////////////////////

struct KeyframeBranch
{
    uint32_t  id              = 0;
    uint32_t  parent          = 0;
    uint64_t  forkPosition    = 0;
    uint64_t  forkCycle       = 0;
    uint64_t  liveEndPosition = 0;
    uint64_t  liveEndCycle    = 0;
    uint64_t  leftAtWallTime  = 0;
    size_t    keyframeCount   = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SavedKeyframeSegment
//
//  A stretch of history as a saved file has it: the active path, or a
//  branch left behind. Each keyframe is at an offset into the saved arena,
//  and the input journal's entries along the stretch start at
//  firstJournalIndex.
//  The active path's branch fields are zero but its live end.
//
////////////////////////////////////////////////////////////////////////////////

struct SavedKeyframeSegment
{
    KeyframeBranch             info;
    size_t                     firstJournalIndex = 0;
    std::vector<KeyframeInfo>  infos;
    std::vector<size_t>        offsets;
    std::vector<InputRecord>   records;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SavedKeyframes
//
//  A whole history laid out for a file: the settings it was taken with, the
//  active path and then the branches left behind, oldest first, and the
//  stored bytes of every keyframe, end to end in that order.
//
////////////////////////////////////////////////////////////////////////////////

struct SavedKeyframes
{
    KeyframeSettings                   settings;
    std::vector<SavedKeyframeSegment>  segments;
    std::vector<Byte>                  arena;
    uint32_t                           nextBranchId = 1;
};
