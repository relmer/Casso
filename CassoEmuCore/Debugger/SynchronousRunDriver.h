#pragma once

#include "Debugger/IRunDriver.h"

class MachineHost;
class RunStopHook;





////////////////////////////////////////////////////////////////////////////////
//
//  SynchronousRunDriver
//
//  Runs a debugger run to completion inside Start, on the calling thread, and
//  delivers the stop before returning. Batch mode uses it; nothing paces the
//  machine and nothing reads a clock.
//
////////////////////////////////////////////////////////////////////////////////

class SynchronousRunDriver : public IRunDriver
{
public:
    SynchronousRunDriver (MachineHost & host, RunStopHook & hook);

    void     SetRunObserver (IRunObserver * observer) override { m_observer = observer; }
    HRESULT  Start          (const RunRequest & request) override;
    void     Pause          () override { m_pauseRequested = true; }

private:
    static constexpr uint64_t  kChunkCycles = 10000;

    MachineHost   & m_host;
    RunStopHook   & m_hook;
    IRunObserver  * m_observer       = nullptr;
    bool            m_pauseRequested = false;
};
