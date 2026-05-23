#pragma once

#include "Pch.h"

#include "MenuSystem.h"





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
    WORD         id;
    std::string  payload;
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


    CpuManager  ();
    ~CpuManager ();


    HRESULT Start (ThreadEnterFn  onThreadEnter,
                   CommandFn      onCommand,
                   FrameFn        onFrame,
                   ThreadExitFn   onThreadExit);
    void    Stop  ();

    void    PostCommand    (WORD id, const std::string & payload = {});

    bool    IsRunning      () const noexcept;
    bool    IsPaused       () const noexcept;
    void    SetPaused      (bool paused) noexcept;
    bool    TogglePaused   () noexcept;

    SpeedMode  GetSpeedMode  () const noexcept;
    void       SetSpeedMode  (SpeedMode mode) noexcept;

    // Shared storage exposed for ClipboardManager wiring. The paste
    // buffer is guarded by the same mutex as the command queue so
    // both UI-thread enqueue paths can take a single lock.
    std::mutex   & GetCommandMutex () noexcept { return m_cmdMutex; }
    std::string  & GetPasteBuffer  () noexcept { return m_pasteBuffer; }

private:
    void ThreadProc ();
    void DrainCommandQueue ();


    std::thread                   m_thread;

    std::atomic<bool>             m_running    { true };
    std::atomic<bool>             m_paused     { false };
    std::atomic<SpeedMode>        m_speedMode  { SpeedMode::Authentic };

    std::mutex                    m_pauseMutex;
    std::condition_variable       m_pauseCV;

    std::mutex                    m_cmdMutex;
    std::vector<EmulatorCommand>  m_commandQueue;
    std::string                   m_pasteBuffer;

    ThreadEnterFn  m_onThreadEnter;
    CommandFn      m_onCommand;
    FrameFn        m_onFrame;
    ThreadExitFn   m_onThreadExit;
};
