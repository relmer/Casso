#pragma once

#include "Debugger/IRunDriver.h"
#include "Ui/UiCommandTypes.h"

class CpuManager;
class DebugHook;
class MachineHost;
class RunStopHook;





////////////////////////////////////////////////////////////////////////////////
//
//  CpuManagerRunDriver
//
//  How a debugger run executes inside the running emulator.
//
//  START MUST NOT BLOCK. The CPU thread drives frames, audio and the command
//  queue as well as the guest, so a driver that ran the whole request inside
//  Start would stall all four -- and the queue it stalled is the one a `pause`
//  arrives on, which is to say the run could never be stopped by the only means
//  of stopping it. Start therefore records the run, installs the hook, un-pauses
//  the CPU manager and returns; the frame loop already running is what executes
//  it.
//
//  The stop is delivered FROM THE CPU THREAD, in OnSliceExecuted, because that
//  is the thread that observed it. Handing the stop to another thread to
//  announce would let the machine run on between the stop and the announcement,
//  so the registers a client reads would not be the ones the run stopped at.
//
//  Budgets are counted ACROSS slices rather than per slice. The frame loop
//  chooses its own slice length from the frame's cycle target, so a budget
//  checked inside one slice would be a budget rounded up to whatever the frame
//  happened to ask for.
//
////////////////////////////////////////////////////////////////////////////////

class CpuManagerRunDriver : public IRunDriver
{
public:
    CpuManagerRunDriver (MachineHost & host, CpuManager & cpuManager, RunStopHook & hook);

    void     SetRunObserver (IRunObserver * observer) override { m_observer = observer; }
    HRESULT  Start          (const RunRequest & request) override;
    void     Pause          () override;

    //  Called from the CPU thread after each slice. Returns true when the run
    //  has ended, which is the frame loop's cue to stop executing this frame.
    //
    //  Also the whole of the driver's per-slice cost when no run is active: a
    //  machine nobody is debugging pays one comparison per slice.
    bool     OnSliceExecuted (uint32_t cyclesExecuted);

    //  Ends the run at once, with reason pause. For a machine the user has
    //  already paused: no slice will run to deliver the stop, so it is
    //  delivered here instead. CPU thread only; does nothing with no run.
    void     EndForUserPause ();

    bool     IsRunning       () const { return m_isRunning; }

private:
    void     Finish          (StopReason reason, uint64_t cycles);

    MachineHost   & m_host;
    CpuManager    & m_cpuManager;
    RunStopHook   & m_hook;
    IRunObserver  * m_observer        = nullptr;

    //  Written by the thread that starts the run and read by the CPU thread.
    //  Both are the CPU thread in every path that matters -- a command arrives
    //  on the queue the CPU thread drains -- which is what keeps this ordinary
    //  state rather than something atomic.
    bool                     m_isRunning      = false;
    bool                     m_pauseRequested = false;
    uint64_t                 m_spent          = 0;
    std::optional<uint64_t>  m_budget;

    //  What the speed was before a full-speed run took it, so the machine the
    //  operator was watching comes back at the speed they set.
    SpeedMode       m_previousSpeed   = SpeedMode::Authentic;
    bool            m_speedChanged    = false;

    DebugHook     * m_previousHook    = nullptr;
};
