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
////////////////////////////////////////////////////////////////////////////////

struct HistoryStatus
{
    bool                           isRecording        = false;
    bool                           isBehindLive       = false;
    uint64_t                       instructionsBehind = 0;
    uint64_t                       cyclesBehind       = 0;
    int                            unsavedDisks       = 0;
    std::optional<ReverseOutcome>  outcome;
};
