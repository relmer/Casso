#pragma once

#include "Debugger/Reverse/ReverseOutcome.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryStatus
//
//  Where the machine stands in its recorded history, for the debugger's
//  views: whether history is being recorded, and how far behind live the
//  machine is, in instructions and in CPU cycles, and how many disks hold
//  writes not yet saved to their image files. outcome is how the last
//  reverse command ended, kept until the machine next runs; empty when no
//  reverse command has run since.
//
//  While recording, how much of the memory budget history holds, and the CPU
//  cycle its oldest snapshot was taken at, where history begins. Once the
//  budget is full the oldest snapshots are dropped, so that cycle moves
//  forward. beginWallTime is the host's clock when that snapshot was taken,
//  as a UTC FILETIME, or 0 when unknown.
//
////////////////////////////////////////////////////////////////////////////////

struct HistoryStatus
{
    bool                           isRecording        = false;
    bool                           isBehindLive       = false;
    uint64_t                       instructionsBehind = 0;
    uint64_t                       cyclesBehind       = 0;
    int                            unsavedDisks       = 0;
    std::optional<ReverseOutcome>  outcome;

    bool                           hasHistory         = false;
    uint64_t                       beginCycle         = 0;
    uint64_t                       beginWallTime      = 0;
    size_t                         budgetBytes        = 0;
    size_t                         usedBytes          = 0;
};
