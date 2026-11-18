#pragma once

#include "Debugger/Project/DebugProject.h"
#include "Debugger/Project/DebugProjectStore.h"
#include "Debugger/Project/ProjectScope.h"
#include "Debugger/Project/ProjectSwitchGuard.h"
#include "Shell/DebugProjectMerge.h"

class DebugViewModel;
class IDebuggerHost;
class IShellDebugContext;
class MachineHost;
class Win32FileSystem;
struct DebuggerSettings;
struct Reply;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugProject
//
//  The debug projects (FR-166 to FR-171), on the CPU thread: the store, the
//  project open and its file's text as last written, and the disk drive 1
//  had at the last power cycle. The scope the signals choose -- the debug
//  file, the binary, the boot disk, the machine -- picks the project, and a
//  change of scope saves the open one and opens the scope's on the service
//  tick, outside any command. The window strings of a project just opened,
//  a copy of the open project and the startup commands the window sets are
//  passed to and from the UI thread under the project lock.
//
////////////////////////////////////////////////////////////////////////////////

class ShellDebugProject
{
public:
    ShellDebugProject (IDebuggerHost      & host,
                       MachineHost        & machine,
                       DebuggerSettings   & settings,
                       Win32FileSystem    & debugFiles,
                       IShellDebugContext & context,
                       DebugViewModel     & viewModel);

    ShellDebugProject             (const ShellDebugProject &) = delete;
    ShellDebugProject & operator= (const ShellDebugProject &) = delete;

    //  The console's line once a debug project loads: the word "project"
    //  linked to the project's file, and the scope's label with the file it
    //  ends with linked to `targetPath`.
    static std::string  GetProjectLine (const ProjectScope & scope, const std::wstring & projectPath, const std::wstring & targetPath);

    //  The store opens with the debug channel; a test sets a different one.
    void   OpenStore     ();
    void   SetStore      (std::unique_ptr<DebugProjectStore> store) { m_projectStore = std::move (store); }

    //  CPU thread: the service tick, a save when the file's text would
    //  change, and the project closed with the debugger.
    void   Service       ();
    void   Save          (bool isKeepingSymbolFiles = false);
    void   Close         ();
    void   NoteBootDisk  ();

    //  PROJECT and its forms, run inside the command.
    void   RunRequest    (const std::string & text, Reply & reply);

    //  UI thread: the window strings of a project just opened, a copy of the
    //  open project, and the startup commands the window sets.
    bool   TakeWindowState    ();
    bool   TryGetCopy         (DebugProject & project);
    void   SetStartupCommands (const std::vector<DebugProject::StartupCommand> & commands);

    //  The window's strings a project saves beyond its two; the caller has
    //  the project lock.
    std::map<std::string, std::string>  GetWindowStrings () const;
    void                                SetWindowStrings (const std::map<std::string, std::string> & strings);

    std::mutex          & GetMutex ()       { return m_projectMutex; }
    DebugProjectMerge   & GetMerge ()       { return m_merge; }

private:
    //  The Disk II controller's slot, as the machine host has it.
    static constexpr int  kDiskSlot = 6;

    ProjectScope  GetScope           ();
    std::wstring  GetBootDisk        ();
    std::wstring  GetScopeFilePath   (const ProjectScope & scope);
    void          UnloadProgramFiles ();
    void          Sync               ();
    bool          TryAllowSwitch     (const ProjectScope & scope);
    void          Apply              (const DebugProject & project);
    void          RunStartupCommands ();
    void          Capture            (DebugProject & project);
    void          Publish            ();

    IDebuggerHost                                           & m_host;
    MachineHost                                             & m_machine;
    DebuggerSettings                                        & m_settings;   // the emulator's preferences have it; saved through the host
    Win32FileSystem                                         & m_debugFiles;
    IShellDebugContext                                      & m_context;
    DebugViewModel                                          & m_viewModel;
    std::unique_ptr<DebugProjectStore>                        m_projectStore;
    DebugProject                                              m_project;
    bool                                                      m_hasProject           = false;
    std::optional<DebugProject>                               m_projectToOpen;
    std::string                                               m_projectSaved;
    std::wstring                                              m_bootDisk;
    bool                                                      m_isBootDiskNoted      = false;

    //  The boot disk while the project is a debug file's or a binary's.
    std::optional<std::wstring>                               m_programBootDisk;
    ULONGLONG                                                 m_projectCheckedAt     = 0;
    std::mutex                                                m_projectMutex;
    std::optional<DebugProject>                               m_projectWindowPending;
    std::optional<DebugProject>                               m_projectCopy;   // a copy of the open project, for the window
    std::optional<std::vector<DebugProject::StartupCommand>>  m_startupPending;

    // The scope a project's own symbol files called for once it was applied,
    // as when a machine's project loads a debug file. Switching to it is part
    // of the same restore, so the user is not offered a merge.
    std::optional<ProjectScope>                               m_restoredScope;
    ProjectSwitchGuard                                        m_switchGuard;
    DebugProjectMerge                                         m_merge;
};
