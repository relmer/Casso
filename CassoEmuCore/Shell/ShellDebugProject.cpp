#include "Pch.h"

#include "Shell/ShellDebugProject.h"
#include "Config/DebuggerSettings.h"
#include "Config/Win32FileSystem.h"
#include "Core/PathResolver.h"
#include "Core/TextEncoding.h"
#include "Debugger/Channel/HostAccessPolicy.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/DebugSession.h"
#include "Shell/DebugViewModel.h"
#include "Shell/IDebuggerHost.h"
#include "Shell/IShellDebugContext.h"
#include "Shell/MachineHost.h"
#include "Ui/Debugger/ConsoleLinks.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject::ShellDebugProject
//
////////////////////////////////////////////////////////////////////////////////

ShellDebugProject::ShellDebugProject (
    IDebuggerHost        & host,
    MachineHost          & machine,
    DebuggerSettings     & settings,
    Win32FileSystem      & debugFiles,
    IShellDebugContext   & context,
    DebugViewModel       & viewModel) :
    m_host       (host),
    m_machine    (machine),
    m_settings   (settings),
    m_debugFiles (debugFiles),
    m_context    (context),
    m_viewModel  (viewModel),
    m_merge      (host, context, viewModel)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject::Close
//
//  The project is saved and closed; the next debugger opens it again.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugProject::Close()
{
    Save();
    m_hasProject = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject::OpenStore
//
//  CPU thread, as the debug channel opens. The store's folder is
//  %LOCALAPPDATA%\Casso\DebugProjects, made if it is not there. A debugger
//  attached without the channel, as a test attaches one, has no projects
//  unless a test calls SetProjectStore.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugProject::OpenStore()
{
    constexpr const wchar_t  * kpszFolder = L"DebugProjects";
    fs::path                   folder     = PathResolver::GetLocalAppDataDir (L"Casso");
    std::error_code            error;



    if (m_projectStore != nullptr || folder.empty())
    {
        return;
    }

    folder /= kpszFolder;
    fs::create_directories (folder, error);

    m_projectStore = std::make_unique<DebugProjectStore> (m_debugFiles, folder.native());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject::GetScope
//
//  The debug file, else the binary BLOAD last loaded, else the machine with
//  the disk drive 1 had at its last power cycle, else the machine. The
//  power-on at startup is not reported as a power cycle, and the channel
//  can open before the disks are mounted, so until a power cycle the disk
//  in drive 1 now is the one the machine booted.
//
////////////////////////////////////////////////////////////////////////////////

ProjectScope ShellDebugProject::GetScope()
{
    DebugSession  & session   = m_context.GetDebugController()->GetSession();
    std::string     debugName = TextEncoding::WideToNarrow (fs::path (session.GetDebugFilePath()).filename().native());



    return ProjectScope::Choose (session.GetDebugFileKey(), debugName, session.GetBinaryKey(), session.GetBinaryName(),
                                 m_machine.GetConfig().name, GetBootDisk());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject::GetBootDisk
//
//  The disk drive 1 had at the last power cycle, or, before the first one,
//  the disk in it now.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ShellDebugProject::GetBootDisk()
{
    return m_isBootDiskNoted ? m_bootDisk : TextEncoding::NarrowToWide (m_machine.GetDiskStore().GetSourcePath (kDiskSlot, 0));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject::GetScopeFilePath
//
//  The file a scope is for: the debug file, the binary or the boot disk;
//  empty for the machine alone.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ShellDebugProject::GetScopeFilePath (const ProjectScope & scope)
{
    DebugSession  & session = m_context.GetDebugController()->GetSession();



    switch (scope.kind)
    {
    case ProjectScopeKind::DebugFile:   return session.GetDebugFilePath();
    case ProjectScopeKind::Binary:      return session.GetBinaryPath();
    case ProjectScopeKind::MachineBoot: return GetBootDisk();
    default:                            return std::wstring();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject::GetProjectLine
//
////////////////////////////////////////////////////////////////////////////////

std::string ShellDebugProject::GetProjectLine (const ProjectScope & scope, const std::wstring & projectPath, const std::wstring & targetPath)
{
    return std::format ("Loaded the debug {} for {}",
                        ConsoleLinks::MakeLink ("project", TextEncoding::WideToNarrow (projectPath)),
                        ConsoleLinks::LinkFileName (scope.label, TextEncoding::WideToNarrow (targetPath)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject::NoteBootDisk
//
//  CPU thread, at each power cycle: the disk in drive 1 then, the machine's
//  signal for its project. A debug file or binary outranks the boot disk in
//  the scope, so one loaded while another disk was booted is unloaded, and
//  the next sync switches to the new disk's project.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugProject::NoteBootDisk()
{
    m_bootDisk        = TextEncoding::NarrowToWide (m_machine.GetDiskStore().GetSourcePath (kDiskSlot, 0));
    m_isBootDiskNoted = true;

    if (m_programBootDisk.has_value() && *m_programBootDisk != m_bootDisk)
    {
        UnloadProgramFiles();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject::UnloadProgramFiles
//
//  The debug file with the symbol table it loaded into, and the binary BLOAD
//  loaded: the program on the disk booted before.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugProject::UnloadProgramFiles()
{
    DebugSession                 * session = (m_context.GetDebugController() != nullptr) ? &m_context.GetDebugController()->GetSession() : nullptr;
    std::optional<SymbolTableId>   table;
    std::vector<std::string>       lines;



    m_programBootDisk.reset();

    if (session == nullptr)
    {
        return;
    }

    table = session->GetDebugFileTable();

    if (session->HasDebugFile())
    {
        lines.push_back (std::format ("Unloaded {}: another disk was booted from drive 1.",
                                      ConsoleLinks::LinkFileName (TextEncoding::WideToNarrow (fs::path (session->GetDebugFilePath()).filename().native()),
                                                                  TextEncoding::WideToNarrow (session->GetDebugFilePath()))));
    }

    if (table.has_value())
    {
        session->GetSymbols().Clear (*table);
        session->ForgetSymbolLoads  (*table);
        session->ForgetAddedSymbols (*table);
    }

    session->ClearDebugFile();
    session->ForgetBinaryLoad();
    m_context.AddConsoleLines (lines);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject::Service
//
//  CPU thread, each service tick while the debugger is open, outside any
//  command: a project switch the signals call for, whichever client's
//  command changed them, a PROJECT OPEN or MERGE waiting, and a save when
//  the project differs from its file.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugProject::Service()
{
    //  How often the open project is compared with what the debugger has now,
    //  and written when they differ.
    constexpr ULONGLONG  kCheckMs = DebugProjectStore::kSaveDelayMs;
    ULONGLONG            now      = GetTickCount64();



    Sync();

    if (m_projectToOpen.has_value())
    {
        DebugProject  opened = std::move (*m_projectToOpen);



        m_projectToOpen.reset();
        Save();
        opened.scope = m_project.scope;
        Apply (opened);

        //  A debug file the project loads changes the scope; as for a
        //  restore, that is no switch the user made, so no merge is offered.
        m_restoredScope = GetScope();
    }

    if (m_merge.TakeOwed())
    {
        m_merge.RunLines();
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

    if (m_hasProject && now - m_projectCheckedAt >= kCheckMs)
    {
        m_projectCheckedAt = now;
        Save();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject::Sync
//
//  When the scope the signals choose is not the open project's, the open
//  one is saved and the scope's opens. The old project is saved with the symbol
//  files it had at its last capture: the load that changed the scope is part
//  of the new one, and captured into the old project it would load the next
//  time that opened, which would switch away from it again. Switches back
//  and forth are paused by m_switchGuard. A scope with no project yet starts
//  from what the debugger and its window have now. An existing project for
//  a more specific scope replaces the debugger's items with its own. The
//  user is offered the old project's breakpoints, watches and bookmarks
//  too, when it has any and the switch is not part of restoring the old
//  project, as when a machine's project loads a debug file.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugProject::Sync()
{
    ProjectScope              scope;
    DebugProject              old;
    DebugProject              opened;
    std::vector<std::string>  notices;
    bool                      isNew   = false;
    bool                      isOffer = false;
    HRESULT                   hr      = S_OK;



    if (m_projectStore == nullptr || m_context.GetDebugController() == nullptr)
    {
        return;
    }

    scope = GetScope();

    if (m_hasProject && scope == m_project.scope)
    {
        return;
    }

    if (m_hasProject && !TryAllowSwitch (scope))
    {
        return;
    }

    if (m_hasProject)
    {
        Save (true);
        old     = m_project;
        isOffer = scope.IsMoreSpecificThan (old.scope) && scope != m_restoredScope && DebugProjectMerge::HasItems (old);
    }

    if (scope.kind == ProjectScopeKind::DebugFile || scope.kind == ProjectScopeKind::Binary)
    {
        m_programBootDisk = GetBootDisk();
    }
    else
    {
        m_programBootDisk.reset();
    }

    hr = m_projectStore->Open (scope, opened, notices);
    IGNORE_RETURN_VALUE (hr, S_OK);

    ConsoleLinks::LinkFiles (notices, { TextEncoding::WideToNarrow (opened.filePath) });
    m_context.AddConsoleLines (notices);

    isNew = opened.engineScript.empty() && opened.symbolFiles.empty() && opened.windowStrings.empty() && opened.startupCommands.empty();

    if (isNew)
    {
        Capture (opened);

        m_project    = opened;
        m_hasProject = true;
        m_projectSaved.clear();
        Publish();
        return;
    }

    Apply (opened);
    m_restoredScope = GetScope();

    if (isOffer)
    {
        m_merge.Offer (old, m_project.scope.label);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject::TryAllowSwitch
//
//  One console line is added when switching is paused.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDebugProject::TryAllowSwitch (const ProjectScope & scope)
{
    bool  wasHeld = m_switchGuard.IsHeld();



    if (m_switchGuard.TryAllow (scope, GetTickCount64()))
    {
        return true;
    }

    if (!wasHeld)
    {
        m_context.AddConsoleLines ({ std::format ("The debug project for {} and the one for {} each load a file that opens the other, so switching stops at {} "
                                        "until the debug file, binary or machine changes.",
                                        m_project.scope.label, scope.label, m_project.scope.label) });
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject::Apply
//
//  The project's items into the session, its window strings passed to the
//  UI thread, then its startup commands run in order, each in its mode.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugProject::Apply (const DebugProject & project)
{
    std::vector<std::string>  failures;
    std::vector<std::string>  lines;



    {
        std::lock_guard<std::mutex>  viewLock (m_viewModel.GetLock());



        project.ApplyEngine (m_context.GetDebugController()->GetSession(), failures);
        m_viewModel.GetState().GetCommands().SetBreakpointUndoText (m_context.GetDebugController()->GetSession(), project.breakpointUndo);
    }

    {
        std::lock_guard<std::mutex>  lock (m_projectMutex);



        m_projectWindowPending = project;
    }

    m_project      = project;
    m_hasProject   = true;
    m_projectSaved = project.ToJson (fs::path (project.filePath).parent_path().native());
    Publish();

    lines.push_back (GetProjectLine (project.scope, project.filePath, GetScopeFilePath (project.scope)));

    for (const std::string & failure : failures)
    {
        lines.push_back ("Not restored: " + failure);
    }

    m_context.AddConsoleLines (lines);
    RunStartupCommands();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject::RunStartupCommands
//
//  Each through the console, so its lines appear as if typed.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugProject::RunStartupCommands()
{
    std::vector<std::string>  lines;



    for (const DebugProject::StartupCommand & command : m_project.startupCommands)
    {
        std::vector<std::string>  echoed;



        {
            std::lock_guard<std::mutex>  viewLock (m_viewModel.GetLock());



            echoed = m_viewModel.GetState().GetCommands().ExecuteConsoleLine (m_context.GetDebugController()->GetSession(), command.text, command.mode);
        }

        DebuggerViewCommands::AddCommandGap (echoed);
        lines.insert (lines.end(), echoed.begin(), echoed.end());
    }

    m_context.AddConsoleLines (lines);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject::RunRequest
//
//  PROJECT, PROJECT SAVEAS file, PROJECT OPEN file and PROJECT MERGE, run
//  inside the command. An OPEN or a MERGE changes the session, so it is
//  made on the next service tick.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugProject::RunRequest (const std::string & text, Reply & reply)
{
    std::string                 verb;
    std::string                 path;
    size_t                      split   = text.find_first_of (" \t");
    std::vector<std::string>    notices;
    DebugProject                opened;
    const DebugProject        * reported = &m_project;
    DebugSession              & session  = m_context.GetDebugController()->GetSession();
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

    if (!path.empty() && !session.CheckHostAccess (session.ResolvePath (path), verb == "SAVEAS" ? HostAccess::Write : HostAccess::Read, reply))
    {
        return;
    }

    if (verb == "SAVEAS" && !path.empty())
    {
        Capture (m_project);
        hr = m_projectStore->SaveAs (m_project, m_context.GetDebugController()->GetSession().ResolvePath (path));

        if (FAILED (hr))
        {
            reply.SetError (CommandStatus::Error, "file not written", std::format ("{} could not be written.", path));
            return;
        }

        m_projectSaved = m_project.ToJson (fs::path (m_project.filePath).parent_path().native());
        Publish();
    }
    else if (verb == "OPEN" && !path.empty())
    {
        hr = m_projectStore->OpenFile (m_context.GetDebugController()->GetSession().ResolvePath (path), opened, notices);

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
        m_merge.MarkOwed();
    }
    else if (!verb.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "PROJECT takes SAVEAS file, OPEN file or MERGE, or nothing.");
        return;
    }

    reply.data = ProjectData { ProjectScope::GetKindName (reported->scope.kind), reported->scope.key, reported->scope.label,
                               TextEncoding::WideToNarrow (reported->filePath), reported->isAutomatic };
    reply.files.push_back (TextEncoding::WideToNarrow (reported->filePath));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject::Capture
//
//  The session's items and the window's strings, as they are now, apart from
//  the layout, which is the window's. Strings a switch passed to the UI
//  thread, not yet applied to the window, are the project's.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugProject::Capture (DebugProject & project)
{
    std::lock_guard<std::mutex>  lock (m_projectMutex);



    //  The breakpoint undo history changes only on the CPU thread, which this
    //  runs on, so reading it takes no view lock; a PROJECT command arrives
    //  here with that lock already taken.
    project.breakpointUndo = m_viewModel.GetState().GetCommands().GetBreakpointUndoText();
    project.CaptureEngine (m_context.GetDebugController()->GetSession(), &m_context.GetDebugController()->GetHandlers().GetExecution());

    if (m_projectWindowPending.has_value())
    {
        project.rangeSets          = m_projectWindowPending->rangeSets;
        project.disassemblyOptions = m_projectWindowPending->disassemblyOptions;
        project.windowStrings      = m_projectWindowPending->windowStrings;
        return;
    }

    project.rangeSets          = m_settings.heatMapRanges;
    project.disassemblyOptions = m_settings.disassemblyOptions;
    project.windowStrings      = GetWindowStrings();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject::GetWindowStrings
//
//  The window's settings a project saves beyond its two, by name, without
//  the layout's; the caller has the project lock.
//
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, std::string> ShellDebugProject::GetWindowStrings() const
{
    std::map<std::string, std::string>  strings;



    for (const auto & [name, text] : m_settings.windowState)
    {
        if (!DebugProject::IsLayoutString (name))
        {
            strings[name] = text;
        }
    }

    strings["heatMapOptions"]  = m_settings.heatMapOptions;
    strings["diskLogOptions"]  = m_settings.diskLogOptions;
    strings["inputLogOptions"] = m_settings.inputLogOptions;
    strings["videoLogOptions"] = m_settings.videoLogOptions;
    strings["soundLogOptions"] = m_settings.soundLogOptions;
    strings["serialLogOptions"]  = m_settings.serialLogOptions;
    strings["networkLogOptions"] = m_settings.networkLogOptions;
    return strings;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject::SetWindowStrings
//
//  As the two, a setting is replaced only by a string the project has; the
//  caller has the project lock. The layout's strings are the window's, so an
//  older project's are left out.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugProject::SetWindowStrings (const std::map<std::string, std::string> & strings)
{
    std::pair<const char *, std::string *>  settings[] =
    {
        { "heatMapOptions",  &m_settings.heatMapOptions  },
        { "diskLogOptions",  &m_settings.diskLogOptions  },
        { "inputLogOptions", &m_settings.inputLogOptions },
        { "videoLogOptions", &m_settings.videoLogOptions },
        { "soundLogOptions", &m_settings.soundLogOptions },
        { "serialLogOptions",  &m_settings.serialLogOptions },
        { "networkLogOptions", &m_settings.networkLogOptions },
    };



    for (const auto & [name, text] : strings)
    {
        auto  setting = std::ranges::find_if (settings, [&name] (const auto & each) { return name == each.first; });

        if (text.empty() || DebugProject::IsLayoutString (name))
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
//  ShellDebugProject::Save
//
//  CPU thread. Written only when the file's text would change; with
//  `isKeepingSymbolFiles`, its symbol files are the ones last captured.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugProject::Save (bool isKeepingSymbolFiles)
{
    std::vector<DebugProject::SymbolFile>  symbolFiles = m_project.symbolFiles;
    std::string                            text;
    HRESULT                                hr          = S_OK;



    if (!m_hasProject || m_context.GetDebugController() == nullptr || m_projectStore == nullptr)
    {
        return;
    }

    Capture (m_project);

    if (isKeepingSymbolFiles)
    {
        m_project.symbolFiles = std::move (symbolFiles);
    }

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
//  ShellDebugProject::TakeWindowState
//
//  UI thread: the window strings of a project just opened, made the
//  window's settings; false when no project opened since the last call.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDebugProject::TakeWindowState()
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
        m_settings.heatMapRanges      = pending->rangeSets.empty()          ? m_settings.heatMapRanges      : pending->rangeSets;
        m_settings.disassemblyOptions = pending->disassemblyOptions.empty() ? m_settings.disassemblyOptions : pending->disassemblyOptions;

        SetWindowStrings (pending->windowStrings);
    }

    m_host.SaveSettings();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject::Publish
//
//  CPU thread: a copy of the open project for the window's dialogs.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugProject::Publish()
{
    std::lock_guard<std::mutex>  lock (m_projectMutex);



    m_projectCopy = m_project;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject::TryGetCopy
//
//  UI thread.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellDebugProject::TryGetCopy (DebugProject & project)
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
//  ShellDebugProject::SetStartupCommands
//
//  UI thread: the CPU thread takes them on its next service tick, and the
//  project is saved with them.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugProject::SetStartupCommands (const std::vector<DebugProject::StartupCommand> & commands)
{
    std::lock_guard<std::mutex>  lock (m_projectMutex);



    m_startupPending = commands;
}





