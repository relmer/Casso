#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseCommand
//
//  What a reverse execution request asks for. Seek goes to a position given
//  with it, and SeekCycle to the first instruction at or after a cycle given
//  with it; ScrubCycle is the same seek made while a drag of the history
//  timeline goes on, which a later seek will follow. GoLive returns to the
//  end of history; StepForward replays the recorded future one instruction.
//  ReverseContinue goes back to the latest position a stop test fires at.
//  RewindFrame goes back one video frame's cycles, as a held rewind key does
//  each frame. SkipBack and SkipForward move ReverseController::kSkipCycles,
//  ten seconds, back or toward the live end, stopping at either end.
//  SwitchBranch makes the branch of history given with it the active path,
//  and lands at its live end.
//
////////////////////////////////////////////////////////////////////////////////

enum class ReverseCommand
{
    StepBack,
    StepBackOver,
    StepBackOut,
    ReverseContinue,
    StepForward,
    Seek,
    SeekCycle,
    ScrubCycle,
    GoLive,
    RewindFrame,
    SkipBack,
    SkipForward,
    SwitchBranch,
};
