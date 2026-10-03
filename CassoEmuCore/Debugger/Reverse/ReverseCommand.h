#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseCommand
//
//  What a reverse execution request asks for. Seek goes to a position given
//  with it; GoLive returns to the end of history; StepForward replays the
//  recorded future one instruction. ReverseContinue goes back to the latest
//  position a stop test fires at.
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
    GoLive,
};
