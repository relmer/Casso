#pragma once

#include "Pch.h"

#include "Ui/UiCommandTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorCommand
//
//  Single unit of work queued from the UI thread for the CPU thread to
//  execute. Carries the menu/command id plus an opaque payload string
//  (typically a file path) the dispatcher unpacks per id.
//
////////////////////////////////////////////////////////////////////////////////

struct EmulatorCommand
{
    WORD                                   id       = 0;
    std::string                            payload;
    std::chrono::steady_clock::time_point  postedAt = {};    // when the UI asked, for a pause's landing point
};





////////////////////////////////////////////////////////////////////////////////
//
//  CpuManager
//
//  Owner of the dedicated CPU-execution thread, the run/pause/step
//  transition state, and the UI -> CPU command queue. Also hosts the
//  paste-buffer storage that ClipboardManager reads/writes (it shares
//  the command-queue mutex because both surfaces are populated by the
//  UI thread and drained by the CPU thread).
//
//  The thread body delegates the per-frame work and per-command
//  dispatch back to EmulatorShell through std::function callbacks so
//  no shell internals leak across the manager boundary.
//
////////////////////////////////////////////////////////////////////////////////

class CpuManager
{
public:
    using ThreadEnterFn = std::function<void()>;
    using ThreadExitFn  = std::function<void()>;
    using CommandFn     = std::function<void(const EmulatorCommand &)>;
    using FrameFn       = std::function<void()>;
    using ServiceFn     = std::function<void()>;
    using TimePoint     = std::chrono::steady_clock::time_point;
    using Now           = std::function<TimePoint ()>;
    using GateFn        = std::function<bool (WORD, const std::string &)>;


    CpuManager  ();
    ~CpuManager ();


    HRESULT Start (ThreadEnterFn  onThreadEnter,
                   CommandFn      onCommand,
                   FrameFn        onFrame,
                   ThreadExitFn   onThreadExit);
    void    Stop  ();

    void    PostCommand    (WORD id, const std::string & payload = {});

    //  Asked about every command before it is queued, on the posting thread;
    //  false drops it. Set before Start.
    void    SetCommandGate (GateFn gate) { m_gate = std::move (gate); }

    //  Whether a posted command is still waiting for the CPU thread.
    bool    HasPendingCommands ();

    //  Work the CPU thread does whether or not the machine is paused, on every
    //  pass through its loop: the debug channel, which has to answer a client
    //  that asks about a paused machine. Set before Start. While one is set, a
    //  paused machine wakes every kServiceIntervalMs to run it.
    static constexpr DWORD  kServiceIntervalMs = 20;

    void    SetServiceFunction (ServiceFn service) { m_onService = std::move (service); }

    bool    IsRunning      () const noexcept;
    bool    IsPaused       () const noexcept;
    void    SetPaused      (bool paused) noexcept;
    void    TogglePaused   () noexcept;

    //  Whether the CPU thread has acted on a pause: the machine stopped on an
    //  instruction boundary, the thread left its frame, and it runs no frame
    //  until it is resumed. Commands posted to a paused machine still run on
    //  it. IsPaused gives what was asked for; this gives where the CPU thread
    //  is. True while no CPU thread runs.
    bool    IsParked           () const noexcept;

    //  Waits up to timeout for the CPU thread to park after a pause; true once
    //  it has, false when the wait ran out first.
    bool    TryWaitUntilParked (std::chrono::milliseconds timeout);

    //  The flag a pause raises, for the machine to test between instructions
    //  (MachineHost::SetStopFlag).
    const std::atomic<bool>  & GetPauseFlag() const noexcept { return m_paused; }

    SpeedMode  GetSpeedMode       () const noexcept;
    SpeedMode  GetUserSpeedMode   () const noexcept;
    bool       IsUserMaximumSpeed () const noexcept { return GetUserSpeedMode() == SpeedMode::Maximum; }
    void       SetSpeedMode       (SpeedMode mode, SpeedChooser chooser) noexcept;

    // Shared storage exposed for ClipboardManager wiring. The paste
    // buffer is guarded by the same mutex as the command queue so
    // both UI-thread enqueue paths can take a single lock.
    std::mutex   & GetCommandMutex () noexcept { return m_cmdMutex; }
    std::string  & GetPasteBuffer  () noexcept { return m_pasteBuffer; }

    //  The host frame tick, for a pause that lands as far into the frame as
    //  the request came into the tick. The clock is replaceable so a test can
    //  fix the fraction; set it before Start.
    void    SetClock               (Now now) { m_now = std::move (now); }
    void    MarkTickStart          ();
    double  GetRequestTickFraction () const;

    static double  ComputeTickFraction (TimePoint tickStart, TimePoint requestedAt);

private:
    void ThreadProc ();
    void WaitWhilePaused();
    void DrainCommandQueue ();
    bool TryPark ();
    void ParkForExit ();


    std::thread                   m_thread;

    std::atomic<bool>             m_running    { true };
    std::atomic<bool>             m_paused     { false };
    std::atomic<bool>             m_isParked   { true };                     // written under m_pauseMutex
    std::atomic<SpeedMode>        m_speedMode  { SpeedMode::Authentic };
    std::atomic<SpeedMode>        m_userSpeed  { SpeedMode::Authentic };     // the last speed the user chose

    std::mutex                    m_pauseMutex;
    std::condition_variable       m_pauseCV;
    std::condition_variable       m_parkedCV;

    std::mutex                    m_cmdMutex;
    std::vector<EmulatorCommand>  m_commandQueue;
    std::string                   m_pasteBuffer;

    //  CPU thread only, except m_now, which PostCommand also reads.
    Now                           m_now          = [] { return std::chrono::steady_clock::now(); };
    GateFn                        m_gate;
    TimePoint                     m_tickStart    = {};
    std::optional<TimePoint>      m_dispatchedAt;

    ThreadEnterFn  m_onThreadEnter;
    CommandFn      m_onCommand;
    FrameFn        m_onFrame;
    ServiceFn      m_onService;
    ThreadExitFn   m_onThreadExit;
};
