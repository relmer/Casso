#include "Pch.h"

#include "Shell/EmulatorShell.h"

#include "Config/WindowPlacementProfile.h"
#include "Debugger/DebugCommandPayload.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "resource.h"
#include "Debugger/Source/SourcePathList.h"





//  The client id the window's own commands carry. Channel clients are numbered
//  by the transport from 1, so 0 is never one of theirs.
static constexpr uint32_t  s_kWindowClientId = 0;





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

void EmulatorShell::OpenDebuggerWindow (bool activate)
{
    HRESULT  hr = S_OK;



    if (m_debuggerWindow == nullptr || m_debuggerWindow->GetHwnd() == nullptr)
    {
        m_debuggerWindow = std::make_unique<DebuggerWindow>();

        hr = m_debuggerWindow->Create (m_hInstance, m_hwnd, &m_chromeTheme, this, activate);
        CHRF (hr, m_debuggerWindow.reset());

        ApplyAppIconToWindow (m_debuggerWindow->GetHwnd());

        //  While the debugger's title bar is held the OS runs its own move
        //  loop on this thread, so no frame runs for it or for the machine's
        //  window. A frame pumped from that loop keeps both of them going, as
        //  the printer's window does.
        m_debuggerWindow->SetOnModalLoopTick ([this] ()
        {
            TryPresentUiFrame();
        });
    }

    m_debuggerWindow->Show (activate);

    SetDebugWindowShown (true);
    m_cpuManager.PostCommand (IDM_DEBUG_OPEN);

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

bool EmulatorShell::IsDebuggerMessageRoot (HWND root, HWND rootOwner, HWND debugger)
{
    return debugger != nullptr && (root == debugger || rootOwner == debugger);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsDebuggerMessage
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::IsDebuggerMessage (const MSG & msg) const
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

void EmulatorShell::OnDebuggerWindowClosed()
{
    bool  isDetach = std::exchange (m_isDetachPending, false);



    SetDebugWindowShown (false);
    m_cpuManager.PostCommand (IDM_DEBUG_CLOSE, isDetach ? "detach" : "");
}





////////////////////////////////////////////////////////////////////////////////
//
//  DetachDebugger
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::DetachDebugger()
{
    m_isDetachPending = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerKeyScheme
//
////////////////////////////////////////////////////////////////////////////////

std::string EmulatorShell::GetDebuggerKeyScheme()
{
    return m_globalPrefs.debuggerKeyScheme;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerKeyScheme
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebuggerKeyScheme (const std::string & name)
{
    m_globalPrefs.debuggerKeyScheme = name;
    SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerTheme
//
////////////////////////////////////////////////////////////////////////////////

std::string EmulatorShell::GetDebuggerTheme()
{
    return m_globalPrefs.debuggerTheme;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerTheme
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebuggerTheme (const std::string & name)
{
    m_globalPrefs.debuggerTheme = name;
    SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetReverseOptions
//
////////////////////////////////////////////////////////////////////////////////

ReverseOptions EmulatorShell::GetReverseOptions()
{
    ReverseOptions  options;



    options.isRecording = m_globalPrefs.reverseRecording;
    options.budgetMb    = m_globalPrefs.reverseBudgetMb;
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

void EmulatorShell::SetReverseOptions (const ReverseOptions & options)
{
    m_globalPrefs.reverseRecording = options.isRecording;
    m_globalPrefs.reverseBudgetMb  = options.budgetMb;
    SaveGlobalPrefsDeferred();

    PostCommand (IDM_DEBUG_REVERSE_OPTIONS, CpuCommandDispatcher::FormatReverseOptionsPayload (options.isRecording, options.budgetMb));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerLayout
//
////////////////////////////////////////////////////////////////////////////////

std::string EmulatorShell::GetDebuggerLayout()
{
    return m_globalPrefs.debuggerLayout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerLayout
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebuggerLayout (const std::string & text)
{
    if (m_globalPrefs.debuggerLayout == text)
    {
        return;
    }

    m_globalPrefs.debuggerLayout = text;
    SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerClosedPanes
//
////////////////////////////////////////////////////////////////////////////////

std::string EmulatorShell::GetDebuggerClosedPanes()
{
    return m_globalPrefs.debuggerClosedPanes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerClosedPanes
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebuggerClosedPanes (const std::string & text)
{
    if (m_globalPrefs.debuggerClosedPanes == text)
    {
        return;
    }

    m_globalPrefs.debuggerClosedPanes = text;
    SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerCommandBarDock
//
////////////////////////////////////////////////////////////////////////////////

std::string EmulatorShell::GetDebuggerCommandBarDock()
{
    return m_globalPrefs.debuggerCommandBarDock;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerCommandBarDock
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebuggerCommandBarDock (const std::string & text)
{
    if (m_globalPrefs.debuggerCommandBarDock == text)
    {
        return;
    }

    m_globalPrefs.debuggerCommandBarDock = text;
    SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerTimelineDock
//
////////////////////////////////////////////////////////////////////////////////

std::string EmulatorShell::GetDebuggerTimelineDock()
{
    return m_globalPrefs.debuggerTimelineDock;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerTimelineDock
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebuggerTimelineDock (const std::string & text)
{
    if (m_globalPrefs.debuggerTimelineDock == text)
    {
        return;
    }

    m_globalPrefs.debuggerTimelineDock = text;
    SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerFocusedPane
//
////////////////////////////////////////////////////////////////////////////////

std::string EmulatorShell::GetDebuggerFocusedPane()
{
    return m_globalPrefs.debuggerFocusedPane;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerFocusedPane
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebuggerFocusedPane (const std::string & text)
{
    if (m_globalPrefs.debuggerFocusedPane == text)
    {
        return;
    }

    m_globalPrefs.debuggerFocusedPane = text;
    SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerTextZoomPercent
//
////////////////////////////////////////////////////////////////////////////////

int EmulatorShell::GetDebuggerTextZoomPercent()
{
    return m_globalPrefs.debuggerTextZoomPercent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerTextZoomPercent
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebuggerTextZoomPercent (int percent)
{
    if (m_globalPrefs.debuggerTextZoomPercent == percent)
    {
        return;
    }

    m_globalPrefs.debuggerTextZoomPercent = percent;
    SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerDisassemblyOptions
//
////////////////////////////////////////////////////////////////////////////////

std::string EmulatorShell::GetDebuggerDisassemblyOptions()
{
    return m_globalPrefs.debuggerDisassemblyOptions;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerDisassemblyOptions
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebuggerDisassemblyOptions (const std::string & text)
{
    if (m_globalPrefs.debuggerDisassemblyOptions == text)
    {
        return;
    }

    m_globalPrefs.debuggerDisassemblyOptions = text;
    SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerHeatMapOptions
//
////////////////////////////////////////////////////////////////////////////////

std::string EmulatorShell::GetDebuggerHeatMapOptions()
{
    return m_globalPrefs.debuggerHeatMapOptions;
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

void EmulatorShell::SetDebuggerHeatMapOptions (const std::string & text)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, "heatmap options " + text);

    if (m_globalPrefs.debuggerHeatMapOptions == text)
    {
        return;
    }

    m_globalPrefs.debuggerHeatMapOptions = text;
    SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResetDebuggerHeatMap
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ResetDebuggerHeatMap()
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, "heatmap reset");
}





////////////////////////////////////////////////////////////////////////////////
//
//  SendDebuggerHeatMapRequest
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SendDebuggerHeatMapRequest (const std::string & words)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, "heatmap " + words);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerHeatMapRanges
//
////////////////////////////////////////////////////////////////////////////////

std::string EmulatorShell::GetDebuggerHeatMapRanges()
{
    return m_globalPrefs.debuggerHeatMapRanges;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerHeatMapRanges
//
//  UI thread. The ranges are the window's alone, read against the symbols
//  each snapshot carries, so the CPU thread is told nothing.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebuggerHeatMapRanges (const std::string & text)
{
    if (m_globalPrefs.debuggerHeatMapRanges == text)
    {
        return;
    }

    m_globalPrefs.debuggerHeatMapRanges = text;
    SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDebuggerOpenViews
//
////////////////////////////////////////////////////////////////////////////////

std::string EmulatorShell::GetDebuggerOpenViews()
{
    return m_globalPrefs.debuggerOpenViews;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerOpenViews
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebuggerOpenViews (const std::string & text)
{
    if (m_globalPrefs.debuggerOpenViews == text)
    {
        return;
    }

    m_globalPrefs.debuggerOpenViews = text;
    SaveGlobalPrefsDeferred();
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

std::string EmulatorShell::GetDebuggerPlacementKey() const
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

bool EmulatorShell::TryGetDebuggerPlacement (RECT & rectPx)
{
    WindowPlacementProfile::Bounds  bounds;
    WindowPlacementProfile          profile (m_globalPrefs);
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

void EmulatorShell::SetDebuggerPlacement (const RECT & rectPx)
{
    WindowPlacementProfile          profile (m_globalPrefs);
    WindowPlacementProfile::Bounds  bounds;
    std::string                     key    = GetDebuggerPlacementKey();



    bounds.x = rectPx.left;
    bounds.y = rectPx.top;
    bounds.w = (int) (rectPx.right - rectPx.left);
    bounds.h = (int) (rectPx.bottom - rectPx.top);

    WindowTrace::LogRect ("save.prefs", "debugger", rectPx, "key=" + key);
    profile.Save (key, bounds, WindowPlacementProfile::Target::Debugger);
    SaveGlobalPrefsDeferred();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindDebuggerSource
//
//  A folder where a file was found goes into the preferences, so they are
//  saved; the next search, in this session or another, looks there first.
//
////////////////////////////////////////////////////////////////////////////////

SourceLookup EmulatorShell::FindDebuggerSource (const DebugSourceFile & record, const std::wstring & debugFilePath,
                                                const std::string & programKey)
{
    SourcePathList  paths   (m_globalPrefs);
    SourceService   service (m_uiFs, paths);
    SourceLookup    lookup  = service.Find (record, debugFilePath, programKey);



    if (lookup.match == SourceMatch::Exact || lookup.match == SourceMatch::Unverified)
    {
        SaveGlobalPrefsDeferred();
    }

    return lookup;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MatchDroppedDebuggerSource
//
////////////////////////////////////////////////////////////////////////////////

SourceLookup EmulatorShell::MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> & files, const std::wstring & path,
                                                        const std::string & programKey, int & recordIndex)
{
    SourcePathList  paths   (m_globalPrefs);
    SourceService   service (m_uiFs, paths);
    SourceLookup    lookup  = service.MatchDropped (files, path, programKey, recordIndex);



    if (recordIndex >= 0)
    {
        SaveGlobalPrefsDeferred();
    }

    return lookup;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunDebuggerCommand
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RunDebuggerCommand (const std::string & line)
{
    m_cpuManager.PostCommand (IDM_DEBUG_COMMAND, DebugCommandPayload::Encode (s_kWindowClientId, line));
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunDebuggerCommandInMode
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RunDebuggerCommandInMode (const std::string & line, CommandMode mode)
{
    m_cpuManager.PostCommand (IDM_DEBUG_COMMAND, DebugCommandPayload::Encode (s_kWindowClientId, line, mode));
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunDebuggerAction
//
//  The action waits on the shell's queue, since a command crosses to the CPU
//  thread as text and an action must not.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RunDebuggerAction (const DebuggerAction & action)
{
    {
        std::lock_guard<std::mutex>  held (m_debugViewMutex);

        m_debugActionsPending.push_back (action);
    }

    m_cpuManager.PostCommand (IDM_DEBUG_ACTION);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PauseDebugger
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PauseDebugger()
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

void EmulatorShell::RunEmulatorCommand (int commandId)
{
    PostMessageW (m_hwnd, WM_COMMAND, MAKEWPARAM (commandId, 0), 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCodeViewSuffix
//
//  Nothing for the first code view, 2 to 4 for the others, as the CPU
//  thread's view names read.
//
////////////////////////////////////////////////////////////////////////////////

std::string EmulatorShell::GetCodeViewSuffix (int view)
{
    return (view <= 0) ? std::string() : std::to_string (view + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerFollowView / CloseDebuggerCodeView
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebuggerFollowView (int view)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, std::format ("follow {}", view + 1));
}


void EmulatorShell::CloseDebuggerCodeView (int view)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, std::format ("codeclose {}", view + 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerCodeAddress
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebuggerCodeLines (int lines, int view)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, std::format ("lines{} {:04X}", GetCodeViewSuffix (view), (Word) lines));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerCodeAddress
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebuggerCodeAddress (std::optional<Word> address, int view)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, address.has_value() ? std::format ("code{} {:04X}", GetCodeViewSuffix (view), *address)
                                                                 : std::format ("code{} pc", GetCodeViewSuffix (view)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerCodeTop
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebuggerCodeTop (Word top, int view)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, std::format ("codetop{} {:04X}", GetCodeViewSuffix (view), top));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerMemoryWindow
//
//  Window 1 is the "memory" view; 2 to 4 are "memory2" and on, and no
//  address closes one.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebuggerMemoryWindow (int id, std::optional<Word> address)
{
    std::string  view = (id == 1) ? std::string ("memory") : std::format ("memory{}", id);



    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, address.has_value() ? std::format ("{} {:04X}", view, *address)
                                                                 : view + " close");
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerTraceTop
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebuggerTraceTop (std::optional<uint64_t> first)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, first.has_value() ? std::format ("trace {}", *first)
                                                               : std::string ("trace end"));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebuggerHeatMapShown
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebuggerHeatMapShown (bool shown)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, shown ? std::string ("heatmap on") : std::string ("heatmap off"));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetBeamOverlayOn
//
//  UI thread. The CPU thread is asked to draw the picture again, since a
//  stopped machine would otherwise show the change only at its next step.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetBeamOverlayOn (bool on)
{
    m_isBeamOverlayOn.store (on, memory_order_release);
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, "beam");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScrollDebuggerCode
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ScrollDebuggerCode (int lines, int view)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, std::format ("codescroll{} {}", GetCodeViewSuffix (view), lines));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GoToDebuggerMemory
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::GoToDebuggerMemory (int window, const std::string & text)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, std::format ("goto {} {}", window, text));
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeDebuggerUpdate
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> & snapshot,
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

void EmulatorShell::OpenDebugChannel()
{
    HRESULT  hr = OpenDebugger();



    IGNORE_RETURN_VALUE (hr, S_OK);

    if (m_debugger == nullptr)
    {
        return;
    }

    m_debugger->GetSession().SetAttached (true);

    SetDebugCommandHandler ([this] (uint32_t clientId, const std::string & line, std::optional<CommandMode> mode)
    {
        std::vector<std::string>  lines;



        if (clientId != s_kWindowClientId || m_debugger == nullptr)
        {
            return;
        }

        {
            std::lock_guard<std::mutex>  viewHeld (m_debugViewStateLock);



            lines = m_debugViewState.ExecuteConsoleLine (m_debugger->GetSession(), line, mode);
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

void EmulatorShell::CloseDebugChannel (bool isDetach)
{
    if (m_debugger == nullptr)
    {
        return;
    }

    if (isDetach)
    {
        if (m_debugRunDriver != nullptr)
        {
            m_debugRunDriver->EndForUserPause();
        }

        m_debugger->GetSession().SetAttached (false);
        m_cpuManager.SetPaused (false);
        m_debugger->GetSession().OnUserResumed();
    }

    m_debugger->Close();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PauseDebugRun
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PauseDebugRun()
{
    if (m_debugger != nullptr)
    {
        m_debugger->RequestPause();
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

void EmulatorShell::RunDebugActions()
{
    std::vector<DebuggerAction>   actions;
    std::vector<std::string>      lines;
    std::unique_lock<std::mutex>  viewHeld (m_debugViewStateLock, std::defer_lock);



    {
        std::lock_guard<std::mutex>  held (m_debugViewMutex);

        actions.swap (m_debugActionsPending);
    }

    if (m_debugger == nullptr)
    {
        return;
    }

    viewHeld.lock();

    for (const DebuggerAction & action : actions)
    {
        std::vector<std::string>  shown = m_debugViewState.ExecuteAction (m_debugger->GetSession(), action);

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

void EmulatorShell::SetDebugView (const std::string & view, std::optional<Word> address)
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

void EmulatorShell::ScrollDebugCode (int lines, int view)
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

void EmulatorShell::GoToDebugMemory (int window, const std::string & text)
{
    std::lock_guard<std::mutex>  viewHeld (m_debugViewStateLock);



    if (m_debugger != nullptr)
    {
        m_debugViewState.RequestGoTo (m_debugger->GetSession(), window, text);
        m_isDebugViewDirty = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebugTraceView
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebugTraceView (std::optional<uint64_t> first)
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

void EmulatorShell::SetDebugHeatMapShown (bool shown)
{
    m_debugViewState.SetHeatMapShown (shown);
    m_isDebugViewDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDebugHeatMapOptions
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebugHeatMapOptions (const std::string & text)
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

void EmulatorShell::ResetDebugHeatMap()
{
    if (m_debugger != nullptr)
    {
        m_debugger->GetSession().GetTarget().ResetHeatMap();
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

void EmulatorShell::SetDebugHeatMapIgnore (const std::vector<std::pair<Word, Word>> & spans)
{
    if (m_debugger != nullptr)
    {
        m_debugger->GetSession().GetTarget().SetUnwrittenIgnore (spans);
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

void EmulatorShell::SetDebugHeatMapHover (std::optional<Word> address)
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

void EmulatorShell::RunDebugHeatMapAccess (const HeatAccessRequest & request)
{
    HeatLastAccess   access;
    HeatAccessState  state       = HeatAccessState::None;
    HeatAccessPlan   plan;
    bool             isRecording = m_reverseHost != nullptr && m_reverseHost->IsRecording();
    uint64_t         oldest      = 0;



    if (m_debugger == nullptr)
    {
        return;
    }

    if (m_debugger->GetSession().GetTarget().FoldHeatMap() != nullptr)
    {
        state = m_debugger->GetSession().GetTarget().LookUpHeatMapAccess (HeatMapOptions::GetSpace (request.bank), request.isWrite, request.address, access);
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
        if (!m_cpuManager.IsPaused())
        {
            m_cpuManager.SetPaused (true);
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

void EmulatorShell::RedrawDebugFrame()
{
    uint64_t  colorSig = ComputeColorSig();



    if (colorSig == m_lastRenderColorSig)
    {
        return;
    }

    RenderFramebuffer();
    PublishFramebuffer();

    m_lastRenderColorSig = colorSig;
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

void EmulatorShell::PublishDebuggerView()
{
    HRESULT               hr         = S_OK;
    ULONGLONG             now        = GetTickCount64();
    bool                  isDue      = false;
    bool                  isBreaking = false;
    bool                  isTaken    = true;
    DebugViewInput        input;
    DebuggerViewSnapshot  live;



    if (m_debugger == nullptr)
    {
        return;
    }

    //  The heat map records for its pane, which no one sees while the window
    //  is closed, and for BRKUNINIT, which needs it whatever is shown; then it
    //  is folded now and then, so its counts never pile up unfolded.
    if (!m_isDebugWindowShown.load())
    {
        isBreaking = m_debugger->GetSession().HasUnwrittenBreak();

        m_debugger->GetSession().GetTarget().SetHeatMapOn (isBreaking);

        if (isBreaking && now - m_heatFoldedAt >= kHiddenHeatFoldMs)
        {
            (void) m_debugger->GetSession().GetTarget().FoldHeatMap();
            m_heatFoldedAt = now;
        }

        return;
    }

    {
        std::lock_guard<std::mutex>  held (m_debugViewMutex);

        isTaken = !m_isDebugViewFresh;
    }

    isDue = DebuggerViewState::IsBuildDue (m_isDebugViewDirty, m_cpuManager.IsPaused(), m_wasPausedAtDebugBuild,
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
    m_machine.SetSpeedMode (m_cpuManager.GetSpeedMode());

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
    m_wasPausedAtDebugBuild = m_cpuManager.IsPaused();
    m_wasHeatRebuilding     = m_debugger->GetSession().GetTarget().IsHeatMapRebuilding();
    m_wasCallRebuilding     = m_debugger->GetCallHistory().IsRebuilding();
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

void EmulatorShell::GatherDebugView (DebugViewInput & input, DebuggerViewSnapshot & live)
{
    DebugSession              & session  = m_debugger->GetSession();
    IDebugTarget              & target   = session.GetTarget();
    bool                        isPaused = m_cpuManager.IsPaused();
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

void EmulatorShell::PublishDebugSnapshot (std::shared_ptr<const DebuggerViewSnapshot> snapshot)
{
    std::lock_guard<std::mutex>  held (m_debugViewMutex);



    m_debugViewSnapshot = std::move (snapshot);
    m_isDebugViewFresh  = true;
}
