#include "Pch.h"

#include "Core/ThreadName.h"
#include "CpuManager.h"

#include "Core/MachineConfig.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////
//
//  CpuManager
//
////////////////////////////////////////////////////////////////////////////////

CpuManager::CpuManager()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~CpuManager
//
//  Forces the run flag down and joins the thread if it is still
//  attached. Safe to call after an explicit Stop().
//
////////////////////////////////////////////////////////////////////////////////

CpuManager::~CpuManager()
{
    Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Start
//
//  Stores the per-thread / per-frame / per-command callbacks and
//  spawns the CPU thread. The callbacks are invoked from the worker
//  thread, never from the UI thread; the caller is responsible for
//  any cross-thread synchronization they require.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CpuManager::Start (
    ThreadEnterFn  onThreadEnter,
    CommandFn      onCommand,
    FrameFn        onFrame,
    ThreadExitFn   onThreadExit)
{
    HRESULT  hr     = S_OK;
    bool     isIdle = false;



    isIdle = !m_thread.joinable();
    CBRA (isIdle);

    m_onThreadEnter = std::move (onThreadEnter);
    m_onCommand     = std::move (onCommand);
    m_onFrame       = std::move (onFrame);
    m_onThreadExit  = std::move (onThreadExit);

    m_running.store (true, std::memory_order_release);

    m_thread = std::thread (&CpuManager::ThreadProc, this);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Stop
//
//  Clears the run flag, wakes the pause CV (in case the thread is
//  parked on a pause), and joins. Idempotent.
//
//  The wake takes the pause mutex for the same reason PostCommand does:
//  a notify that lands between the waiter's predicate check and its block
//  is lost, and a lost wakeup HERE hangs the join -- app shutdown deadlocks
//  against a paused machine.
//
////////////////////////////////////////////////////////////////////////////////

void CpuManager::Stop()
{
    m_running.store (false, std::memory_order_release);

    {
        std::lock_guard<std::mutex>  lock (m_pauseMutex);

        m_pauseCV.notify_all();
    }

    if (m_thread.joinable())
    {
        m_thread.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PostCommand
//
//  Wakes a PAUSED CPU thread, because the queue is serviced while paused:
//  mount / eject / write-protect are user intents that must land when the
//  user asks for them, not when the machine next runs. The wake takes the
//  pause mutex so it cannot slip into the window between the waiter
//  evaluating its predicate and actually blocking -- a lost wakeup there
//  would strand the command until something else resumed the thread.
//  The gate, when one is set, may drop the command first.
//
////////////////////////////////////////////////////////////////////////////////

void CpuManager::PostCommand (WORD id, const std::string & payload)
{
    TimePoint  postedAt = m_now();



    if (m_gate && !m_gate (id, payload))
    {
        return;
    }

    {
        std::lock_guard<std::mutex>  lock (m_cmdMutex);



        m_commandQueue.push_back ({ id, payload, postedAt });
    }

    {
        std::lock_guard<std::mutex>  lock (m_pauseMutex);



        m_pauseCV.notify_all();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsRunning
//
////////////////////////////////////////////////////////////////////////////////

bool CpuManager::IsRunning() const noexcept
{
    return m_running.load (std::memory_order_acquire);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsPaused
//
////////////////////////////////////////////////////////////////////////////////

bool CpuManager::IsPaused() const noexcept
{
    return m_paused.load (std::memory_order_acquire);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPaused
//
//  Stores and wakes under the pause mutex. TryPark tests the flag under the
//  same mutex before the CPU thread runs a frame, so a pause cannot land
//  unseen between that test and the frame after it. A lost wakeup on the
//  resume edge (see Stop) would leave the machine parked with the UI
//  reporting it running.
//
////////////////////////////////////////////////////////////////////////////////

void CpuManager::SetPaused (bool paused) noexcept
{
    std::lock_guard<std::mutex>  lock (m_pauseMutex);



    m_paused.store (paused, std::memory_order_release);
    m_pauseCV.notify_all();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TogglePaused
//
//  Flips the pause flag and wakes the CPU thread, in one step under the pause
//  mutex that every change to the flag takes. A read and then a store with
//  nothing held between them lost a toggle whenever another change landed
//  in between.
//
//  Returns nothing: the one caller re-derives the state through IsPaused
//  (via UpdateWindowTitle), and a bool return whose meaning is "the new
//  state" rather than success is exactly the ambiguity the naming rule
//  forbids. Anyone who needs the state after a toggle asks IsPaused.
//
////////////////////////////////////////////////////////////////////////////////

void CpuManager::TogglePaused() noexcept
{
    std::lock_guard<std::mutex>  lock (m_pauseMutex);



    m_paused.store (!m_paused.load (std::memory_order_acquire), std::memory_order_release);
    m_pauseCV.notify_all();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsParked
//
////////////////////////////////////////////////////////////////////////////////

bool CpuManager::IsParked() const noexcept
{
    return m_isParked.load (std::memory_order_acquire);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryWaitUntilParked
//
//  The acknowledgment of a pause. The CPU thread parks once it is outside a
//  frame: as it enters the pause wait when the pause lands between frames,
//  and otherwise once the machine has stopped on the instruction boundary
//  after the pause and the frame has ended. The wait is bounded because the
//  thread may first be busy with a long command, and a caller on the UI
//  thread must not hang on it.
//
////////////////////////////////////////////////////////////////////////////////

bool CpuManager::TryWaitUntilParked (std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex>  lock (m_pauseMutex);



    return m_parkedCV.wait_for (lock, timeout, [this] { return m_isParked.load (std::memory_order_acquire); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPark
//
//  CPU thread, outside a frame: parks it when a pause is asked for or the
//  thread is stopping, and marks it running otherwise; true when it parked.
//  The test runs under the pause mutex, which every change to the flag takes,
//  so a pause is either seen here or raised while the next frame runs, where
//  the machine stops on its next instruction boundary and the thread parks
//  as soon as the frame returns.
//
////////////////////////////////////////////////////////////////////////////////

bool CpuManager::TryPark()
{
    std::lock_guard<std::mutex>  lock (m_pauseMutex);
    bool                         isParked = m_paused.load (std::memory_order_acquire) || !m_running.load (std::memory_order_acquire);



    m_isParked.store (isParked, std::memory_order_release);

    if (isParked)
    {
        m_parkedCV.notify_all();
    }

    return isParked;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParkForExit
//
//  CPU thread, last thing before it ends: no thread runs the machine now,
//  however the loop was left, so a waiter is let go.
//
////////////////////////////////////////////////////////////////////////////////

void CpuManager::ParkForExit()
{
    std::lock_guard<std::mutex>  lock (m_pauseMutex);



    m_isParked.store (true, std::memory_order_release);
    m_parkedCV.notify_all();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSpeedMode
//
////////////////////////////////////////////////////////////////////////////////

SpeedMode CpuManager::GetSpeedMode() const noexcept
{
    return m_speedMode.load (std::memory_order_acquire);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetUserSpeedMode
//
//  The speed the user last chose, which an automatic change leaves alone.
//
////////////////////////////////////////////////////////////////////////////////

SpeedMode CpuManager::GetUserSpeedMode() const noexcept
{
    return m_userSpeed.load (std::memory_order_acquire);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSpeedMode
//
//  Paces the machine at mode. A change the user chose also becomes the
//  user's speed; an automatic one leaves that as it was.
//
////////////////////////////////////////////////////////////////////////////////

void CpuManager::SetSpeedMode (
    SpeedMode     mode,
    SpeedChooser  chooser) noexcept
{
    if (chooser == SpeedChooser::User)
    {
        m_userSpeed.store (mode, std::memory_order_release);
    }

    m_speedMode.store (mode, std::memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetEffectiveSpeedMode
//
////////////////////////////////////////////////////////////////////////////////

SpeedMode CpuManager::GetEffectiveSpeedMode() const noexcept
{
    return IsMaximumOverride() ? SpeedMode::Maximum : GetSpeedMode();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrainCommandQueue
//
//  Swaps the pending command vector onto the stack under the mutex,
//  then dispatches each command outside the lock so handlers may take
//  longer-held locks without deadlocking UI-thread PostCommand callers.
//
////////////////////////////////////////////////////////////////////////////////

void CpuManager::DrainCommandQueue()
{
    std::vector<EmulatorCommand>  cmds;



    {
        std::lock_guard<std::mutex>  lock (m_cmdMutex);

        cmds.swap (m_commandQueue);
    }

    if (m_onCommand)
    {
        for (const auto & cmd : cmds)
        {
            m_dispatchedAt = cmd.postedAt;
            m_onCommand (cmd);
        }

        m_dispatchedAt.reset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MarkTickStart
//
//  The start of the host frame tick a pass belongs to. The pass itself takes
//  a sliver of the tick and the thread sleeps out the rest, so a request made
//  during the tick reaches the CPU thread between this pass and the next.
//
////////////////////////////////////////////////////////////////////////////////

void CpuManager::MarkTickStart()
{
    m_tickStart = m_now();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRequestTickFraction
//
//  How far through the current host tick the request being handled arrived.
//  A command holds the time the UI posted it, since the CPU thread reads the
//  queue only between passes and its own clock would put every request at the
//  top of a tick. Work that is not a posted command, such as the debug
//  channel's service, arrived now.
//
////////////////////////////////////////////////////////////////////////////////

double CpuManager::GetRequestTickFraction() const
{
    TimePoint  requestedAt = m_dispatchedAt.has_value() ? *m_dispatchedAt : m_now();



    return ComputeTickFraction (m_tickStart, requestedAt);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeTickFraction
//
//  The share of one host frame tick between its start and a request, held to
//  [0, 1). A request stamped before the tick began, during the pass, is at its
//  top; one later than a whole tick, which only an unpaced Maximum speed
//  produces, is at its end.
//
////////////////////////////////////////////////////////////////////////////////

double CpuManager::ComputeTickFraction (
    TimePoint  tickStart,
    TimePoint  requestedAt)
{
    constexpr double  kNsPerSecond   = 1e9;
    constexpr double  kTickNs        = kNsPerSecond * kAppleCyclesPerFrame / kAppleCpuClock;
    constexpr double  kLastBeforeOne = 1.0 - 1e-9;



    double  elapsedNs = (double) std::chrono::duration_cast<std::chrono::nanoseconds> (requestedAt - tickStart).count();
    double  fraction  = elapsedNs / kTickNs;



    if (fraction < 0.0)
    {
        fraction = 0.0;
    }

    if (fraction > kLastBeforeOne)
    {
        fraction = kLastBeforeOne;
    }

    return fraction;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasPendingCommands
//
//  The pause wait's other wake condition, so a command posted to a paused
//  machine is dispatched rather than parked until resume.
//
////////////////////////////////////////////////////////////////////////////////

bool CpuManager::HasPendingCommands()
{
    std::lock_guard<std::mutex>  lock (m_cmdMutex);



    return !m_commandQueue.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WaitWhilePaused
//
//  CPU thread, top of each pass: blocks while the machine is paused and no
//  command is waiting. A paused thread parks first, under the mutex the wait
//  takes, so a pause that landed outside a frame, in the pacing wait that
//  fills most of each frame period at 1x and Double, is acknowledged before
//  the thread blocks. The TryPark after the service pass is too late for it:
//  the wait would first run its whole service interval, since no command
//  arrives to end it, and with no service function nothing would end it
//  before the resume.
//
//  A service to run means a paused machine cannot block until woken: a debug
//  client querying it has no way to post a command, so the wait ends every
//  kServiceIntervalMs and the service runs.
//
////////////////////////////////////////////////////////////////////////////////

void CpuManager::WaitWhilePaused()
{
    std::unique_lock<std::mutex>  lock (m_pauseMutex);
    auto                          isWoken = [&]
    {
        return !m_paused.load  (std::memory_order_acquire) ||
               !m_running.load (std::memory_order_acquire) ||
               HasPendingCommands();
    };



    if (m_paused.load (std::memory_order_acquire))
    {
        m_isParked.store (true, std::memory_order_release);
        m_parkedCV.notify_all();
    }

    if (m_onService)
    {
        m_pauseCV.wait_for (lock, std::chrono::milliseconds (kServiceIntervalMs), isWoken);
    }
    else
    {
        m_pauseCV.wait (lock, isWoken);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadProc
//
//  CPU-thread entry point. Owns COM init/uninit on this thread, the
//  high-resolution waitable timer for 60 Hz frame pacing, the pause
//  wait (WaitWhilePaused), the park that acknowledges a pause, and the
//  per-frame callback fan-out.
//
////////////////////////////////////////////////////////////////////////////////

void CpuManager::ThreadProc()
{
    //  kFramePeriod100Ns is one frame in 100 ns units. The pacing below
    //  advances an ABSOLUTE deadline by exactly that much per pass, so a late
    //  wake-up is repaid by the next frame running short. Re-arming a RELATIVE
    //  timer after each wait, as this did, restarts the interval from whenever
    //  the thread happens to get scheduled, so lateness is never repaid.
    //
    //  What that costs is JITTER, not drift. Measured over 2341 frames, the
    //  wake is late by a mean of 329 us with a standard deviation of 244 and a
    //  tail to 5.4 ms -- a distribution as wide as its own mean, not a constant
    //  offset. The long-run rate barely moves either way (99.62% of real time
    //  before, 99.82% after), which is why a rate measurement alone does not
    //  find this.
    //
    //  It reaches the speaker through the audio queue. A frame hands over its
    //  whole slice of samples at once while the endpoint drains continuously,
    //  so what matters is the SPACING of those handovers, not their average.
    //  Under the relative timer a run of late frames emptied the queue and
    //  WasapiAudio::DrainFrames spliced in its decaying filler; at the endpoint
    //  that was 1.76% amplitude modulation at 41 Hz, audible as a rhythmic
    //  beating under every sound the machine made. With the deadline absolute
    //  a late frame is followed by an early one, the queue level stays put, and
    //  the same measurement reads 0.03%.
    //
    //  kRebaseSlack100Ns is how far behind the deadline may fall before it is
    //  re-based rather than chased, so a pause, a breakpoint or a long stall
    //  is not repaid as a burst of frames at full tilt.
    //
    //  THE DEADLINE IS KEPT ON THE PERFORMANCE COUNTER AND THE WAIT IS ASKED
    //  FOR AS AN INTERVAL. Both halves matter. QueryPerformanceCounter is
    //  monotonic, where GetSystemTimeAsFileTime follows the wall clock, and a
    //  waitable timer armed with an absolute time follows the wall clock too.
    //  Holding the deadline in wall-clock terms meant a backward step -- an
    //  NTP correction, a resumed VM, a changed time zone -- left it in the
    //  future by the size of the step while the rebase guard, which only looks
    //  for a deadline that has fallen BEHIND, saw nothing wrong. The thread
    //  then waited out the whole jump with the machine frozen.
    constexpr LONGLONG  kHundredNsPerSecond = 10000000LL;
    constexpr LONGLONG  kFramePeriod100Ns   = kHundredNsPerSecond *
                                              kAppleCyclesPerFrame / kAppleCpuClock;
    constexpr LONGLONG  kRebaseSlack100Ns   = kFramePeriod100Ns * 4;



    HRESULT        hr              = S_OK;
    HANDLE         hTimer          = nullptr;
    LARGE_INTEGER  dueTime         = {};
    LARGE_INTEGER  qpcFreq         = {};
    LARGE_INTEGER  qpcNow          = {};
    SpeedMode      speed           = SpeedMode::Authentic;
    bool           fComInitialized = false;
    BOOL           fSuccess        = FALSE;
    LONGLONG       nowNs           = 0;
    LONGLONG       deadline        = 0;
    LONGLONG       wait100Ns       = 0;


    ThreadName::Set (L"Casso machine");

    hr = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
    CHRA (hr);

    fComInitialized = true;

    if (m_onThreadEnter)
    {
        m_onThreadEnter();
    }

    hTimer = CreateWaitableTimerEx (nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    CWRA (hTimer);

    //  Fixed at boot and never zero on any machine this runs on, but it is a
    //  divisor, so it is checked rather than assumed.
    fSuccess = QueryPerformanceFrequency (&qpcFreq);
    CWRA (fSuccess);
    CBRA (qpcFreq.QuadPart > 0);

    while (m_running.load (std::memory_order_acquire))
    {
        WaitWhilePaused();

        // Runs before the pause check below, so a paused machine still
        // services mount / eject / settings commands: the shell gives the
        // user immediate feedback (the drive door swings open the moment
        // eject is clicked) and the store has to catch up, paused or not.
        DrainCommandQueue();

        if (m_onService)
        {
            m_onService();
        }

        if (TryPark())
        {
            continue;
        }

        QueryPerformanceCounter (&qpcNow);

        //  Whole seconds first, then the remainder, rather than scaling the
        //  raw count. The counter runs from boot, and Windows reports 10 MHz
        //  on current hardware, so a single multiply by kHundredNsPerSecond
        //  passes LLONG_MAX after 25.6 hours of uptime and wraps. nowNs then
        //  steps backward, the clamp below bounds the wait, and that frame
        //  hands its samples over out of step -- the jitter the absolute
        //  deadline exists to remove. Split this way the seconds term stays
        //  small and the remainder is below the frequency, so neither term
        //  can overflow.
        nowNs = (qpcNow.QuadPart / qpcFreq.QuadPart) * kHundredNsPerSecond +
                (qpcNow.QuadPart % qpcFreq.QuadPart) * kHundredNsPerSecond / qpcFreq.QuadPart;

        deadline += kFramePeriod100Ns;

        //  Behind by more than the slack: a pause, a breakpoint or a long
        //  stall, which is not repaid.
        if (deadline < nowNs - kRebaseSlack100Ns)
        {
            deadline = nowNs + kFramePeriod100Ns;
        }

        //  Ahead by more than one frame cannot happen from pacing alone and
        //  means the counter and the deadline have lost each other. Clamping
        //  bounds the wait at a frame however that came about, so no single
        //  wait can freeze the machine.
        wait100Ns = deadline - nowNs;

        if (wait100Ns > kFramePeriod100Ns)
        {
            wait100Ns = kFramePeriod100Ns;
            deadline  = nowNs + kFramePeriod100Ns;
        }

        //  Negative is a relative interval, positive an absolute wall-clock
        //  FILETIME. Relative, so that a wall-clock step cannot reach the wait.
        dueTime.QuadPart = (wait100Ns > 0) ? -wait100Ns : -1;
        fSuccess = SetWaitableTimer (hTimer, &dueTime, 0, nullptr, nullptr, FALSE);
        CWRA (fSuccess);

        MarkTickStart();

        if (m_onFrame)
        {
            m_onFrame();
        }

        //  A pause raised while the frame ran stopped the machine on the next
        //  instruction boundary and ended the frame there. The thread parks now
        //  rather than after the pacing wait, so the pause is acknowledged as
        //  soon as the frame returns, and the deadline is re-based on resume.
        if (TryPark())
        {
            deadline = 0;
            continue;
        }

        speed = GetEffectiveSpeedMode();

        if (speed != SpeedMode::Maximum)
        {
            WaitForSingleObject (hTimer, INFINITE);
        }
        else
        {
            //  Maximum speed does not pace at all, so the deadline is
            //  meaningless while it is engaged; re-base when pacing resumes.
            deadline = 0;
        }
    }

Error:
    if (hTimer != nullptr)
    {
        CloseHandle (hTimer);
    }

    if (m_onThreadExit)
    {
        m_onThreadExit();
    }

    if (fComInitialized)
    {
        CoUninitialize();
    }

    ParkForExit();
}
