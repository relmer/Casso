#include "Pch.h"

#include "CpuManager.h"

#include "Core/MachineConfig.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr LONGLONG  s_kHundredNsPerSecond = 10000000LL;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CpuManager
//
////////////////////////////////////////////////////////////////////////////////

CpuManager::CpuManager ()
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

CpuManager::~CpuManager ()
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
    HRESULT  hr = S_OK;


    CBRA (!m_thread.joinable());

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
////////////////////////////////////////////////////////////////////////////////

void CpuManager::Stop ()
{
    m_running.store (false, std::memory_order_release);
    m_pauseCV.notify_all();

    if (m_thread.joinable())
    {
        m_thread.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PostCommand
//
////////////////////////////////////////////////////////////////////////////////

void CpuManager::PostCommand (WORD id, const std::string & payload)
{
    std::lock_guard<std::mutex>  lock (m_cmdMutex);

    m_commandQueue.push_back ({ id, payload });
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsRunning
//
////////////////////////////////////////////////////////////////////////////////

bool CpuManager::IsRunning () const noexcept
{
    return m_running.load (std::memory_order_acquire);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsPaused
//
////////////////////////////////////////////////////////////////////////////////

bool CpuManager::IsPaused () const noexcept
{
    return m_paused.load (std::memory_order_acquire);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPaused
//
////////////////////////////////////////////////////////////////////////////////

void CpuManager::SetPaused (bool paused) noexcept
{
    m_paused.store (paused, std::memory_order_release);
    m_pauseCV.notify_all();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TogglePaused
//
//  Flips the pause flag and wakes the CPU thread. Returns the new
//  paused state so menu wiring can resync its checkmark in a single
//  call.
//
////////////////////////////////////////////////////////////////////////////////

bool CpuManager::TogglePaused () noexcept
{
    bool  next = !m_paused.load (std::memory_order_acquire);


    m_paused.store (next, std::memory_order_release);
    m_pauseCV.notify_all();
    return next;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSpeedMode
//
////////////////////////////////////////////////////////////////////////////////

SpeedMode CpuManager::GetSpeedMode () const noexcept
{
    return m_speedMode.load (std::memory_order_acquire);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSpeedMode
//
////////////////////////////////////////////////////////////////////////////////

void CpuManager::SetSpeedMode (SpeedMode mode) noexcept
{
    m_speedMode.store (mode, std::memory_order_release);
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

void CpuManager::DrainCommandQueue ()
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
            m_onCommand (cmd);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadProc
//
//  CPU-thread entry point. Owns COM init/uninit on this thread, the
//  high-resolution waitable timer for 60 Hz frame pacing, the pause
//  CV wait, and the per-frame callback fan-out.
//
////////////////////////////////////////////////////////////////////////////////

void CpuManager::ThreadProc ()
{
    HRESULT        hr              = S_OK;
    HANDLE         hTimer          = nullptr;
    LARGE_INTEGER  dueTime         = {};
    SpeedMode      speed           = SpeedMode::Authentic;
    bool           fComInitialized = false;
    BOOL           fSuccess        = FALSE;


    hr = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
    CHRA (hr);

    fComInitialized = true;

    if (m_onThreadEnter)
    {
        m_onThreadEnter();
    }

    hTimer = CreateWaitableTimerEx (nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    CWRA (hTimer);

    while (m_running.load (std::memory_order_acquire))
    {
        {
            std::unique_lock<std::mutex>  lock (m_pauseMutex);

            m_pauseCV.wait (lock,
                [&]
                {
                    return !m_paused.load (std::memory_order_acquire) || !m_running.load (std::memory_order_acquire);
                });
        }

        DrainCommandQueue();

        dueTime.QuadPart = -(s_kHundredNsPerSecond * kAppleCyclesPerFrame / kAppleCpuClock);
        fSuccess = SetWaitableTimer (hTimer, &dueTime, 0, nullptr, nullptr, FALSE);
        CWRA (fSuccess);

        if (m_onFrame)
        {
            m_onFrame();
        }

        speed = m_speedMode.load (std::memory_order_acquire);

        if (speed != SpeedMode::Maximum)
        {
            WaitForSingleObject (hTimer, INFINITE);
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
}
