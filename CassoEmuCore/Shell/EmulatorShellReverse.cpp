#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/EmulatorShellInternal.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "resource.h"





//  The console's line for a step back out with no caller in history.
static constexpr const char  * s_kNoCallerText = "No caller to step back out to.";





////////////////////////////////////////////////////////////////////////////////
//
//  StartReverseRecording
//
//  Begins the machine's history where it stands, once it is built and power
//  cycled, from the settings the emulator read at start or Tools > Options
//  set since; with recording off, any history is dropped instead. The first call creates the host,
//  whose live callback hands the game-port input held back while the
//  machine was behind live to the UI thread, which writes it and releases the
//  keys and mouse button let go of in the meantime. Step back out asks the
//  debugger's call record, while one is attached, whether any call in
//  history entered the code now running.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::StartReverseRecording()
{
    HRESULT  hr = S_OK;



    if (!m_isReverseOn)
    {
        StopReverseRecording();
        return;
    }

    if (m_reverseHost == nullptr)
    {
        m_reverseHost = std::make_unique<ReverseHost> (m_machine);
        m_reverseHost->GetController().SetReplayControl (&m_replayControl);

        m_reverseHost->GetController().SetCallerProbe ([this] (uint64_t historyStartCycle)
        {
            return m_debugSession != nullptr && m_debugSession->HasNoCallSince (historyStartCycle);
        });

        m_reverseHost->SetLiveCallback ([this] ()
        {
            PostMessageW (m_hwnd, WM_APP_GAMEPORT_FLUSH, 0, 0);
        });
    }

    hr = m_reverseHost->StartRecording (ReverseHost::MakeSettings (m_reverseBudgetMb, m_reverseIntervalFrames));

    if (FAILED (hr))
    {
        DEBUGMSG (L"Reverse execution could not start recording: 0x%08X\n", hr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StopReverseRecording
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::StopReverseRecording()
{
    if (m_reverseHost != nullptr)
    {
        m_reverseHost->StopRecording();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyReverseOptions
//
//  CPU thread: Tools > Options' settings, taken at once. Turning recording
//  off makes the machine live first, announced as any return to live is,
//  then drops history; turning it on starts recording where the machine
//  stands. A new budget applies to the history already held: a smaller one
//  drops the oldest snapshots until it fits, and a larger one grows the
//  store now.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplyReverseOptions (
    bool  isRecording,
    int   budgetMb)
{
    HRESULT  hr          = S_OK;
    bool     wasOn       = m_isReverseOn && m_reverseHost != nullptr && m_reverseHost->IsRecording();
    size_t   budgetBytes = ReverseHost::MakeSettings (budgetMb, m_reverseIntervalFrames).keyframes.budgetBytes;



    m_isReverseOn     = isRecording;
    m_reverseBudgetMb = budgetMb;

    if (!isRecording)
    {
        if (wasOn && m_reverseHost->IsBehindLive())
        {
            RunReverseCommand (ReverseCommand::GoLive, 0);
        }

        StopReverseRecording();
    }
    else if (!wasOn)
    {
        StartReverseRecording();
    }
    else
    {
        hr = m_reverseHost->GetController().GetKeyframes().ChangeBudget (budgetBytes);
        CHR (hr);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PostReverseCommand
//
//  UI thread: queues the command for the CPU thread, which owns the machine.
//  A stop asked for before now belongs to an earlier command, and is dropped.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PostReverseCommand (
    ReverseCommand  command,
    uint64_t        argument)
{
    m_replayControl.isStopRequested.store (false, memory_order_release);

    PostCommand (IDM_DEBUG_REVERSE, CpuCommandDispatcher::FormatReversePayload (command, argument));
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunReverseCommand
//
//  CPU thread. Moves the machine through its history and announces where it
//  landed the way a step's stop is announced, with the outcome in the stop's
//  history. The replay ran the speaker, whose clicks are not sound to play,
//  and the screen is drawn from the state the machine now holds.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RunReverseCommand (
    ReverseCommand  command,
    uint64_t        argument)
{
    HRESULT        hr      = S_OK;
    EmuCpu       * cpu     = m_machine.GetCpu();
    AppleSpeaker * speaker = m_machine.GetRefs().speaker;
    ReverseResult  result;
    StopEvent      stop;



    BAIL_OUT_IF (m_reverseHost == nullptr || cpu == nullptr, S_OK);

    //  The debugger's status bar reads these while the replay runs, since no
    //  snapshot is built until it ends.
    m_replayControl.progress.store (ReplayControl::kNoProgress, memory_order_relaxed);
    m_replayStartedAt.store (GetTickCount64(), memory_order_relaxed);
    m_isReplayingHistory.store (true, memory_order_release);

    hr = m_reverseHost->Execute (command, argument, m_reverseStopTest, result);

    m_isReplayingHistory.store (false, memory_order_release);
    CHR (hr);

    if (speaker != nullptr)
    {
        speaker->ClearTimestamps();
        speaker->BeginFrame();
    }

    stop.reason    = StopReason::Step;
    stop.pc        = cpu->GetPC();
    stop.registers = cpu->GetCpu6502()->GetRegisters();
    stop.history   = result.outcome;

    m_lastReverseOutcome  = result.outcome;
    m_lastReversePosition = m_machine.GetPosition();

    if (result.outcome == ReverseOutcome::NoCaller)
    {
        std::lock_guard<std::mutex>  held (m_debugViewMutex);

        m_debugConsolePending.push_back (s_kNoCallerText);
    }

    if (m_debugSession != nullptr)
    {
        m_debugSession->OnStopped (stop);
    }

    m_isDebugViewDirty = true;

    RenderFramebuffer();
    PublishFramebuffer();

Error:
    if (FAILED (hr))
    {
        DEBUGMSG (L"Reverse execution command %d failed: 0x%08X\n", static_cast<int> (command), hr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetReplayProgress
//
//  UI thread: how long the running reverse command has run and how much of
//  history it has searched, read from what the CPU thread sets as it goes.
//
////////////////////////////////////////////////////////////////////////////////

ReplayProgress EmulatorShell::GetReplayProgress()
{
    ReplayProgress  progress;



    progress.isReplaying = m_isReplayingHistory.load (memory_order_acquire);

    if (progress.isReplaying)
    {
        progress.elapsedMs = GetTickCount64() - m_replayStartedAt.load (memory_order_relaxed);
        progress.fraction  = m_replayControl.progress.load (memory_order_relaxed);
    }

    return progress;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetHistoryStatus
//
//  The last reverse command's outcome stands until the machine moves from
//  where it landed, by running or by a step, and is dropped then.
//
////////////////////////////////////////////////////////////////////////////////

HistoryStatus EmulatorShell::GetHistoryStatus()
{
    HistoryStatus  status;
    bool           hasMoved = !m_cpuManager.IsPaused() || m_machine.GetPosition() != m_lastReversePosition;



    if (m_reverseHost != nullptr)
    {
        status = m_reverseHost->GetStatus();
    }

    if (hasMoved)
    {
        m_lastReverseOutcome.reset();
    }

    status.outcome = m_lastReverseOutcome;
    return status;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildHistoryTrace
//
//  Behind live, the trace pane lists the instructions that led to where the
//  machine stands, from the step table the last reverse command left.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::BuildHistoryTrace (DebuggerViewSnapshot & snapshot)
{
    std::vector<TraceRecord>  entries;



    if (m_reverseHost == nullptr || m_debugger == nullptr)
    {
        return;
    }

    m_reverseHost->GetRecentTrace (DebuggerViewState::kHistoryTraceRows, entries);

    DebuggerViewState::ApplyHistoryTrace (m_debugger->GetSession(), std::move (entries), snapshot);
}





