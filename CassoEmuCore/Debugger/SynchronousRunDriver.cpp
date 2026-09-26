#include "Pch.h"

#include "Debugger/SynchronousRunDriver.h"

#include "Debugger/IRunObserver.h"
#include "Debugger/RunStopHook.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SynchronousRunDriver::SynchronousRunDriver
//
////////////////////////////////////////////////////////////////////////////////

SynchronousRunDriver::SynchronousRunDriver (MachineHost & host, RunStopHook & hook) :
    m_host (host),
    m_hook (hook)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  SynchronousRunDriver::Start
//
//  Runs in chunks until the hook stops the run, a pause is requested, or the
//  budget is spent. A run with no budget is unbounded. The hook the host had
//  before the run is restored afterwards, so stop conditions that were
//  installed stay installed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SynchronousRunDriver::Start (const RunRequest & request)
{
    HRESULT      hr          = S_OK;
    DebugHook  * previous    = m_host.GetDebugHook();
    uint64_t     spent       = 0;
    uint64_t     chunk       = 0;
    bool         hasCpu      = m_host.GetCpu() != nullptr;
    bool         budgetSpent = false;
    StopEvent    stop;



    CBRAEx (hasCpu, E_UNEXPECTED);

    m_pauseRequested = false;
    m_hook.Begin (request);
    m_host.SetDebugHook (&m_hook);

    while (!m_hook.HasStopped() && !m_pauseRequested && !budgetSpent)
    {
        chunk = request.budget.has_value() ? std::min (kChunkCycles, *request.budget - spent) : kChunkCycles;

        spent       += m_host.RunCycles (chunk);
        budgetSpent  = request.budget.has_value() && spent >= *request.budget;
    }

    stop.reason    = m_hook.HasStopped() ? m_hook.GetReason() : (budgetSpent ? m_hook.GetBudgetReason() : StopReason::Pause);
    stop.registers = m_host.GetCpu()->GetCpu6502()->GetRegisters();
    stop.pc        = stop.registers.pc;
    stop.cycles    = spent;

    m_hook.End();
    m_host.SetDebugHook (previous);

    if (m_observer != nullptr)
    {
        m_observer->OnStopped (stop);
    }

Error:
    return hr;
}
