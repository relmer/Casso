#include "Pch.h"

#include "Debugger/CpuManagerRunDriver.h"

#include "Debugger/IRunObserver.h"
#include "Debugger/RunStopHook.h"
#include "Shell/CpuManager.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CpuManagerRunDriver::CpuManagerRunDriver
//
////////////////////////////////////////////////////////////////////////////////

CpuManagerRunDriver::CpuManagerRunDriver (MachineHost & host, CpuManager & cpuManager, RunStopHook & hook) :
    m_host       (host),
    m_cpuManager (cpuManager),
    m_hook       (hook)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpuManagerRunDriver::Start
//
//  Records the run and lets the frame loop execute it.
//
//  The machine may ALREADY BE RUNNING, which is the ordinary case rather than
//  an error: an emulator session begins free-running, and a `g` from a client
//  attached to a game already in play adopts it into a debugger run. Un-pausing
//  something that was never paused is what makes those two paths one path.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CpuManagerRunDriver::Start (const RunRequest & request)
{
    HRESULT  hr     = S_OK;
    bool     hasCpu = m_host.GetCpu() != nullptr;



    CBRAEx (hasCpu, E_UNEXPECTED);

    m_pauseRequested = false;
    m_spent          = 0;
    m_budget         = request.budget;
    m_previousHook   = m_host.GetDebugHook();

    //  `GG` asks for the run to go as fast as the host can manage. The speed
    //  the operator chose is put back when the run stops, so a debugger run
    //  does not quietly redefine what "normal" means for the rest of the
    //  session.
    m_speedChanged = request.fullSpeed && m_cpuManager.GetSpeedMode() != SpeedMode::Maximum;

    if (m_speedChanged)
    {
        m_previousSpeed = m_cpuManager.GetSpeedMode();
        m_cpuManager.SetSpeedMode (SpeedMode::Maximum);
    }

    m_hook.Begin (request);
    m_host.SetDebugHook (&m_hook);

    m_isRunning = true;

    m_cpuManager.SetPaused (false);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpuManagerRunDriver::Pause
//
//  Asks the run to end. The stop itself is delivered by the CPU thread at the
//  next slice boundary, so a pause and a breakpoint end a run through exactly
//  one path.
//
//  A pause with NO RUN IN PROGRESS still stops the machine. The emulator is
//  free-running whenever nobody has started a debugger run, and a client that
//  asks it to stop means the machine rather than the bookkeeping.
//
////////////////////////////////////////////////////////////////////////////////

void CpuManagerRunDriver::Pause()
{
    m_pauseRequested = true;

    if (!m_isRunning)
    {
        m_cpuManager.SetPaused (true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpuManagerRunDriver::OnSliceExecuted
//
//  The CPU thread's report that a slice has run, and the one place a run ends.
//
//  ORDER MATTERS: the hook is asked first, because a breakpoint that fired in
//  the same slice as a budget ran out is a breakpoint. The reason a client is
//  told is the reason it would act on, and "budget" for an instruction it had
//  set a breakpoint on would send it looking for a fault that is not there.
//
////////////////////////////////////////////////////////////////////////////////

bool CpuManagerRunDriver::OnSliceExecuted (uint32_t cyclesExecuted)
{
    bool  budgetSpent = false;



    if (!m_isRunning)
    {
        return false;
    }

    m_spent    += cyclesExecuted;
    budgetSpent = m_budget.has_value() && m_spent >= *m_budget;

    if (m_hook.HasStopped())
    {
        Finish (m_hook.GetReason(), m_spent);
        return true;
    }

    if (m_pauseRequested)
    {
        Finish (StopReason::Pause, m_spent);
        return true;
    }

    if (budgetSpent)
    {
        Finish (StopReason::Budget, m_spent);
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpuManagerRunDriver::Finish
//
//  Stops the machine, puts back what the run borrowed, and announces the stop.
//
//  THE MACHINE IS PAUSED BEFORE THE REGISTERS ARE READ, so what a client is
//  told is what the machine holds rather than what it held a few thousand
//  cycles ago.
//
////////////////////////////////////////////////////////////////////////////////

void CpuManagerRunDriver::Finish (StopReason reason, uint64_t cycles)
{
    StopEvent  stop;



    m_cpuManager.SetPaused (true);

    m_hook.End();
    m_host.SetDebugHook (m_previousHook);

    if (m_speedChanged)
    {
        m_cpuManager.SetSpeedMode (m_previousSpeed);
        m_speedChanged = false;
    }

    m_isRunning      = false;
    m_pauseRequested = false;

    stop.reason    = reason;
    stop.registers = m_host.GetCpu()->GetCpu6502()->GetRegisters();
    stop.pc        = stop.registers.pc;
    stop.cycles    = cycles;

    if (m_observer != nullptr)
    {
        m_observer->OnStopped (stop);
    }
}
