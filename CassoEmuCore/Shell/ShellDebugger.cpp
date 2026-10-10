#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/ShellDebugger.h"

#include "Config/WindowPlacementProfile.h"
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

ShellDebugger::ShellDebugger (EmulatorShell & shell) :
    m_shell (shell)
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
    return m_shell.GetHostDialogs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::IsBehindLiveForUi
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDebugger::IsBehindLiveForUi()
{
    return m_shell.m_machine.GetHostInputGate().IsHeld();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::DoesDebuggerFileExist
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDebugger::DoesDebuggerFileExist (const std::wstring & path)
{
    return m_shell.m_uiFs.Exists (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::PrepareFramebuffers
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::PrepareFramebuffers()
{
    m_shell.AllocateFramebuffers();
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

        hr = m_debuggerWindow->Create (m_shell.m_hInstance, m_shell.m_hwnd, &m_shell.m_chromeTheme, this, activate);
        CHRF (hr, m_debuggerWindow.reset());

        m_shell.ApplyAppIconToWindow (m_debuggerWindow->GetHwnd());

        //  While the debugger's title bar is held the OS runs its own move
        //  loop on this thread, so no frame runs for it or for the machine's
        //  window. A frame pumped from that loop keeps both of them going, as
        //  the printer's window does.
        m_debuggerWindow->SetOnModalLoopTick ([this] ()
        {
            m_shell.TryPresentUiFrame();
        });
    }

    m_debuggerWindow->Show (activate);

    SetDebugWindowShown (true);
    m_shell.m_cpuManager.PostCommand (IDM_DEBUG_OPEN);

Error:
    return;
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



    SetDebugWindowShown (false);
    m_shell.m_cpuManager.PostCommand (IDM_DEBUG_CLOSE, isDetach ? "detach" : "");
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
//  GetDebuggerKeyScheme
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebugger::GetDebuggerKeyScheme()
{
    return m_shell.m_globalPrefs.debuggerKeyScheme;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerKeyScheme
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerKeyScheme (const std::string & name)
{
    m_shell.m_globalPrefs.debuggerKeyScheme = name;
    m_shell.SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerTheme
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebugger::GetDebuggerTheme()
{
    return m_shell.m_globalPrefs.debuggerTheme;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerTheme
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerTheme (const std::string & name)
{
    m_shell.m_globalPrefs.debuggerTheme = name;
    m_shell.SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetReverseOptions
//
////////////////////////////////////////////////////////////////////////////////

ReverseOptions ShellDebugger::GetReverseOptions()
{
    ReverseOptions  options;



    options.isRecording = m_shell.m_globalPrefs.reverseRecording;
    options.budgetMb    = m_shell.m_globalPrefs.reverseBudgetMb;
    return options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetReverseOptions
//
//  Saved, and posted to the CPU thread, which owns the reverse host and
//  applies them at once.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetReverseOptions (const ReverseOptions & options)
{
    m_shell.m_globalPrefs.reverseRecording = options.isRecording;
    m_shell.m_globalPrefs.reverseBudgetMb  = options.budgetMb;
    m_shell.SaveGlobalPrefsDeferred();

    m_shell.PostCommand (IDM_DEBUG_REVERSE_OPTIONS, CpuCommandDispatcher::FormatReverseOptionsPayload (options.isRecording, options.budgetMb));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerLayout
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebugger::GetDebuggerLayout()
{
    return m_shell.m_globalPrefs.debuggerLayout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerLayout
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerLayout (const std::string & text)
{
    if (m_shell.m_globalPrefs.debuggerLayout == text)
    {
        return;
    }

    m_shell.m_globalPrefs.debuggerLayout = text;
    m_shell.SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerClosedPanes
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebugger::GetDebuggerClosedPanes()
{
    return m_shell.m_globalPrefs.debuggerClosedPanes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerClosedPanes
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerClosedPanes (const std::string & text)
{
    if (m_shell.m_globalPrefs.debuggerClosedPanes == text)
    {
        return;
    }

    m_shell.m_globalPrefs.debuggerClosedPanes = text;
    m_shell.SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerCommandBarDock
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebugger::GetDebuggerCommandBarDock()
{
    return m_shell.m_globalPrefs.debuggerCommandBarDock;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerCommandBarDock
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerCommandBarDock (const std::string & text)
{
    if (m_shell.m_globalPrefs.debuggerCommandBarDock == text)
    {
        return;
    }

    m_shell.m_globalPrefs.debuggerCommandBarDock = text;
    m_shell.SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerTimelineDock
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebugger::GetDebuggerTimelineDock()
{
    return m_shell.m_globalPrefs.debuggerTimelineDock;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerTimelineDock
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerTimelineDock (const std::string & text)
{
    if (m_shell.m_globalPrefs.debuggerTimelineDock == text)
    {
        return;
    }

    m_shell.m_globalPrefs.debuggerTimelineDock = text;
    m_shell.SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerFocusedPane
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebugger::GetDebuggerFocusedPane()
{
    return m_shell.m_globalPrefs.debuggerFocusedPane;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerFocusedPane
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerFocusedPane (const std::string & text)
{
    if (m_shell.m_globalPrefs.debuggerFocusedPane == text)
    {
        return;
    }

    m_shell.m_globalPrefs.debuggerFocusedPane = text;
    m_shell.SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerTextZoomPercent
//
////////////////////////////////////////////////////////////////////////////////

int ShellDebugger::GetDebuggerTextZoomPercent()
{
    return m_shell.m_globalPrefs.debuggerTextZoomPercent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerTextZoomPercent
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerTextZoomPercent (int percent)
{
    if (m_shell.m_globalPrefs.debuggerTextZoomPercent == percent)
    {
        return;
    }

    m_shell.m_globalPrefs.debuggerTextZoomPercent = percent;
    m_shell.SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerDisassemblyOptions
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebugger::GetDebuggerDisassemblyOptions()
{
    return m_shell.m_globalPrefs.debuggerDisassemblyOptions;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerDisassemblyOptions
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerDisassemblyOptions (const std::string & text)
{
    if (m_shell.m_globalPrefs.debuggerDisassemblyOptions == text)
    {
        return;
    }

    m_shell.m_globalPrefs.debuggerDisassemblyOptions = text;
    m_shell.SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerHeatMapOptions
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebugger::GetDebuggerHeatMapOptions()
{
    return m_shell.m_globalPrefs.debuggerHeatMapOptions;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerHeatMapOptions
//
//  UI thread. The CPU thread is told every time, since the window sends its
//  options as it opens, when the machine has only the defaults; the setting
//  is saved only when it changed.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerHeatMapOptions (const std::string & text)
{
    m_shell.m_cpuManager.PostCommand (IDM_DEBUG_VIEW, "heatmap options " + text);

    if (m_shell.m_globalPrefs.debuggerHeatMapOptions == text)
    {
        return;
    }

    m_shell.m_globalPrefs.debuggerHeatMapOptions = text;
    m_shell.SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResetDebuggerHeatMap
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::ResetDebuggerHeatMap()
{
    m_shell.m_cpuManager.PostCommand (IDM_DEBUG_VIEW, "heatmap reset");
}





////////////////////////////////////////////////////////////////////////////////
//
//  SendDebuggerHeatMapRequest
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SendDebuggerHeatMapRequest (const std::string & words)
{
    m_shell.m_cpuManager.PostCommand (IDM_DEBUG_VIEW, "heatmap " + words);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerHeatMapRanges
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebugger::GetDebuggerHeatMapRanges()
{
    return m_shell.m_globalPrefs.debuggerHeatMapRanges;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerHeatMapRanges
//
//  UI thread. The ranges are the window's alone, read against the symbols
//  each snapshot carries, so the CPU thread is told nothing.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerHeatMapRanges (const std::string & text)
{
    if (m_shell.m_globalPrefs.debuggerHeatMapRanges == text)
    {
        return;
    }

    m_shell.m_globalPrefs.debuggerHeatMapRanges = text;
    m_shell.SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerOpenViews
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebugger::GetDebuggerOpenViews()
{
    return m_shell.m_globalPrefs.debuggerOpenViews;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerOpenViews
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerOpenViews (const std::string & text)
{
    if (m_shell.m_globalPrefs.debuggerOpenViews == text)
    {
        return;
    }

    m_shell.m_globalPrefs.debuggerOpenViews = text;
    m_shell.SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerPlacementKey
//
//  Saving and restoring must agree on the key. The topology key names the
//  monitor arrangement and nothing else, so the debugger window carries its
//  placement with it across screens.
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebugger::GetDebuggerPlacementKey() const
{
    return WindowPlacementProfile::BuildTopologyKey();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryGetDebuggerPlacement
//
//  Where the debugger window was left on this monitor arrangement. A
//  placement that no longer lands on any monitor is declined, so a window
//  saved on a screen since removed opens at its default place.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDebugger::TryGetDebuggerPlacement (RECT & rectPx)
{
    WindowPlacementProfile::Bounds  bounds;
    WindowPlacementProfile          profile (m_shell.m_globalPrefs);
    std::string                     key      = GetDebuggerPlacementKey();
    RECT                            saved    = {};



    WindowTrace::Log ("restore.lookup", "debugger", "key=" + key);

    //  THE KEY STANDS FOR THE MONITOR ARRANGEMENT, and an arrangement
    //  differing by a screen not yet attached at startup is a different key
    //  with nothing under it. Rather than open at a default the user never chose,
    //  any placement they made under another arrangement is taken, so long as
    //  it still lands on a screen.
    if (!profile.TryLoad (key, bounds, WindowPlacementProfile::Target::Debugger))
    {
        for (const auto & [otherKey, other] : profile.GetAll (WindowPlacementProfile::Target::Debugger))
        {
            RECT  candidate = RECT { other.x, other.y, other.x + other.w, other.y + other.h };

            if (other.w > 0 && other.h > 0 && MonitorFromRect (&candidate, MONITOR_DEFAULTTONULL) != nullptr)
            {
                WindowTrace::LogRect ("restore.other", "debugger", candidate, "saved under key=" + otherKey);
                rectPx = candidate;
                return true;
            }
        }

        return false;
    }

    saved = RECT { bounds.x, bounds.y, bounds.x + bounds.w, bounds.y + bounds.h };

    if (MonitorFromRect (&saved, MONITOR_DEFAULTTONULL) == nullptr)
    {
        return false;
    }

    rectPx = saved;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerPlacement
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerPlacement (const RECT & rectPx)
{
    WindowPlacementProfile          profile (m_shell.m_globalPrefs);
    WindowPlacementProfile::Bounds  bounds;
    std::string                     key    = GetDebuggerPlacementKey();



    bounds.x = rectPx.left;
    bounds.y = rectPx.top;
    bounds.w = (int) (rectPx.right - rectPx.left);
    bounds.h = (int) (rectPx.bottom - rectPx.top);

    WindowTrace::LogRect ("save.prefs", "debugger", rectPx, "key=" + key);
    profile.Save (key, bounds, WindowPlacementProfile::Target::Debugger);
    m_shell.SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindDebuggerSource
//
//  A folder where a file was found goes into the preferences, so they are
//  saved; the next search, in this session or another, looks there first.
//
////////////////////////////////////////////////////////////////////////////////

SourceLookup ShellDebugger::FindDebuggerSource (const DebugSourceFile & record, const std::wstring & debugFilePath,
                                                const std::string & programKey)
{
    SourcePathList  paths   (m_shell.m_globalPrefs);
    SourceService   service (m_shell.m_uiFs, paths);
    SourceLookup    lookup  = service.Find (record, debugFilePath, programKey);



    if (lookup.match == SourceMatch::Exact || lookup.match == SourceMatch::Unverified)
    {
        m_shell.SaveGlobalPrefsDeferred();
    }

    return lookup;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MatchDroppedDebuggerSource
//
////////////////////////////////////////////////////////////////////////////////

SourceLookup ShellDebugger::MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> & files, const std::wstring & path,
                                                        const std::string & programKey, int & recordIndex)
{
    SourcePathList  paths   (m_shell.m_globalPrefs);
    SourceService   service (m_shell.m_uiFs, paths);
    SourceLookup    lookup  = service.MatchDropped (files, path, programKey, recordIndex);



    if (recordIndex >= 0)
    {
        m_shell.SaveGlobalPrefsDeferred();
    }

    return lookup;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunDebuggerCommand
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::RunDebuggerCommand (const std::string & line)
{
    m_shell.m_cpuManager.PostCommand (IDM_DEBUG_COMMAND, DebugCommandPayload::Encode (s_kWindowClientId, line));
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunDebuggerCommandInMode
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::RunDebuggerCommandInMode (const std::string & line, CommandMode mode)
{
    m_shell.m_cpuManager.PostCommand (IDM_DEBUG_COMMAND, DebugCommandPayload::Encode (s_kWindowClientId, line, mode));
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunDebuggerAction
//
//  The action waits on the shell's queue, since a command crosses to the CPU
//  thread as text and an action must not.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::RunDebuggerAction (const DebuggerAction & action)
{
    {
        std::lock_guard<std::mutex>  held (m_debugViewMutex);

        m_debugActionsPending.push_back (action);
    }

    m_shell.m_cpuManager.PostCommand (IDM_DEBUG_ACTION);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PauseDebugger
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::PauseDebugger()
{
    m_shell.m_cpuManager.PostCommand (IDM_DEBUG_PAUSE);
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
    PostMessageW (m_shell.m_hwnd, WM_COMMAND, MAKEWPARAM (commandId, 0), 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCodeViewSuffix
//
//  Nothing for the first code view, 2 to 4 for the others, as the CPU
//  thread's view names read.
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebugger::GetCodeViewSuffix (int view)
{
    return (view <= 0) ? std::string() : std::to_string (view + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerFollowView / CloseDebuggerCodeView
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerFollowView (int view)
{
    m_shell.m_cpuManager.PostCommand (IDM_DEBUG_VIEW, std::format ("follow {}", view + 1));
}


void ShellDebugger::CloseDebuggerCodeView (int view)
{
    m_shell.m_cpuManager.PostCommand (IDM_DEBUG_VIEW, std::format ("codeclose {}", view + 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerCodeAddress
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerCodeLines (int lines, int view)
{
    m_shell.m_cpuManager.PostCommand (IDM_DEBUG_VIEW, std::format ("lines{} {:04X}", GetCodeViewSuffix (view), (Word) lines));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerCodeAddress
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerCodeAddress (std::optional<Word> address, int view)
{
    m_shell.m_cpuManager.PostCommand (IDM_DEBUG_VIEW, address.has_value() ? std::format ("code{} {:04X}", GetCodeViewSuffix (view), *address)
                                                                 : std::format ("code{} pc", GetCodeViewSuffix (view)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerCodeTop
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerCodeTop (Word top, int view)
{
    m_shell.m_cpuManager.PostCommand (IDM_DEBUG_VIEW, std::format ("codetop{} {:04X}", GetCodeViewSuffix (view), top));
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
    std::string  view = (id == 1) ? std::string ("memory") : std::format ("memory{}", id);



    m_shell.m_cpuManager.PostCommand (IDM_DEBUG_VIEW, address.has_value() ? std::format ("{} {:04X}", view, *address)
                                                                 : view + " close");
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerTraceTop
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerTraceTop (std::optional<uint64_t> first)
{
    m_shell.m_cpuManager.PostCommand (IDM_DEBUG_VIEW, first.has_value() ? std::format ("trace {}", *first)
                                                               : std::string ("trace end"));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerHeatMapShown
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerHeatMapShown (bool shown)
{
    m_shell.m_cpuManager.PostCommand (IDM_DEBUG_VIEW, shown ? std::string ("heatmap on") : std::string ("heatmap off"));
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
    m_shell.m_cpuManager.PostCommand (IDM_DEBUG_VIEW, "beam");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScrollDebuggerCode
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::ScrollDebuggerCode (int lines, int view)
{
    m_shell.m_cpuManager.PostCommand (IDM_DEBUG_VIEW, std::format ("codescroll{} {}", GetCodeViewSuffix (view), lines));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDebuggerMemory
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::GoToDebuggerMemory (int window, const std::string & text)
{
    m_shell.m_cpuManager.PostCommand (IDM_DEBUG_VIEW, std::format ("goto {} {}", window, text));
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeDebuggerUpdate
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDebugger::TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> & snapshot,
                                        std::vector<std::string>                     & consoleLines)
{
    std::lock_guard<std::mutex>  held (m_debugViewMutex);
    bool                         any  = m_isDebugViewFresh || !m_debugConsolePending.empty();



    if (m_isDebugViewFresh)
    {
        snapshot           = m_debugViewSnapshot;
        m_isDebugViewFresh = false;
    }

    consoleLines.swap (m_debugConsolePending);
    m_debugConsolePending.clear();

    return any;
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
            std::lock_guard<std::mutex>  viewHeld (m_debugViewStateLock);



            lines = m_debugViewState.ExecuteConsoleLine (m_debugController->GetSession(), line, mode);
        }

        DebuggerViewState::AddCommandGap (lines);

        {
            std::lock_guard<std::mutex>  held (m_debugViewMutex);

            m_debugConsolePending.insert (m_debugConsolePending.end(), lines.begin(), lines.end());
        }

        m_isDebugViewDirty = true;
    });

    m_isDebugViewDirty = true;
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
        m_shell.m_cpuManager.SetPaused (false);
        m_debugController->GetSession().OnUserResumed();
    }

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
        m_isDebugViewDirty = true;
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
    std::vector<DebuggerAction>   actions;
    std::vector<std::string>      lines;
    std::unique_lock<std::mutex>  viewHeld (m_debugViewStateLock, std::defer_lock);



    {
        std::lock_guard<std::mutex>  held (m_debugViewMutex);

        actions.swap (m_debugActionsPending);
    }

    if (m_debugController == nullptr)
    {
        return;
    }

    viewHeld.lock();

    for (const DebuggerAction & action : actions)
    {
        std::vector<std::string>  shown = m_debugViewState.ExecuteAction (m_debugController->GetSession(), action);

        DebuggerViewState::AddCommandGap (shown);
        lines.insert (lines.end(), shown.begin(), shown.end());
    }

    viewHeld.unlock();

    {
        std::lock_guard<std::mutex>  held (m_debugViewMutex);

        m_debugConsolePending.insert (m_debugConsolePending.end(), lines.begin(), lines.end());
    }

    m_isDebugViewDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebugView
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebugView (const std::string & view, std::optional<Word> address)
{
    int                          index = 0;
    std::lock_guard<std::mutex>  viewHeld (m_debugViewStateLock);



    if (view == "follow" && address.has_value())
    {
        m_debugViewState.SetFollowView ((int) *address);
    }
    else if (view == "codeclose" && address.has_value())
    {
        m_debugViewState.CloseCodeView ((int) *address);
    }
    else if (CpuCommandDispatcher::TryGetCodeView (view, "codetop", index) && address.has_value())
    {
        m_debugViewState.OpenCodeViewAt (index, *address);
    }
    else if (CpuCommandDispatcher::TryGetCodeView (view, "lines", index) && address.has_value())
    {
        m_debugViewState.SetCodeLines ((int) *address, index);
    }
    else if (CpuCommandDispatcher::TryGetCodeView (view, "code", index) && address.has_value() && !m_debugViewState.IsCodeViewOpen (index))
    {
        m_debugViewState.OpenCodeView (index, *address);
    }
    else if (CpuCommandDispatcher::TryGetCodeView (view, "code", index) && address.has_value())
    {
        m_debugViewState.CenterCodeOn (*address, index);
    }
    else if (CpuCommandDispatcher::TryGetCodeView (view, "code", index))
    {
        m_debugViewState.ShowPcIn (index);
    }
    else if (view == "memory" && address.has_value())
    {
        m_debugViewState.SetMemoryAddress (*address);
    }
    else if (view.starts_with ("memory") && view.size() == 7)
    {
        int  id = view.back() - '0';

        if (address.has_value())
        {
            m_debugViewState.OpenMemoryWindow (id, *address);
        }
        else
        {
            m_debugViewState.CloseMemoryWindow (id);
        }
    }

    m_isDebugViewDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScrollDebugCode
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::ScrollDebugCode (int lines, int view)
{
    std::lock_guard<std::mutex>  viewHeld (m_debugViewStateLock);



    m_debugViewState.ScrollCode (lines, view);
    m_isDebugViewDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDebugMemory
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::GoToDebugMemory (int window, const std::string & text)
{
    std::lock_guard<std::mutex>  viewHeld (m_debugViewStateLock);



    if (m_debugController != nullptr)
    {
        m_debugViewState.RequestGoTo (m_debugController->GetSession(), window, text);
        m_isDebugViewDirty = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebugTraceView
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebugTraceView (std::optional<uint64_t> first)
{
    std::lock_guard<std::mutex>  viewHeld (m_debugViewStateLock);



    m_debugViewState.SetTraceTop (first);
    m_isDebugViewDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebugHeatMapShown
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebugHeatMapShown (bool shown)
{
    m_debugViewState.SetHeatMapShown (shown);
    m_isDebugViewDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebugHeatMapOptions
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebugHeatMapOptions (const std::string & text)
{
    m_debugViewState.SetHeatMapOptions (HeatMapOptions::FromText (text));
    m_isDebugViewDirty = true;
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

    m_isDebugViewDirty = true;
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

    m_isDebugViewDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebugHeatMapHover
//
//  CPU thread. The next snapshot carries the cell's last writer and reader.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebugHeatMapHover (std::optional<Word> address)
{
    m_debugViewState.SetHeatMapHover (address);
    m_isDebugViewDirty = true;
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
    bool             isRecording = m_reverseHost != nullptr && m_reverseHost->IsRecording();
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
        oldest = m_reverseHost->GetController().GetOldestPosition();
    }

    plan = HeatAccessJump::Plan (request, state, access, isRecording, oldest);

    switch (plan.kind)
    {
    case HeatAccessPlan::Kind::ShowCode:
        SetDebugView ("code", plan.pc);
        break;

    case HeatAccessPlan::Kind::Seek:
        if (!m_shell.m_cpuManager.IsPaused())
        {
            m_shell.m_cpuManager.SetPaused (true);
            NotifyDebugPauseChanged (true);
        }

        RunReverseCommand (ReverseCommand::Seek, plan.position);
        break;

    default:
        {
            std::lock_guard<std::mutex>  held (m_debugViewMutex);

            m_debugConsolePending.push_back (plan.message);
        }

        break;
    }

    m_isDebugViewDirty = true;
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
    uint64_t  colorSig = m_shell.ComputeColorSig();



    if (colorSig == m_shell.m_lastRenderColorSig)
    {
        return;
    }

    m_shell.RenderFramebuffer();
    m_shell.PublishFramebuffer();

    m_shell.m_lastRenderColorSig = colorSig;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PublishDebuggerView
//
//  Once a frame while the machine runs, and at once after anything the window
//  did; DebuggerViewState::IsBuildDue says which. This thread only gathers:
//  the panes are built on the publisher's queue.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::PublishDebuggerView()
{
    HRESULT               hr         = S_OK;
    ULONGLONG             now        = GetTickCount64();
    bool                  isDue      = false;
    bool                  isBreaking = false;
    bool                  isTaken    = true;
    DebugViewInput        input;
    DebuggerViewSnapshot  live;



    if (m_debugController == nullptr)
    {
        return;
    }

    //  The heat map records for its pane, which no one sees while the window
    //  is closed, and for BRKUNINIT, which needs it whatever is shown; then it
    //  is folded now and then, so its counts never pile up unfolded.
    if (!m_isDebugWindowShown.load())
    {
        isBreaking = m_debugController->GetSession().HasUnwrittenBreak();

        m_debugController->GetSession().GetTarget().SetHeatMapOn (isBreaking);

        if (isBreaking && now - m_heatFoldedAt >= kHiddenHeatFoldMs)
        {
            (void) m_debugController->GetSession().GetTarget().FoldHeatMap();
            m_heatFoldedAt = now;
        }

        return;
    }

    {
        std::lock_guard<std::mutex>  held (m_debugViewMutex);

        isTaken = !m_isDebugViewFresh;
    }

    isDue = DebuggerViewState::IsBuildDue (m_isDebugViewDirty, m_shell.m_cpuManager.IsPaused(), m_wasPausedAtDebugBuild,
                                           isTaken, now, m_debugViewBuiltAt);

    //  A stopped machine waiting on the heat map's rebuild, or the call
    //  record's, builds again a frame apart, so what is rebuilt shows when
    //  it comes in.
    isDue = isDue || ((m_wasHeatRebuilding || m_wasCallRebuilding) && now - m_debugViewBuiltAt >= DebuggerViewState::kBuildIntervalMs);

    if (!isDue)
    {
        return;
    }

    //  The clock panel reports the speed, which the CPU manager paces and the
    //  machine does not know.
    m_shell.m_machine.SetSpeedMode (m_shell.m_cpuManager.GetSpeedMode());

    //  Anything the window did is shown at once, the heat map included.
    if (m_isDebugViewDirty)
    {
        m_debugViewState.MarkHeatMapReadDue();
    }

    //  The heat map follows the machine through history.
    SyncHeatHistory (true);

    if (!m_debugBuildQueue.IsCreated())
    {
        hr = m_debugBuildQueue.Create (kDebugBuildQueueCapacity, L"Casso debugger build");
        IGNORE_RETURN_VALUE (hr, S_OK);

        //  Without a queue the publisher builds on this thread, as before.
        m_debugViewPublisher.SetQueue (m_debugBuildQueue.IsCreated() ? &m_debugBuildQueue : nullptr);

        //  Without a pool the panes of a build are built one after another.
        hr = m_debugBuildPool.Create (kDebugBuildThreads, L"Casso debugger pane builder");
        IGNORE_RETURN_VALUE (hr, S_OK);

        m_debugViewPublisher.SetRunner (m_debugBuildPool.IsCreated() ? &m_debugBuildPool : nullptr);
    }

    GatherDebugView (input, live);
    m_debugViewPublisher.Submit (std::move (input), std::move (live));

    m_debugViewBuiltAt      = now;
    m_wasPausedAtDebugBuild = m_shell.m_cpuManager.IsPaused();
    m_wasHeatRebuilding     = m_debugController->GetSession().GetTarget().IsHeatMapRebuilding();
    m_wasCallRebuilding     = m_debugController->GetCallHistory().IsRebuilding();
    m_isDebugViewDirty = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GatherDebugView
//
//  Everything the next snapshot reads that only this thread may touch: the
//  device panels and the heat map, built now since they drive the live
//  machine; the history status and, behind live, the instructions replayed
//  to get there; the machine's capture; and the session's tables. The run
//  state is the CPU manager's, not the session's, so it is handed in: the
//  command bar gates its stepping entries on it, and the code pane's
//  annotations are built only when it is paused.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::GatherDebugView (DebugViewInput & input, DebuggerViewSnapshot & live)
{
    DebugSession              & session  = m_debugController->GetSession();
    IDebugTarget              & target   = session.GetTarget();
    bool                        isPaused = m_shell.m_cpuManager.IsPaused();
    auto                        capture  = std::make_shared<DebugViewCapture>();
    uint64_t                    first    = 0;
    std::vector<TraceRecord>    entries;



    m_debugViewState.BuildLive (session, live, isPaused);
    live.history = GetHistoryStatus();

    if (live.history.isBehindLive && m_reverseHost != nullptr)
    {
        m_reverseHost->GetRecentTrace (DebuggerViewState::kHistoryTraceRows, entries);
        input.historyTrace = std::move (entries);
    }

    first = DebuggerViewState::GetTraceWindowFirst (target.GetTraceSize(), m_debugViewState.GetTraceTop(), DebuggerViewState::kTraceRows);

    DebugViewCapture::Take (target, CallRecord(), (size_t) first, DebuggerViewState::kTraceRows, *capture);
    session.TakeView (m_debugSessionView);

    input.capture  = std::move (capture);
    input.session  = m_debugSessionView;
    input.isPaused = isPaused;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PublishDebugSnapshot
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::PublishDebugSnapshot (std::shared_ptr<const DebuggerViewSnapshot> snapshot)
{
    std::lock_guard<std::mutex>  held (m_debugViewMutex);



    m_debugViewSnapshot = std::move (snapshot);
    m_isDebugViewFresh  = true;
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
    if (m_debugSession != nullptr)
    {
        m_debugSession->OnReset (isPowerCycle);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NotifyDebugMachineChanged
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::NotifyDebugMachineChanged (const std::string & machineName)
{
    //  A debugger run on the old machine ends with it, announced as a pause,
    //  and gives back the speed it borrowed.
    if (m_debugRunDriver != nullptr)
    {
        m_debugRunDriver->EndForUserPause();
    }

    if (m_debugSession != nullptr)
    {
        m_debugSession->OnMachineChanged (machineName, m_shell.m_cpuManager.IsPaused());
    }
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
    HRESULT                              hr         = S_OK;
    std::vector<BYTE>                    userSid;
    std::unique_ptr<Win32NamedPipeApi>   api;
    std::unique_ptr<Win32PipeTransport>  transport;
    std::unique_ptr<DebuggerController>  controller;
    uint32_t                             processId  = GetCurrentProcessId();



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

    api        = std::make_unique<Win32NamedPipeApi>();
    transport  = std::make_unique<Win32PipeTransport> (*api, processId, std::move (userSid));
    controller = std::make_unique<DebuggerController> (m_shell.m_machine, m_shell.m_cpuManager, *transport, m_debugFiles,
        [this] (ChannelHello & hello)
        {
            hello.title   = TextEncoding::WideToNarrow (m_shell.m_titlePrefix);
            hello.machine = m_shell.m_machine.GetConfig().name;
        },
        processId);

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
    SyncHeatHistory (false);

    SetDebugRunDriver  (nullptr);
    SetDebugSession    (nullptr);
    SetReverseStopTest (nullptr);

    m_debugController.reset();
    m_pipeTransport.reset();
    m_pipeApi.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ServiceDebugger
//
//  The CPU manager's service tick: once per pass through its loop, paused or
//  running, so a client gets a reply either way. The call record's rebuild
//  from history is looked after here too, before the panes are gathered.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::ServiceDebugger()
{
    if (m_debugController != nullptr)
    {
        m_debugController->Pump();
        ServiceCallHistory();
        PublishDebuggerView();
    }

    // A debugger edit can make a paused machine live again.
    if (m_reverseHost != nullptr)
    {
        m_reverseHost->SyncInputGate();
    }
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
    SetReverseStopTest (&m_debugController->GetReverseStopTest());

    m_debugController->GetSession().SetReverseRequester ([this] (ReverseCommand command)
    {
        bool  isRecording = m_reverseHost != nullptr && m_reverseHost->IsRecording();



        if (isRecording)
        {
            PostReverseCommand (command);
        }

        return isRecording;
    });

    //  Loading replaces the machine, so both go to the CPU thread as the
    //  File menu's commands do, and run there between instructions.
    m_debugController->GetSession().SetHistoryGuard ([this] (const std::string & line, CommandMode mode)
    {
        return GuardHistoryEdit (line, mode);
    });

    m_debugController->GetSession().SetStateFileRequester ([this] (StateFileRequest request, const std::wstring & path)
    {
        m_shell.PostCommand ((request == StateFileRequest::Load) ? IDM_FILE_LOAD_STATE : IDM_FILE_SAVE_STATE, CpuCommandDispatcher::PathToPayload (path));
        return true;
    });
}




