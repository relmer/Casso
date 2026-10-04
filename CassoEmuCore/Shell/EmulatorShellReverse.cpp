#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/EmulatorShellInternal.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StartReverseRecording
//
//  Begins the machine's history where it stands, once it is built and power
//  cycled, from the settings the emulator read at start; with recording off
//  there, any history is dropped instead. The first call creates the host,
//  whose live callback hands the game-port input held back while the
//  machine was behind live to the UI thread, which writes it and releases the
//  keys and mouse button let go of in the meantime.
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
//  PostReverseCommand
//
//  UI thread: queues the command for the CPU thread, which owns the machine.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PostReverseCommand (
    ReverseCommand  command,
    uint64_t        argument)
{
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

    hr = m_reverseHost->Execute (command, argument, m_reverseStopTest, result);
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





