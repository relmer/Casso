#include "Pch.h"

#include "Shell/EmulatorShell.h"

#include "Config/WindowPlacementProfile.h"
#include "Core/WindowTrace.h"
#include "Debugger/DebugCommandPayload.h"
#include "Debugger/DebuggerController.h"
#include "resource.h"





//  The client id the window's own commands carry. Channel clients are numbered
//  by the transport from 1, so 0 is never one of theirs.
static constexpr uint32_t  s_kWindowClientId = 0;





////////////////////////////////////////////////////////////////////////////////
//
//  OpenDebuggerWindow
//
//  Creates the window the first time and shows it every time. The channel is
//  opened on the CPU thread, where the controller lives, so a window opened
//  while the machine runs does not wait on it.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OpenDebuggerWindow()
{
    HRESULT  hr = S_OK;



    if (m_debuggerWindow == nullptr || m_debuggerWindow->GetHwnd() == nullptr)
    {
        m_debuggerWindow = std::make_unique<DebuggerWindow>();

        hr = m_debuggerWindow->Create (m_hInstance, m_hwnd, &m_chromeTheme, this);
        CHRF (hr, m_debuggerWindow.reset());

        ApplyAppIconToWindow (m_debuggerWindow->GetHwnd());
    }

    m_debuggerWindow->Show();
    SetForegroundWindow (m_debuggerWindow->GetHwnd());

    m_isDebugWindowShown.store (true);
    m_cpuManager.PostCommand (IDM_DEBUG_OPEN);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDebuggerWindowClosed
//
//  Closing the window closes the debugger: the channel closes and every client
//  is told so. The session survives, so reopening finds the breakpoints as
//  they were left.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OnDebuggerWindowClosed()
{
    m_isDebugWindowShown.store (false);
    m_cpuManager.PostCommand (IDM_DEBUG_CLOSE);
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
//  PauseDebugger
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PauseDebugger()
{
    m_cpuManager.PostCommand (IDM_DEBUG_PAUSE);
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

    SetDebugCommandHandler ([this] (uint32_t clientId, const std::string & line)
    {
        Reply                     reply;
        std::vector<std::string>  lines;



        if (clientId != s_kWindowClientId || m_debugger == nullptr)
        {
            return;
        }

        reply = m_debugViewState.ExecuteWindowLine (m_debugger->GetSession(), line,
                                                    m_debugger->GetSession().GetMode());

        lines.push_back (DebugSession::GetPrompt (m_debugger->GetSession().GetMode()) + line);
        lines.insert (lines.end(), reply.text.begin(), reply.text.end());

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
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::CloseDebugChannel()
{
    if (m_debugger != nullptr)
    {
        m_debugger->Close();
    }
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
//  SetDebugView
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebugView (const std::string & view, std::optional<Word> address)
{
    int  index = 0;



    if (view == "follow" && address.has_value())
    {
        m_debugViewState.SetFollowView ((int) *address);
    }
    else if (view == "codeclose" && address.has_value())
    {
        m_debugViewState.CloseCodeView ((int) *address);
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
    m_debugViewState.SetTraceTop (first);
    m_isDebugViewDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PublishDebuggerView
//
//  Once a frame while the machine runs, and at once after anything the window
//  did; DebuggerViewState::IsBuildDue says which.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PublishDebuggerView()
{
    ULONGLONG                                     now      = GetTickCount64();
    bool                                          isDue    = false;
    std::shared_ptr<const DebuggerViewSnapshot>   snapshot;



    if (m_debugger == nullptr || !m_isDebugWindowShown.load())
    {
        return;
    }

    isDue = DebuggerViewState::IsBuildDue (m_isDebugViewDirty, m_cpuManager.IsPaused(), m_wasPausedAtDebugBuild,
                                           now, m_debugViewBuiltAt);

    if (!isDue)
    {
        return;
    }

    //  The clock panel reports the speed, which the CPU manager paces and the
    //  machine does not know.
    m_machine.SetSpeedMode (m_cpuManager.GetSpeedMode());

    //  The run state is the CPU manager's, not the session's, so it is handed
    //  in: the command bar gates its stepping entries on it, and the code
    //  pane's annotations are built only when it is paused (FR-110).
    DebuggerViewSnapshot  built = m_debugViewState.Build (m_debugger->GetSession(), m_cpuManager.IsPaused());
    snapshot       = std::make_shared<const DebuggerViewSnapshot> (std::move (built));

    {
        std::lock_guard<std::mutex>  held (m_debugViewMutex);

        m_debugViewSnapshot = std::move (snapshot);
        m_isDebugViewFresh  = true;
    }

    m_debugViewBuiltAt      = now;
    m_wasPausedAtDebugBuild = m_cpuManager.IsPaused();
    m_isDebugViewDirty = false;
}
