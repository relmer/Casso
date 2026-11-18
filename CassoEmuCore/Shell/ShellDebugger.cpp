#include "Pch.h"

#include "Shell/IDebuggerHost.h"
#include "Shell/ShellDebugger.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Shell/CpuManager.h"

#include "Config/WindowPlacementProfile.h"
#include "Core/TextEncoding.h"
#include "Debugger/DebugCommandPayload.h"
#include "Debugger/Channel/PipeSecurity.h"
#include "Debugger/Channel/Win32NamedPipeApi.h"
#include "Debugger/Channel/Win32PipeTransport.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "resource.h"
#include "Debugger/Source/SourcePathList.h"





//  The client id the window's own commands carry. Channel clients are numbered
//  by the transport from 1, so 0 is never one of theirs.
static constexpr uint32_t  s_kWindowClientId = 0;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::ShellDebugger
//
////////////////////////////////////////////////////////////////////////////////

ShellDebugger::ShellDebugger (IDebuggerHost & host, MachineHost & machine, CpuManager & cpuManager, DebuggerSettings & settings) :
    m_host       (host),
    m_machine    (machine),
    m_cpuManager (cpuManager),
    m_settings   (settings)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::~ShellDebugger
//
////////////////////////////////////////////////////////////////////////////////

ShellDebugger::~ShellDebugger() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::GetHostDialogs
//
////////////////////////////////////////////////////////////////////////////////

IHostDialogs & ShellDebugger::GetHostDialogs() noexcept
{
    return m_host.GetHostDialogs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::DoesDebuggerFileExist
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDebugger::DoesDebuggerFileExist (const std::wstring & path)
{
    return m_host.GetUiFileSystem().Exists (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::PrepareFramebuffers
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::PrepareFramebuffers()
{
    m_host.PrepareFramebuffers();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenDebuggerWindow
//
//  Creates the window the first time and shows it every time, taking the
//  foreground only when activate is set: a window the launch opens leaves it
//  with the machine's window. The channel is opened on the CPU thread, where
//  the controller lives, so a window opened while the machine runs does not
//  wait on it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::OpenDebuggerWindow (bool activate)
{
    HRESULT  hr = S_OK;



    if (m_debuggerWindow == nullptr || m_debuggerWindow->GetHwnd() == nullptr)
    {
        m_debuggerWindow = std::make_unique<DebuggerWindow>();

        hr = m_debuggerWindow->Create (m_host.GetInstance(), m_host.GetMainWindow(), &m_host.GetChromeTheme(), this, activate);
        CHRF (hr, m_debuggerWindow.reset());

        m_host.ApplyAppIcon (m_debuggerWindow->GetHwnd());
        m_debuggerWindow->SetTitlePrefix (m_host.GetTitlePrefix());

        //  While the debugger's title bar is held the OS runs its own move
        //  loop on this thread, so no frame runs for it or for the machine's
        //  window. A frame pumped from that loop keeps both of them going, as
        //  the printer's window does.
        m_debuggerWindow->SetOnModalLoopTick ([this] ()
        {
            m_host.PresentUiFrame();
        });
    }

    m_debuggerWindow->Show (activate);

    m_view.SetWindowShown (true);
    m_cpuManager.PostCommand (IDM_DEBUG_OPEN);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenDebuggerLog
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::OpenDebuggerLog (DebugLogKind kind)
{
    OpenDebuggerWindow();

    if (m_debuggerWindow != nullptr)
    {
        m_debuggerWindow->OpenEventLog (kind);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsDebuggerMessageRoot
//
//  A floating pane is a top-level window of its own, owned by the debugger
//  window, so either the root itself or its owner can be the debugger.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDebugger::IsDebuggerMessageRoot (HWND root, HWND rootOwner, HWND debugger)
{
    return debugger != nullptr && (root == debugger || rootOwner == debugger);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsDebuggerMessage
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDebugger::IsDebuggerMessage (const MSG & msg) const
{
    HWND  debugger = (m_debuggerWindow != nullptr) ? m_debuggerWindow->GetHwnd() : nullptr;
    HWND  root     = (msg.hwnd != nullptr) ? GetAncestor (msg.hwnd, GA_ROOT) : nullptr;
    HWND  owner    = (root != nullptr) ? GetWindow (root, GW_OWNER) : nullptr;



    return IsDebuggerMessageRoot (root, owner, debugger);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDebuggerWindowClosed
//
//  Closing the window closes the debugger: the channel closes and every client
//  is told so. The session survives, so reopening finds the breakpoints as
//  they were left. A close that Detach asked for detaches as well.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::OnDebuggerWindowClosed()
{
    bool  isDetach = std::exchange (m_isDetachPending, false);



    m_view.SetWindowShown (false);
    m_cpuManager.PostCommand (IDM_DEBUG_CLOSE, [isDetach] (IDebugCommandTarget & target) { target.CloseDebugChannel (isDetach); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DetachDebugger
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::DetachDebugger()
{
    m_isDetachPending = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResetDebuggerHeatMap
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::ResetDebuggerHeatMap()
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [] (IDebugCommandTarget & target) { target.ResetDebugHeatMap(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerHeatMapHover
//
//  UI thread. The heat map cell the mouse is over, or none.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerHeatMapHover (std::optional<Word> address)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [address] (IDebugCommandTarget & target) { target.SetDebugHeatMapHover (address); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunDebuggerHeatMapAccess
//
//  UI thread. A cell's last write or read, to list or to go back to.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::RunDebuggerHeatMapAccess (const HeatAccessRequest & request)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [request] (IDebugCommandTarget & target) { target.RunDebugHeatMapAccess (request); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerHeatMapIgnore
//
//  UI thread. The spans whose reads before written the map omits.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerHeatMapIgnore (const std::vector<std::pair<Word, Word>> & spans)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [spans] (IDebugCommandTarget & target) { target.SetDebugHeatMapIgnore (spans); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunDebuggerCommand
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::RunDebuggerCommand (const std::string & line)
{
    m_cpuManager.PostCommand (IDM_DEBUG_COMMAND, DebugCommandPayload::Encode (s_kWindowClientId, line));
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunDebuggerCommandInMode
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::RunDebuggerCommandInMode (const std::string & line, CommandMode mode)
{
    m_cpuManager.PostCommand (IDM_DEBUG_COMMAND, DebugCommandPayload::Encode (s_kWindowClientId, line, mode));
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunDebuggerAction
//
//  The action waits in the mailbox, since a command crosses to the CPU
//  thread as text and an action must not.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::RunDebuggerAction (const DebuggerAction & action)
{
    m_mailbox.QueueAction (action, m_settings.echoActions);
    m_cpuManager.PostCommand (IDM_DEBUG_ACTION);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PauseDebugger
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::PauseDebugger()
{
    m_cpuManager.PostCommand (IDM_DEBUG_PAUSE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunEmulatorCommand
//
//  The debugger's menu bar offers some of the main window's own commands;
//  they go to the main window as its menu would send them.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::RunEmulatorCommand (int commandId)
{
    PostMessageW (m_host.GetMainWindow(), WM_COMMAND, MAKEWPARAM (commandId, 0), 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::GetEmulatorSpeed
//
//  The speed the user chose, which the emulator's speed menu checks.
//
////////////////////////////////////////////////////////////////////////////////

SpeedMode ShellDebugger::GetEmulatorSpeed()
{
    return m_cpuManager.GetUserSpeedMode();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerFollowView / CloseDebuggerCodeView
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerFollowView (int view)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [view] (IDebugCommandTarget & target) { target.SetDebugFollowView (view); });
}


void ShellDebugger::CloseDebuggerCodeView (int view)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [view] (IDebugCommandTarget & target) { target.CloseDebugCodeView (view); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerCodeAddress
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerCodeLines (int lines, int view)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [lines, view] (IDebugCommandTarget & target) { target.SetDebugCodeLines (lines, view); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerCodeAddress
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerCodeAddress (std::optional<Word> address, int view)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [address, view] (IDebugCommandTarget & target) { target.ShowDebugCode (view, address); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerCodeTop
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerCodeTop (Word top, int view)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [top, view] (IDebugCommandTarget & target) { target.OpenDebugCodeViewAt (view, top); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerMemoryWindow
//
//  Window 1 is the "memory" view; 2 to 4 are "memory2" and on, and no
//  address closes one.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerMemoryWindow (int id, std::optional<Word> address)
{



    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [id, address] (IDebugCommandTarget & target) { target.SetDebugMemoryWindow (id, address); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerTraceTop
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerTraceTop (std::optional<uint64_t> first)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [first] (IDebugCommandTarget & target) { target.SetDebugTraceView (first); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SendDebuggerEventLogRequest
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SendDebuggerEventLogRequest (
    DebugLogKind              kind,
    const DebugLogRequest   & request)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [kind, request] (IDebugCommandTarget & target) { target.SetDebugEventLog (kind, request); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerHeatMapShown
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerHeatMapShown (bool shown)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [shown] (IDebugCommandTarget & target) { target.SetDebugHeatMapShown (shown); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerHeatColumnsShown
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerHeatColumnsShown (bool shown)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [shown] (IDebugCommandTarget & target) { target.SetDebugHeatColumnsShown (shown); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerTraceShown
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerTraceShown (bool shown)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [shown] (IDebugCommandTarget & target) { target.SetDebugTraceShown (shown); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerSoundMuted
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerSoundMuted (int channel, bool isMuted)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [channel, isMuted] (IDebugCommandTarget & target) { target.SetDebugSoundMuted (channel, isMuted); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerBasicShown
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerBasicShown (bool isVisible)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [isVisible] (IDebugCommandTarget & target) { target.SetDebugBasicShown (isVisible); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetBeamOverlayOn
//
//  UI thread. The CPU thread is asked to draw the picture again, since a
//  stopped machine would otherwise show the change only at its next step.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetBeamOverlayOn (bool on)
{
    m_isBeamOverlayOn.store (on, memory_order_release);
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [] (IDebugCommandTarget & target) { target.RedrawDebugFrame(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerCodeShown
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerCodeShown (int lines, int view)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [lines, view] (IDebugCommandTarget & target) { target.SetDebugCodeShown (lines, view); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScrollDebuggerCode
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::ScrollDebuggerCode (int lines, int view)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [lines, view] (IDebugCommandTarget & target) { target.ScrollDebugCode (lines, view); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  HoldDebuggerCode
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::HoldDebuggerCode (int view, bool isHeld)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [isHeld, view] (IDebugCommandTarget & target) { target.HoldDebugCode (isHeld, view); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDebuggerMemory
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::GoToDebuggerMemory (int window, const std::string & text)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [window, text] (IDebugCommandTarget & target) { target.GoToDebugMemory (window, text); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectDebuggerCode
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SelectDebuggerCode (int view, Word address)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, [view, address] (IDebugCommandTarget & target) { target.SelectDebugCode (view, address); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenDebuggerFile
//
//  UI thread. A console link opens its file's folder with the file selected:
//  in Casso Explorer, or in File Explorer when the setting picks it or Casso
//  Explorer does not start.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::OpenDebuggerFile (const std::wstring & path)
{
    if (m_prefs.IsDebuggerLinkingToFileExplorer() || !m_host.ShowFileInCassoExplorer (path))
    {
        m_host.ShowFileInFolder (path);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenDebugChannel
//
//  CPU thread. The window's commands are run for it here, with their text
//  sent back to its console, since the window is not a channel client and
//  has no connection for a reply to travel on.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::OpenDebugChannel()
{
    HRESULT  hr = OpenDebugger();



    IGNORE_RETURN_VALUE (hr, S_OK);
    m_project.OpenStore();

    if (m_debugController == nullptr)
    {
        return;
    }

    m_debugController->GetSession().SetAttached (true);

    SetDebugCommandHandler ([this] (uint32_t clientId, const std::string & line, std::optional<CommandMode> mode)
    {
        std::vector<std::string>  lines;



        if (clientId != s_kWindowClientId || m_debugController == nullptr)
        {
            return;
        }

        {
            std::lock_guard<std::mutex>  viewHeld (m_viewModel.GetLock());



            lines = m_viewModel.GetState().ExecuteConsoleLine (m_debugController->GetSession(), line, mode);
        }

        DebuggerViewState::AddCommandGap (lines);
        m_mailbox.AddConsoleLines (lines);
        m_viewModel.MarkDirty();
    });

    m_viewModel.MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CloseDebugChannel
//
//  The channel only. The controller and its session stay, so breakpoints and
//  the machine's pause state are left as they were.
//
//  A DETACH ALSO TAKES THE DEBUGGER OFF THE MACHINE: a debugger run in
//  progress ends, the session's CPU hook comes off, and a stopped machine
//  runs freely. The breakpoints stay in the session, and opening the window
//  again attaches it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::CloseDebugChannel (bool isDetach)
{
    if (m_debugController == nullptr)
    {
        return;
    }

    if (isDetach)
    {
        if (m_debugRunDriver != nullptr)
        {
            m_debugRunDriver->EndForUserPause();
        }

        m_debugController->GetSession().SetAttached (false);
        m_cpuManager.SetPaused (false);
        m_debugController->GetSession().OnUserResumed();
    }

    m_project.Save();
    m_debugController->Close();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PauseDebugRun
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::PauseDebugRun()
{
    if (m_debugController != nullptr)
    {
        m_debugController->RequestPause();
        m_viewModel.MarkDirty();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunDebugActions
//
//  Every queued action, run directly against the session, with the console
//  lines each gives. With no debugger open there is no one to show them to,
//  so the actions are dropped.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::RunDebugActions()
{
    std::vector<DebuggerAction>   actions  = m_mailbox.TakeActions();
    std::vector<std::string>      lines;
    std::unique_lock<std::mutex>  viewHeld (m_viewModel.GetLock(), std::defer_lock);



    if (m_debugController == nullptr)
    {
        return;
    }

    viewHeld.lock();

    for (const DebuggerAction & action : actions)
    {
        std::vector<std::string>  shown = m_viewModel.GetState().ExecuteAction (m_debugController->GetSession(), action);

        DebuggerViewState::AddCommandGap (shown);
        lines.insert (lines.end(), shown.begin(), shown.end());
    }

    viewHeld.unlock();

    m_mailbox.AddConsoleLines (lines);
    m_viewModel.MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDebugMemory
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::GoToDebugMemory (int window, const std::string & text)
{
    std::lock_guard<std::mutex>  viewHeld (m_viewModel.GetLock());



    if (m_debugController != nullptr)
    {
        m_viewModel.GetState().RequestGoTo (m_debugController->GetSession(), window, text);
        m_viewModel.MarkDirty();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebugSoundMuted
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebugSoundMuted (int channel, bool isMuted)
{
    MockingboardCard             * card = m_machine.GetRefs().mockingboard;
    std::lock_guard<std::mutex>    viewHeld (m_viewModel.GetLock());



    if (card != nullptr)
    {
        card->SetSoundMuted (channel, isMuted);
    }

    m_viewModel.MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResetDebugHeatMap
//
//  CPU thread. The map is zeroed in place, so it goes on recording.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::ResetDebugHeatMap()
{
    if (m_debugController != nullptr)
    {
        m_debugController->GetSession().GetTarget().ResetHeatMap();
    }

    m_viewModel.MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebugHeatMapIgnore
//
//  CPU thread. The map leaves the spans out of its break and its count, and
//  the next snapshot out of its reads before written.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebugHeatMapIgnore (const std::vector<std::pair<Word, Word>> & spans)
{
    if (m_debugController != nullptr)
    {
        m_debugController->GetSession().GetTarget().SetUnwrittenIgnore (spans);
    }

    m_viewModel.MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunDebugHeatMapAccess
//
//  CPU thread. The access the heat map holds for the cell, as of where the
//  machine stands, or history holds where the map does not, shown in the
//  first disassembly view, or gone back to: a
//  running machine stops first, as a step back would find it. What cannot be
//  done is said in the console.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::RunDebugHeatMapAccess (const HeatAccessRequest & request)
{
    HeatLastAccess   access;
    HeatAccessState  state       = HeatAccessState::None;
    HeatAccessPlan   plan;
    bool             isRecording = m_history.IsRecording();
    uint64_t         oldest      = 0;



    if (m_debugController == nullptr)
    {
        return;
    }

    if (m_debugController->GetSession().GetTarget().FoldHeatMap() != nullptr)
    {
        state = m_debugController->GetSession().GetTarget().LookUpHeatMapAccess (HeatMapOptions::GetSpace (request.bank), request.isWrite, request.address, access);
    }

    if (isRecording)
    {
        oldest = m_history.GetReverseHost()->GetController().GetOldestPosition();
    }

    plan = HeatAccessJump::Plan (request, state, access, isRecording, oldest);

    switch (plan.kind)
    {
    case HeatAccessPlan::Kind::ShowCode:
        {
            std::lock_guard<std::mutex>  viewHeld (m_viewModel.GetLock());

            m_viewModel.GetState().SelectCodeAt (plan.pc);
        }

        break;

    case HeatAccessPlan::Kind::Seek:
        if (!m_cpuManager.IsPaused())
        {
            m_cpuManager.SetPaused (true);
            NotifyDebugPauseChanged (true);
        }

        m_history.RunCommand (ReverseCommand::Seek, plan.position);
        break;

    default:
        m_mailbox.AddConsoleLine (plan.message);
        break;
    }

    m_viewModel.MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RedrawDebugFrame
//
//  A stopped machine runs no frame, so nothing else would draw the picture
//  again when the beam mark is turned on or off. CPU thread.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::RedrawDebugFrame()
{
    m_host.RedrawStoppedFrame();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunDebugCommand
//
//  A debugger command, run on the CPU thread because that is where the machine
//  it inspects and changes is safe to touch. With no handler attached nobody is
//  debugging, so there is no one to reply to and the line is dropped.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::RunDebugCommand (uint32_t clientId, const std::string & line, std::optional<CommandMode> mode)
{
    m_debugCommandClient = clientId;

    if (m_debugCommandHandler)
    {
        m_debugCommandHandler (clientId, line, mode);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NotifyDebugPauseChanged
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::NotifyDebugPauseChanged (bool paused)
{
    // A pause during a debugger run ends that run, so a client hears one stop
    // with its budget and cycle count rather than a run that never finishes.
    if (paused && m_debugRunDriver != nullptr)
    {
        m_debugRunDriver->EndForUserPause();
    }

    if (m_debugSession == nullptr)
    {
        return;
    }

    if (paused)
    {
        m_debugSession->OnUserPaused();
    }
    else
    {
        m_debugSession->OnUserResumed();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NotifyDebugReset
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::NotifyDebugReset (bool isPowerCycle)
{
    Apple2eKeyboard  * keyboard = m_machine.GetRefs().iieKeyboard;



    if (m_debugSession != nullptr)
    {
        m_debugSession->OnReset (isPowerCycle);
    }

    //  Ctrl-Open-Apple-Reset boots the disk in drive 1 as a power cycle
    //  does: the firmware reads Open Apple after the reset and cold starts.
    if (isPowerCycle || (keyboard != nullptr && keyboard->IsOpenAppleDown()))
    {
        m_project.NoteBootDisk();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NotifyDebugDisksMounted
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::NotifyDebugDisksMounted()
{
    m_project.NoteBootDisk();
}





////////////////////////////////////////////////////////////////////////////////
//
//  NotifyDebugMachineChanged
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::NotifyDebugMachineChanged (const std::string & machineName, bool isStateLoaded)
{
    //  A debugger run on the old machine ends with it, announced as a pause,
    //  and gives back the speed it borrowed.
    if (m_debugRunDriver != nullptr)
    {
        m_debugRunDriver->EndForUserPause();
    }

    if (m_debugSession != nullptr)
    {
        m_debugSession->OnMachineChanged (machineName, m_cpuManager.IsPaused(), isStateLoaded);
    }

    //  The view is the old machine's, and a paused machine builds a new one
    //  only when the view is dirty.
    m_viewModel.MarkDirty();

    m_project.NoteBootDisk();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenDebugger
//
//  The pipe, the controller, and the shell's debug pointers at them. Nothing
//  is attached until the channel has actually opened, so a failure -- another
//  process holding this instance's pipe name -- leaves the shell exactly as it
//  was and the emulator running with no debugger.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ShellDebugger::OpenDebugger()
{
    HRESULT                              hr           = S_OK;
    std::vector<BYTE>                    userSid;
    DWORD                                integrityRid = 0;
    std::unique_ptr<Win32NamedPipeApi>   api;
    std::unique_ptr<Win32PipeTransport>  transport;
    std::unique_ptr<DebuggerController>  controller;
    uint32_t                             processId    = GetCurrentProcessId();



    //  Already built: reopen a channel the window closed. The controller and
    //  its session were kept, so this is only the pipe.
    if (m_debugController != nullptr)
    {
        BAIL_OUT_IF (m_debugController->IsOpen(), S_OK);

        hr = m_debugController->Open();
        CHR (hr);

        BAIL_OUT_IF (true, S_OK);
    }

    hr = PipeSecurityDescriptor::GetCurrentUserSid (userSid);
    CHR (hr);

    hr = PipeSecurityDescriptor::GetCurrentIntegrityLevel (integrityRid);
    CHR (hr);

    api        = std::make_unique<Win32NamedPipeApi>();
    transport  = std::make_unique<Win32PipeTransport> (*api, processId, std::move (userSid), integrityRid);
    controller = std::make_unique<DebuggerController> (m_machine, m_cpuManager, *transport, m_debugFiles,
        [this] (ChannelHello & hello)
        {
            hello.title   = TextEncoding::WideToNarrow (m_host.GetTitlePrefix());
            hello.machine = m_machine.GetConfig().name;
        },
        processId);

    controller->SetConsoleSink ([this] (const std::vector<std::string> & lines) { AddConsoleLines (lines); });

    hr = controller->Open();
    CHR (hr);

    m_pipeApi       = std::move (api);
    m_pipeTransport = std::move (transport);

    AttachDebugger (std::move (controller));

Error:
    if (FAILED (hr))
    {
        DEBUGMSG (L"The debug channel could not be opened: 0x%08X\n", hr);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CloseDebugger
//
//  Detaches before destroying, so the slice loop and the notifications never
//  reach a controller that is going away. The controller's call history
//  unlinks itself from history as it goes, dropping a rebuild under way.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::CloseDebugger()
{
    //  The project is saved and closed; the next debugger opens it again.
    m_project.Close();

    m_history.SyncHeat (false);

    SetDebugRunDriver  (nullptr);
    SetDebugSession    (nullptr);
    m_history.SetStopTest (nullptr);

    if (m_debugController != nullptr)
    {
        m_debugController->GetTarget().GetTrace().SetHistory (nullptr);
    }

    //  The panes went with the window; the logs record on.
    m_eventLogs.CloseAll();
    m_mailbox.ClearEventLogs();

    m_debugController.reset();
    m_pipeTransport.reset();
    m_pipeApi.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ServiceHitLogs
//
//  The messages logging breakpoints wrote at their hits go to the console and
//  to the debug channel's clients.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::ServiceHitLogs()
{
    std::vector<std::string>  logs = m_debugController->GetSession().TakeHitLogs();



    m_debugController->SendHitLogs (logs);

    if (!logs.empty())
    {
        m_mailbox.AddConsoleLines (logs);
        m_viewModel.MarkDirty();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ServicePendingScript
//
//  A script whose line started a run, or a move through history, goes on
//  once the machine has stopped or the move has landed, its text going to
//  the console as the lines before it did.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::ServicePendingScript()
{
    std::vector<std::string>  lines;



    if (m_debugController->GetSession().GetRunState() != RunState::Paused || m_debugController->GetSession().IsReversePending() ||
        !m_debugController->GetSession().HasPendingScript())
    {
        return;
    }

    {
        std::lock_guard<std::mutex>  viewHeld (m_viewModel.GetLock());

        lines = m_viewModel.GetState().RunPendingScript (m_debugController->GetSession());
    }

    if (!lines.empty())
    {
        m_mailbox.AddConsoleLines (lines);
    }

    m_viewModel.MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ServiceDebugger
//
//  The CPU manager's service tick: once per pass through its loop, paused or
//  running, so a client gets a reply either way. The call record's rebuild
//  from history and the event logs are looked after here too, before the
//  panes are gathered. A line a client of the channel ran, such as a step,
//  can change everything in the window, so the panes are built again.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::ServiceDebugger()
{
    uint64_t  linesRun = 0;



    if (m_debugController != nullptr)
    {
        linesRun = m_debugController->GetLinesRun();

        //  The messages written since the last pass are queued ahead of the
        //  pump, so they go out with the stop the same run made.
        ServiceHitLogs();
        m_debugController->Pump();

        if (!m_viewModel.IsDirty() && m_debugController->GetLinesRun() != linesRun)
        {
            m_viewModel.MarkDirty();
        }

        ServicePendingScript();
        m_project.Service();
        m_history.ServiceCalls();
        ServiceEventLogs();

        //  A code view's find text is made once a pause has gone without a
        //  step for a moment; the views are built again then to make it.
        if (!m_viewModel.IsDirty() && m_viewModel.GetState().IsFindTextDue (GetTickCount64()))
        {
            m_viewModel.MarkDirty();
        }

        m_view.Publish();
    }

    // A debugger edit can make a paused machine live again.
    m_history.SyncInputGate();
    m_history.ReportCuts();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AttachDebugger
//
//  The shell's debug pointers at the controller, and the session's requests
//  routed to the CPU thread's queue.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::AttachDebugger (std::unique_ptr<DebuggerController> controller)
{
    m_debugController = std::move (controller);

    SetDebugRunDriver  (&m_debugController->GetRunDriver());
    SetDebugSession    (&m_debugController->GetSession());
    m_history.SetStopTest (&m_debugController->GetReverseStopTest());

    m_debugController->GetSession().SetIoEventSource (&m_eventLogs);

    //  The trace's older instructions are history's, rebuilt at once while
    //  the machine is stopped and in the background while it runs.
    m_eventLogs.GetTraceHistory().SetIsStopped ([this] { return m_cpuManager.IsPaused(); });
    m_eventLogs.GetProfileHistory().SetIsStopped ([this] { return m_cpuManager.IsPaused(); });
    m_debugController->GetHandlers().GetExecution().SetHistoryProfiler ([this] (uint64_t fromCycle, std::optional<uint64_t> toCycle, std::string & error)
    {
        return m_eventLogs.GetProfileHistory().Build (fromCycle, error, toCycle);
    });
    m_debugController->GetTarget().GetTrace().SetHistory (&m_eventLogs.GetTraceHistory());
    m_debugController->GetSession().SetTapeControl   (&m_host.GetTapeControl());
    m_debugController->GetSession().SetMachineControl (&m_host.GetMachineControl());

    m_debugController->GetSession().SetReverseRequester ([this] (ReverseCommand command, uint64_t argument)
    {
        return m_history.TryRequest (command, argument);
    });

    //  Loading replaces the machine, so both go to the CPU thread as the
    //  File menu's commands do, and run there between instructions.
    m_debugController->GetSession().SetHistoryGuard ([this] (const DebugCommand & command)
    {
        return m_divergence.GuardEdit (command);
    });

    m_debugController->GetSession().SetStateFileRequester ([this] (StateFileRequest request, const std::wstring & path)
    {
        m_host.PostCommand ((request == StateFileRequest::Load) ? IDM_FILE_LOAD_STATE : IDM_FILE_SAVE_STATE, CpuCommandDispatcher::PathToPayload (path));
        return true;
    });

    m_debugController->GetSession().SetProjectRequester ([this] (const std::string & text, Reply & reply)
    {
        m_project.RunRequest (text, reply);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepSession
//
//  CPU thread. One instruction stepped through the attached session, so the
//  step lands where the debugger's own would; false when none is attached.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDebugger::StepSession()
{
    DebugCommand  stepInto;



    if (m_debugSession == nullptr)
    {
        return false;
    }

    stepInto.verb       = DebugVerb::StepInto;
    stepInto.sourceName = "T";

    (void) m_debugSession->Execute (stepInto);
    m_viewModel.MarkDirty();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsBeamMarkShown
//
//  The beam mark is drawn while it is turned on and the session has the
//  machine stopped.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDebugger::IsBeamMarkShown()
{
    return m_isBeamOverlayOn.load (memory_order_acquire) &&
           m_debugSession != nullptr                     &&
           m_debugSession->GetRunState() == RunState::Paused;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderWindowFrame
//
//  UI thread. The debugger window's frame, drawn with the emulator's.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::RenderWindowFrame()
{
    if (m_debuggerWindow != nullptr)
    {
        m_debuggerWindow->RenderFrame();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DestroyWindow
//
//  UI thread, at shutdown. The window posts to the CPU thread through the
//  debugger, so it is cut off before either goes.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::DestroyWindow()
{
    if (m_debuggerWindow != nullptr)
    {
        m_debuggerWindow->DetachHost();
        m_debuggerWindow.reset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ServiceEventLogs
//
//  CPU thread, each pass: the stretches rebuilt come in, and each open
//  pane's delta goes to the UI thread when it has anything in it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::ServiceEventLogs()
{
    DebugLogDeltas  deltas;



    //  A stretch of the trace rebuilt in the background is in the next snapshot.
    if (m_eventLogs.Pump())
    {
        m_viewModel.MarkDirty();
    }

    //  A profile built from history takes the place of the profile; while one
    //  builds, each snapshot has how far it is.
    {
        ProfileCountsPtr  counts;

        if (m_eventLogs.GetProfileHistory().Pump (counts) && m_debugController != nullptr)
        {
            m_debugController->GetHandlers().GetExecution().InstallProfile (*counts);
            m_viewModel.MarkDirty();
        }

        if (!m_viewModel.IsDirty() && m_eventLogs.GetProfileHistory().GetProgress().has_value())
        {
            m_viewModel.MarkDirty();
        }
    }

    if (m_eventLogs.TakeDeltas (deltas))
    {
        m_mailbox.AddEventLogs (deltas);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::GetSliceHooks
//
//  CPU thread, once a frame. Behind live does not change within a frame:
//  a reverse command or a cut runs between frames.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerSliceHooks ShellDebugger::GetSliceHooks()
{
    DebuggerSliceHooks  hooks;



    hooks.runDriver      = m_debugRunDriver;
    hooks.heldInputWatch = &m_divergence.GetHeldInputWatch();
    hooks.isBehindLive   = m_history.IsBehindLive();

    return hooks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::AddConsoleLines
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::AddConsoleLines (const std::vector<std::string> & lines)
{
    if (lines.empty())
    {
        return;
    }

    m_mailbox.AddConsoleLines (lines);
    m_viewModel.MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::GetQuestionOwner
//
//  Over the debugger's window, when it is open, a question is in the
//  debugger's theme.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::GetQuestionOwner (bool isActiveOnly, HWND & owner, const IDxuiTheme * & theme)
{
    HWND  debugger = (m_debuggerWindow != nullptr) ? m_debuggerWindow->GetHwnd() : nullptr;



    owner = m_host.GetMainWindow();
    theme = &m_host.GetChromeTheme();

    if (debugger == nullptr || (isActiveOnly && GetActiveWindow() != debugger))
    {
        return;
    }

    owner = debugger;
    theme = (m_debuggerWindow->GetThemeInForce() != nullptr) ? m_debuggerWindow->GetThemeInForce() : theme;
}





