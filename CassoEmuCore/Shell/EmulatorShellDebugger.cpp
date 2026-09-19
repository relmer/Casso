#include "Pch.h"

#include "Shell/EmulatorShell.h"

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
//  SetDebuggerCodeAddress
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetDebuggerCodeAddress (std::optional<Word> address)
{
    m_cpuManager.PostCommand (IDM_DEBUG_VIEW, address.has_value() ? std::format ("code {:04X}", *address)
                                                                 : std::string ("code pc"));
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
    if (view == "code")
    {
        m_debugViewState.SetCodeAddress (address);
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

    snapshot = std::make_shared<const DebuggerViewSnapshot> (m_debugViewState.Build (m_debugger->GetSession()));

    {
        std::lock_guard<std::mutex>  held (m_debugViewMutex);

        m_debugViewSnapshot = std::move (snapshot);
        m_isDebugViewFresh  = true;
    }

    m_debugViewBuiltAt      = now;
    m_wasPausedAtDebugBuild = m_cpuManager.IsPaused();
    m_isDebugViewDirty = false;
}
