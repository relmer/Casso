#include "Pch.h"

#include "Shell/IDebuggerHost.h"
#include "Config/HistorySettings.h"
#include "Shell/ShellDebugger.h"
#include "Shell/Components/ShellHistory.h"
#include "Shell/CpuManager.h"
#include "Debugger/CpuManagerRunDriver.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "Ui/Debugger/ReverseBudget.h"
#include "resource.h"





//  The console's line for a step back out with no caller in history.
static constexpr const char  * s_kNoCallerText = "No caller to step back out to.";





////////////////////////////////////////////////////////////////////////////////
//
//  RunReverseCommand
//
//  CPU thread, from the command queue: a move through history, which the
//  machine's history runs.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::RunReverseCommand (
    ReverseCommand  command,
    uint64_t        argument)
{
    m_history.RunCommand (command, argument);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyReverseOptions
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::ApplyReverseOptions (
    bool  isRecording,
    int   budgetMb)
{
    m_history.ApplyOptions (isRecording, budgetMb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DivergeHistory
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::DivergeHistory()
{
    m_history.Diverge();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHistoryRestarted
//
//  History began again, or is off when the controller is null. The heat
//  map's rebuilds, its look-ups of last accesses and the call record's
//  rebuilds are drawn on machines built as this one is, and the event logs
//  begin again with history, or without it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::OnHistoryRestarted (ReverseController * controller)
{
    if (controller != nullptr)
    {
        m_heatReplayer.SetMachine (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
        m_heatFinder.SetMachine   (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
        m_callReplayer.SetMachine (m_machine.GetConfig(), m_machine.GetCurrentMachineName());
    }

    m_eventLogs.Start (controller);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHistoryStopping
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::OnHistoryStopping()
{
    m_eventLogs.Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasNoCallSince
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDebugger::HasNoCallSince (uint64_t historyStartCycle)
{
    return m_debugSession != nullptr && m_debugSession->HasNoCallSince (historyStartCycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCallerLinks
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::GetCallerLinks (std::vector<CallerLink> & outLinks)
{
    outLinks.clear();

    if (m_debugSession != nullptr)
    {
        m_debugSession->GetCallerLinks (outLinks);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHistoryMoving
//
//  The machine's position before a reverse command, so the call record
//  starts again only if the command moves it or replays anything on it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::OnHistoryMoving()
{
    if (m_debugController != nullptr)
    {
        m_debugController->GetCallHistory().OnMoving();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHistoryMoved
//
//  Landed or not, the machine may have moved; the heat map follows it,
//  rebuilding its heat once a drag of the timeline is let go or a rewind
//  ends, and so does the call record.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::OnHistoryMoved (ReverseCommand command)
{
    bool  isScrub = command == ReverseCommand::ScrubCycle || command == ReverseCommand::RewindFrame;



    if (m_debugSession != nullptr)
    {
        m_debugSession->GetTarget().NoteHistoryMoved (isScrub);
    }

    if (m_debugController != nullptr)
    {
        m_debugController->GetCallHistory().OnMoved (isScrub);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHistoryLanded
//
//  Reported the way a step's stop is, with the outcome in the stop's
//  history, and the views built again.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::OnHistoryLanded (const ReverseResult & result)
{
    EmuCpu     * cpu = m_machine.GetCpu();
    StopEvent    stop;



    if (cpu == nullptr)
    {
        return;
    }

    stop.reason    = StopReason::Step;
    stop.pc        = cpu->GetPC();
    stop.registers = cpu->GetCpu6502()->GetRegisters();
    stop.history   = result.outcome;

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
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsDebugStepRunning
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDebugger::IsDebugStepRunning()
{
    return m_debugRunDriver != nullptr && m_debugRunDriver->IsSilent();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSliceHooks
//
//  CPU thread, once a frame. Behind live does not change within a frame:
//  a reverse command or a cut runs between frames.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerSliceHooks ShellDebugger::GetSliceHooks()
{
    DebuggerSliceHooks  hooks;



    hooks.runDriver      = m_debugRunDriver;
    hooks.heldInputWatch = &m_history.GetHeldInputWatch();
    hooks.isBehindLive   = m_history.IsBehindLive();

    return hooks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetHistoryThumbnails
//
////////////////////////////////////////////////////////////////////////////////

HistoryThumbnails * ShellDebugger::GetHistoryThumbnails()
{
    return &m_history.GetThumbnails();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SeekHistoryCycle
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SeekHistoryCycle (
    uint64_t  cycle,
    bool      isInterim)
{
    m_history.SeekCycle (cycle, isInterim);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsHistorySeekBusy
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDebugger::IsHistorySeekBusy() const
{
    return m_history.IsSeekBusy();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsReplayingHistory
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDebugger::IsReplayingHistory()
{
    return m_history.IsReplaying();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetReplayProgress
//
////////////////////////////////////////////////////////////////////////////////

ReplayProgress ShellDebugger::GetReplayProgress()
{
    return m_history.GetReplayProgress();
}





////////////////////////////////////////////////////////////////////////////////
//
//  StopReplay
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::StopReplay()
{
    m_history.StopReplay();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetReverseOptions
//
////////////////////////////////////////////////////////////////////////////////

ReverseOptions ShellDebugger::GetReverseOptions()
{
    ReverseOptions  options;



    options.isRecording = m_history.GetSettings().isRecording;
    options.budgetMb    = m_history.GetSettings().budgetMb;
    return options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetReverseOptions
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetReverseOptions (const ReverseOptions & options)
{
    bool  isSaved = false;



    isSaved = m_history.SaveOptions (options.isRecording, options.budgetMb);
    IGNORE_RETURN_VALUE (isSaved, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncHeatHistory
//
//  CPU thread. Attached, the debugger's heat map saves its counts in the
//  history and is notified of what the history does; detached, before the
//  debugger goes, the two are unlinked.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SyncHeatHistory (bool isAttached)
{
    IDebugTarget       * target     = (m_debugSession != nullptr) ? &m_debugSession->GetTarget() : nullptr;
    ReverseHost        * host       = m_history.GetReverseHost();
    ReverseController  * controller = (host != nullptr) ? &host->GetController() : nullptr;
    bool                 isLinked   = isAttached && target != nullptr && controller != nullptr;



    if (controller != nullptr)
    {
        controller->SetHistoryObserver (isLinked ? target->GetHistoryObserver() : nullptr);
    }

    if (target != nullptr)
    {
        target->AttachHistory       (isLinked ? &controller->GetKeyframes() : nullptr, isLinked ? &m_heatReplayer : nullptr);
        target->SetHeatAccessFinder (isLinked ? &m_heatFinder : nullptr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ServiceCallHistory
//
//  CPU thread, once a pass while the debugger exists: the debugger's call
//  record is rebuilt from history whenever it starts mid-run. With no
//  history the two are unlinked, and a rebuild under way is dropped. The
//  debugger going drops one too: its call history unlinks itself as it is
//  destroyed.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::ServiceCallHistory()
{
    ReverseHost        * host       = m_history.GetReverseHost();
    ReverseController  * controller = (host != nullptr) ? &host->GetController() : nullptr;
    bool                 isLinked   = controller != nullptr;



    if (m_debugController == nullptr)
    {
        return;
    }

    m_debugController->GetCallHistory().Attach (controller, isLinked ? &m_callReplayer : nullptr);
    m_debugController->GetCallHistory().Service();
}





