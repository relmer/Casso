#include "Pch.h"

#include "Shell/ShellDebugger.h"

#include "Config/DebuggerSettings.h"
#include "Core/PathResolver.h"
#include "Core/TextEncoding.h"
#include "Debugger/DebugCommandPayload.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/DebugSession.h"
#include "Shell/CpuManager.h"
#include "Shell/EmulatorShellInternal.h"
#include "Shell/IDebuggerHost.h"
#include "Shell/MachineHost.h"
#include "resource.h"





static constexpr const wchar_t  * s_kpszProjectFolder = L"DebugProjects";

//  The debugger window's client id, as ShellDebugger.cpp has it.
static constexpr uint32_t         s_kWindowClientId   = 0;

//  The Disk II controller's slot, as the machine host has it.
static constexpr int              s_kDiskSlot         = 6;

//  How often the open project is compared with what the debugger has now,
//  and written when they differ.
static constexpr ULONGLONG        s_kProjectCheckMs   = DebugProjectStore::kSaveDelayMs;

//  The lines of a project's script that clear the old state ahead of its own,
//  left out when its items are added to another project's.
static constexpr const char     * s_kClearLines[]     = { "BPC *", "WC *", "ZPC *", "BMC *", "SKIP CLEAR" };





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::OpenProjectStore
//
//  CPU thread, as the debug channel opens. The store's folder is
//  %LOCALAPPDATA%\Casso\DebugProjects, made if it is not there. A debugger
//  attached without the channel, as a test attaches one, has no projects
//  unless a test calls SetProjectStore.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::OpenProjectStore()
{
    fs::path         folder = PathResolver::GetLocalAppDataDir (L"Casso");
    std::error_code  error;



    if (m_projectStore != nullptr || folder.empty())
    {
        return;
    }

    folder /= s_kpszProjectFolder;
    fs::create_directories (folder, error);

    m_projectStore = std::make_unique<DebugProjectStore> (m_debugFiles, folder.native());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::GetProjectScope
//
//  The debug file, else the binary BLOAD last loaded, else the machine with
//  the disk drive 1 had at its last power cycle, else the machine. The
//  power-on at startup is not reported as a power cycle, and the channel
//  can open before the disks are mounted, so until a power cycle the disk
//  in drive 1 now is the one the machine booted.
//
////////////////////////////////////////////////////////////////////////////////

ProjectScope ShellDebugger::GetProjectScope()
{
    DebugSession  & session   = m_debugController->GetSession();
    std::string     debugName = TextEncoding::WideToNarrow (fs::path (session.GetDebugFilePath()).filename().native());
    std::wstring    bootDisk  = m_isBootDiskNoted ? m_bootDisk : TextEncoding::NarrowToWide (m_machine.GetDiskStore().GetSourcePath (s_kDiskSlot, 0));



    return ProjectScope::Choose (session.GetDebugFileKey(), debugName, session.GetBinaryKey(), session.GetBinaryName(),
                                 m_machine.GetConfig().name, bootDisk);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::NoteBootDisk
//
//  CPU thread, at each power cycle: the disk in drive 1 then, the machine's
//  signal for its project.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::NoteBootDisk()
{
    m_bootDisk        = TextEncoding::NarrowToWide (m_machine.GetDiskStore().GetSourcePath (s_kDiskSlot, 0));
    m_isBootDiskNoted = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::ServiceProject
//
//  CPU thread, each service tick while the debugger is open, outside any
//  command: a project switch the signals call for, whichever client's
//  command changed them, a PROJECT OPEN or MERGE waiting, and a save when
//  the project differs from its file.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::ServiceProject()
{
    ULONGLONG  now = GetTickCount64();



    SyncProject();

    if (m_projectToOpen.has_value())
    {
        DebugProject  opened = std::move (*m_projectToOpen);



        m_projectToOpen.reset();
        SaveProject();
        opened.scope = m_project.scope;
        ApplyProject (opened);
    }

    if (m_isMergeOwed)
    {
        m_isMergeOwed = false;
        RunMergeLines();
    }

    {
        std::lock_guard<std::mutex>  lock (m_projectMutex);



        if (m_startupPending.has_value())
        {
            m_project.startupCommands = std::move (*m_startupPending);
            m_startupPending.reset();
            m_projectCopy = m_project;
        }
    }

    if (m_hasProject && now - m_projectCheckedAt >= s_kProjectCheckMs)
    {
        m_projectCheckedAt = now;
        SaveProject();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::SyncProject
//
//  When the scope the signals choose is not the open project's, the open
//  one is saved and the scope's opens. A scope with no project yet starts
//  from what the debugger and its window have now. An existing project for
//  a more specific scope replaces the debugger's items with its own, and
//  the user is offered the old project's items too (FR-169).
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SyncProject()
{
    ProjectScope              scope;
    DebugProject              old;
    DebugProject              opened;
    std::vector<std::string>  notices;
    bool                      isNew   = false;
    bool                      isOffer = false;
    HRESULT                   hr      = S_OK;



    if (m_projectStore == nullptr || m_debugController == nullptr)
    {
        return;
    }

    scope = GetProjectScope();

    if (m_hasProject && scope == m_project.scope)
    {
        return;
    }

    if (m_hasProject)
    {
        SaveProject();
        old     = m_project;
        isOffer = scope.IsMoreSpecificThan (old.scope);
    }

    hr = m_projectStore->Open (scope, opened, notices);
    IGNORE_RETURN_VALUE (hr, S_OK);

    AddConsoleLines (notices);

    isNew = opened.engineScript.empty() && opened.symbolFiles.empty() && opened.layout.empty() && opened.startupCommands.empty();

    if (isNew)
    {
        CaptureProject (opened);

        m_project    = opened;
        m_hasProject = true;
        m_projectSaved.clear();
        PublishProject();
        return;
    }

    ApplyProject (opened);

    if (isOffer)
    {
        OfferProjectMerge (old);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::ApplyProject
//
//  The project's items into the session, its window strings passed to the
//  UI thread, then its startup commands run in order, each in its mode.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::ApplyProject (const DebugProject & project)
{
    std::vector<std::string>  failures;
    std::vector<std::string>  lines;



    {
        std::lock_guard<std::mutex>  viewLock (m_debugViewStateLock);



        project.ApplyEngine (m_debugController->GetSession(), failures);
    }

    {
        std::lock_guard<std::mutex>  lock (m_projectMutex);



        m_projectWindowPending = project;
    }

    m_project      = project;
    m_hasProject   = true;
    m_projectSaved = project.ToJson (fs::path (project.filePath).parent_path().native());
    PublishProject();

    lines.push_back (std::format ("Debug project for {}: {}", project.scope.label, TextEncoding::WideToNarrow (project.filePath)));

    for (const std::string & failure : failures)
    {
        lines.push_back ("Not restored: " + failure);
    }

    AddConsoleLines (lines);
    RunStartupCommands();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::RunStartupCommands
//
//  Each through the console, so its lines appear as if typed.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::RunStartupCommands()
{
    std::vector<std::string>  lines;



    for (const DebugProject::StartupCommand & command : m_project.startupCommands)
    {
        std::vector<std::string>  echoed;



        {
            std::lock_guard<std::mutex>  viewLock (m_debugViewStateLock);



            echoed = m_debugViewState.ExecuteConsoleLine (m_debugController->GetSession(), command.text, command.mode);
        }

        DebuggerViewState::AddCommandGap (echoed);
        lines.insert (lines.end(), echoed.begin(), echoed.end());
    }

    AddConsoleLines (lines);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::OfferProjectMerge
//
//  The lines that add the old project's items, without the ones that
//  clear, saved for PROJECT MERGE; the prompt is posted to the UI thread.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::OfferProjectMerge (const DebugProject & old)
{
    std::vector<std::string>  lines;



    for (const std::string & line : old.engineScript)
    {
        if (std::ranges::find (s_kClearLines, line) == std::end (s_kClearLines))
        {
            lines.push_back (line);
        }
    }

    if (lines.empty())
    {
        return;
    }

    {
        std::lock_guard<std::mutex>  lock (m_projectMutex);



        m_mergeLines = std::move (lines);
        m_mergeFrom  = old.scope.label;
        m_mergeInto  = m_project.scope.label;
    }

    if (m_host.GetMainWindow() != nullptr)
    {
        PostMessageW (m_host.GetMainWindow(), WM_APP_CONFIRM_PROJECT_MERGE, 0, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::OnConfirmProjectMerge
//
//  UI thread: the prompt, and on Yes, PROJECT MERGE queued for the CPU thread.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::OnConfirmProjectMerge()
{
    std::wstring  question;
    HWND          owner    = m_host.GetMainWindow();
    int           choice   = IDNO;



    {
        std::lock_guard<std::mutex>  lock (m_projectMutex);



        if (m_mergeLines.empty())
        {
            return;
        }

        question = TextEncoding::NarrowToWide (std::format ("The debug project for {} is open. Add the breakpoints, watches "
                                                            "and bookmarks set for {} to it?", m_mergeInto, m_mergeFrom));
    }

    if (m_debuggerWindow != nullptr && m_debuggerWindow->GetHwnd() != nullptr)
    {
        owner = m_debuggerWindow->GetHwnd();
    }

    choice = DxuiMessageBox (owner, &m_host.GetChromeTheme(), question.c_str(), L"Debug project", MB_YESNO | MB_ICONQUESTION);

    if (choice == IDYES)
    {
        m_cpuManager.PostCommand (IDM_DEBUG_COMMAND, DebugCommandPayload::Encode (s_kWindowClientId, "PROJECT MERGE", CommandMode::AppleWin));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::RunMergeLines
//
//  CPU thread: the old project's items added to the open one's.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::RunMergeLines()
{
    std::vector<std::string>  merge;
    std::vector<std::string>  lines;



    {
        std::lock_guard<std::mutex>  lock (m_projectMutex);



        merge.swap (m_mergeLines);
    }

    {
        std::lock_guard<std::mutex>  viewLock (m_debugViewStateLock);



        for (const std::string & line : merge)
        {
            Reply  reply = m_debugController->GetSession().ExecuteLine (line, CommandMode::AppleWin);



            if (reply.status != CommandStatus::Ok)
            {
                lines.push_back ("Not added: " + line);
            }
        }
    }

    AddConsoleLines (lines);
    m_isDebugViewDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::RunProjectRequest
//
//  PROJECT, PROJECT SAVEAS file, PROJECT OPEN file and PROJECT MERGE, run
//  inside the command. An OPEN or a MERGE changes the session, so it is
//  made on the next service tick.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::RunProjectRequest (const std::string & text, Reply & reply)
{
    std::string                 verb;
    std::string                 path;
    size_t                      split   = text.find_first_of (" \t");
    std::vector<std::string>    notices;
    DebugProject                opened;
    const DebugProject        * reported = &m_project;
    HRESULT                     hr       = S_OK;



    verb = text.substr (0, split);
    path = (split == std::string::npos) ? std::string() : text.substr (split + 1);
    path.erase (0, std::min (path.size(), path.find_first_not_of (" \t")));
    path.erase (path.find_last_not_of (" \t") + 1);

    if (path.size() >= 2 && path.front() == '"' && path.back() == '"')
    {
        path = path.substr (1, path.size() - 2);
    }

    std::ranges::transform (verb, verb.begin(), [] (char ch) { return (char) std::toupper ((unsigned char) ch); });

    if (!m_hasProject)
    {
        reply.SetError (CommandStatus::Error, "no debug projects", "The folder for debug projects could not be made.");
        return;
    }

    if (verb == "SAVEAS" && !path.empty())
    {
        CaptureProject (m_project);
        hr = m_projectStore->SaveAs (m_project, m_debugController->GetSession().ResolvePath (path));

        if (FAILED (hr))
        {
            reply.SetError (CommandStatus::Error, "file not written", std::format ("{} could not be written.", path));
            return;
        }

        m_projectSaved = m_project.ToJson (fs::path (m_project.filePath).parent_path().native());
        PublishProject();
    }
    else if (verb == "OPEN" && !path.empty())
    {
        hr = m_projectStore->OpenFile (m_debugController->GetSession().ResolvePath (path), opened, notices);

        if (FAILED (hr) || !notices.empty())
        {
            reply.SetError (CommandStatus::Error, "not a debug project", notices.empty() ? std::format ("{} could not be read.", path) : notices.front());
            return;
        }

        opened.scope    = m_project.scope;
        m_projectToOpen = opened;
        reported        = &*m_projectToOpen;
    }
    else if (verb == "MERGE" && path.empty())
    {
        m_isMergeOwed = true;
    }
    else if (!verb.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "PROJECT takes SAVEAS file, OPEN file or MERGE, or nothing.");
        return;
    }

    reply.data = ProjectData { ProjectScope::GetKindName (reported->scope.kind), reported->scope.key, reported->scope.label,
                               TextEncoding::WideToNarrow (reported->filePath), reported->isAutomatic };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::CaptureProject
//
//  The session's items and the window's strings, as they are now. Strings a
//  switch passed to the UI thread, not yet applied to the window, are the
//  project's.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::CaptureProject (DebugProject & project)
{
    std::lock_guard<std::mutex>  lock (m_projectMutex);



    project.CaptureEngine (m_debugController->GetSession(), &m_debugController->GetHandlers().GetExecution());

    if (m_projectWindowPending.has_value())
    {
        project.layout             = m_projectWindowPending->layout;
        project.rangeSets          = m_projectWindowPending->rangeSets;
        project.disassemblyOptions = m_projectWindowPending->disassemblyOptions;
        project.openViews          = m_projectWindowPending->openViews;
        project.windowStrings      = m_projectWindowPending->windowStrings;
        return;
    }

    project.layout             = m_settings.layout;
    project.rangeSets          = m_settings.heatMapRanges;
    project.disassemblyOptions = m_settings.disassemblyOptions;
    project.openViews          = m_settings.openViews;
    project.windowStrings      = GetProjectWindowStrings();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::GetProjectWindowStrings
//
//  The window's settings a project saves beyond its four, by name; the caller
//  has the project lock.
//
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, std::string> ShellDebugger::GetProjectWindowStrings() const
{
    std::map<std::string, std::string>  strings = m_settings.windowState;



    strings["closedPanes"]     = m_settings.closedPanes;
    strings["focusedPane"]     = m_settings.focusedPane;
    strings["commandBarDock"]  = m_settings.commandBarDock;
    strings["timelineDock"]    = m_settings.timelineDock;
    strings["heatMapOptions"]  = m_settings.heatMapOptions;
    strings["diskLogOptions"]  = m_settings.diskLogOptions;
    strings["inputLogOptions"] = m_settings.inputLogOptions;
    return strings;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::SetProjectWindowStrings
//
//  As the four, a setting is replaced only by a string the project has; the
//  caller has the project lock.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetProjectWindowStrings (const std::map<std::string, std::string> & strings)
{
    std::pair<const char *, std::string *>  settings[] =
    {
        { "closedPanes",     &m_settings.closedPanes     },
        { "focusedPane",     &m_settings.focusedPane     },
        { "commandBarDock",  &m_settings.commandBarDock  },
        { "timelineDock",    &m_settings.timelineDock    },
        { "heatMapOptions",  &m_settings.heatMapOptions  },
        { "diskLogOptions",  &m_settings.diskLogOptions  },
        { "inputLogOptions", &m_settings.inputLogOptions },
    };



    for (const auto & [name, text] : strings)
    {
        auto  setting = std::ranges::find_if (settings, [&name] (const auto & each) { return name == each.first; });

        if (text.empty())
        {
            continue;
        }

        if (setting != std::end (settings))
        {
            *setting->second = text;
        }
        else
        {
            m_settings.windowState[name] = text;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::SaveProject
//
//  CPU thread. Written only when the file's text would change.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SaveProject()
{
    std::string  text;
    HRESULT      hr   = S_OK;



    if (!m_hasProject || m_debugController == nullptr || m_projectStore == nullptr)
    {
        return;
    }

    CaptureProject (m_project);
    text = m_project.ToJson (fs::path (m_project.filePath).parent_path().native());

    if (text == m_projectSaved)
    {
        return;
    }

    hr = m_projectStore->Save (m_project);

    if (SUCCEEDED (hr))
    {
        m_projectSaved = text;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::TakeDebuggerProjectState
//
//  UI thread: the window strings of a project just opened, made the
//  window's settings; false when no project opened since the last call.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDebugger::TakeDebuggerProjectState()
{
    std::optional<DebugProject>  pending;



    {
        std::lock_guard<std::mutex>  lock (m_projectMutex);



        pending.swap (m_projectWindowPending);

        if (!pending.has_value())
        {
            return false;
        }

        //  The window's string is replaced only by a string the project has.
        m_settings.layout             = pending->layout.empty()             ? m_settings.layout             : pending->layout;
        m_settings.heatMapRanges      = pending->rangeSets.empty()          ? m_settings.heatMapRanges      : pending->rangeSets;
        m_settings.disassemblyOptions = pending->disassemblyOptions.empty() ? m_settings.disassemblyOptions : pending->disassemblyOptions;
        m_settings.openViews          = pending->openViews.empty()          ? m_settings.openViews          : pending->openViews;

        SetProjectWindowStrings (pending->windowStrings);
    }

    m_host.SaveSettings();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::PublishProject
//
//  CPU thread: a copy of the open project for the window's dialogs.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::PublishProject()
{
    std::lock_guard<std::mutex>  lock (m_projectMutex);



    m_projectCopy = m_project;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::TryGetDebuggerProject
//
//  UI thread.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDebugger::TryGetDebuggerProject (DebugProject & project)
{
    std::lock_guard<std::mutex>  lock (m_projectMutex);



    if (!m_projectCopy.has_value())
    {
        return false;
    }

    project = *m_projectCopy;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugger::SetDebuggerStartupCommands
//
//  UI thread: the CPU thread takes them on its next service tick, and the
//  project is saved with them.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugger::SetDebuggerStartupCommands (const std::vector<DebugProject::StartupCommand> & commands)
{
    std::lock_guard<std::mutex>  lock (m_projectMutex);



    m_startupPending = commands;
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

    {
        std::lock_guard<std::mutex>  lock (m_debugViewMutex);



        m_debugConsolePending.insert (m_debugConsolePending.end(), lines.begin(), lines.end());
    }

    m_isDebugViewDirty = true;
}
