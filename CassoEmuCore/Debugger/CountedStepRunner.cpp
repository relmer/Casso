#include "Pch.h"

#include "Debugger/CountedStepRunner.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CountedStepRunner::Arm
//
//  The run stop hook counts instructions, which is what T n asks for. A
//  step over, a step out or a source step is not a count of instructions,
//  so it runs as single steps, each begun as the last one ends. The last
//  stop, or any stop that is not a step's end, clears them.
//
////////////////////////////////////////////////////////////////////////////////

void CountedStepRunner::Arm (const DebugCommand & command, RunRequest & request)
{
    if (request.count > 1 && (request.kind == RunKind::StepOver || request.kind == RunKind::StepOut || request.lineTable != nullptr))
    {
        m_nextStep        = command;
        m_nextStep->count = 1;
        m_stepsLeft       = request.count - 1;
        request.count     = 1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CountedStepRunner::Begin
//
//  A driver that runs to the stop before returning leaves each further step
//  of a counted step pending here, and they run in turn. Only the outermost
//  start runs them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CountedStepRunner::Begin (IDebugTarget & target, const RunRequest & request)
{
    HRESULT  hr      = S_OK;
    bool     isOuter = !m_isStartingRun;



    m_isStartingRun = true;

    hr = target.StartRun (request);

    while (isOuter && SUCCEEDED (hr) && m_isStepPending)
    {
        m_isStepPending = false;

        if (!m_owner.StartStep (*m_nextStep))
        {
            StopEvent  held = m_heldStop;

            held.cycles     = 0;
            m_stepsLeft     = 0;
            m_isStartingRun = false;
            m_owner.DeliverStop (held);
        }
    }

    if (isOuter)
    {
        m_isStartingRun = false;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CountedStepRunner::TryContinue
//
//  One step of a counted step has ended. The next begins here, or, when the
//  run that ended has not returned from Begin yet, from there.
//
////////////////////////////////////////////////////////////////////////////////

bool CountedStepRunner::TryContinue (StopEvent & ioStop)
{
    if (ioStop.reason == StopReason::Step && m_stepsLeft > 0 && m_nextStep.has_value())
    {
        --m_stepsLeft;
        m_stepCycles += ioStop.cycles;
        m_heldStop    = ioStop;
        m_owner.PauseForNextStep();

        if (m_nextStep->budget.has_value())
        {
            m_nextStep->budget = (*m_nextStep->budget > ioStop.cycles) ? *m_nextStep->budget - ioStop.cycles : 1;
        }

        if (m_isStartingRun)
        {
            m_isStepPending = true;
            return true;
        }

        if (m_owner.StartStep (*m_nextStep))
        {
            return true;
        }

        ioStop.cycles = 0;
        m_stepsLeft   = 0;
    }

    ioStop.cycles += m_stepCycles;
    m_stepCycles   = 0;
    m_stepsLeft    = 0;
    m_nextStep.reset();

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CountedStepRunner::Reset
//
////////////////////////////////////////////////////////////////////////////////

void CountedStepRunner::Reset()
{
    m_nextStep.reset();
    m_stepsLeft     = 0;
    m_stepCycles    = 0;
    m_isStepPending = false;
}
