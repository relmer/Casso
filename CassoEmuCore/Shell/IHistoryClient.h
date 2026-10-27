#pragma once

#include "Pch.h"

class IReverseStopTest;
class ReverseController;
struct CallerLink;
struct DebugCommand;
struct ReverseResult;
enum class ReverseCommand;





////////////////////////////////////////////////////////////////////////////////
//
//  IHistoryClient
//
//  What the machine's history requires of the debugger, which follows it: the
//  views rebuilt from history when it starts again or stops, the call record
//  step back out consults, reverse continue's stop test, the moves through
//  history and their landings, and the edits deferred behind live. Every
//  call is on the CPU thread. The shell's debugger implements it; history
//  with no client attached runs without a debugger.
//
////////////////////////////////////////////////////////////////////////////////

class IHistoryClient
{
public:
    virtual ~IHistoryClient() = default;

    //  History started again at the machine's position, or stopped: the
    //  controller is null while history is off.
    virtual void                OnHistoryRestarted (ReverseController * controller) = 0;
    virtual void                OnHistoryStopping  ()                               = 0;

    //  The call record: whether no call made since the cycle where history
    //  begins entered the code now running, and the calls not yet returned from.
    virtual bool                HasNoCallSince     (uint64_t historyStartCycle)          = 0;
    virtual void                GetCallerLinks     (std::vector<CallerLink> & outLinks) = 0;

    //  Reverse continue's stop test, or null with no debugger session.
    virtual IReverseStopTest  * GetReverseStopTest () = 0;

    //  A reverse command is about to move the machine; it moved, or did not;
    //  and where it landed, reported as a step's stop.
    virtual void                OnHistoryMoving    ()                               = 0;
    virtual void                OnHistoryMoved     (ReverseCommand command)         = 0;
    virtual void                OnHistoryLanded    (const ReverseResult & result)   = 0;

    //  The debugger edits deferred behind live, run live in the order made.
    virtual void                RunDeferredEdits   (const std::vector<DebugCommand> & edits) = 0;

    //  Whether the machine runs a debugger step rather than a run, and the
    //  session notified that the machine was paused or resumed.
    virtual bool                IsDebugStepRunning ()            = 0;
    virtual void                OnPauseChanged     (bool paused) = 0;
};
