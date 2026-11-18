#pragma once

#include "Pch.h"

#include "Debugger/DebugCommand.h"
#include "Debugger/IDebugTarget.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ICountedStepOwner
//
//  The session a CountedStepRunner runs steps for: it pauses between steps,
//  starts each next step, and takes the stop that ends the count.
//
////////////////////////////////////////////////////////////////////////////////

class ICountedStepOwner
{
public:
    virtual ~ICountedStepOwner() = default;

    virtual void  PauseForNextStep () = 0;
    virtual bool  StartStep        (const DebugCommand & step) = 0;
    virtual void  DeliverStop      (const StopEvent & stop) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CountedStepRunner
//
//  A counted step over, step out or source step, which is not a count of
//  instructions the run stop hook can count. It runs as that many single
//  steps, each begun as the last one ends, and the stop that ends the count
//  has every step's cycles. When a step's stop comes while its run is
//  starting, the next step is marked pending, and the start runs it, so a
//  long count does not nest a call per step.
//
////////////////////////////////////////////////////////////////////////////////

class CountedStepRunner
{
public:
    explicit CountedStepRunner (ICountedStepOwner & owner) : m_owner (owner) {}

    //  Takes over a count above one of the kinds above, so the request runs
    //  one step.
    void     Arm         (const DebugCommand & command, RunRequest & request);

    //  Starts the request's run, then each step marked pending during it.
    HRESULT  Begin       (IDebugTarget & target, const RunRequest & request);

    //  True when a step's stop starts the next step or marks it pending;
    //  otherwise the count is over, and the stop has every step's cycles.
    bool     TryContinue (StopEvent & ioStop);

    void     Reset       ();

private:
    ICountedStepOwner            & m_owner;
    std::optional<DebugCommand>    m_nextStep;
    uint32_t                       m_stepsLeft     = 0;
    uint64_t                       m_stepCycles    = 0;
    StopEvent                      m_heldStop;
    bool                           m_isStartingRun = false;
    bool                           m_isStepPending = false;
};
