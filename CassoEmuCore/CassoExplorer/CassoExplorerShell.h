#pragma once

#include "Pch.h"

#include "CassoLink/CassoLinkFolder.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/ICassoExplorerHost.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoExplorer/Model/CassoExplorerSession.h"
#include "Config/Win32FileSystem.h"
#include "Seams/Win32ByteStreamFactory.h"
#include "Seams/Win32DiskFileIo.h"
#include "Seams/IProcessProbe.h"
#include "CassoLink/RunningCassoRegistry.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLaunchOptions
//
//  What CassoExplorer's command line asks for. The emulator launches it with
//  `--owner <window handle in decimal>` and, when it has one, `--title
//  <label>`; a launch from a shortcut has neither. A path, given once without
//  a switch, opens in a tab: a folder, a disk image, or a file's folder.
//  `--new-window` opens a window of its own on the path alone, rather than
//  the windows of the last session.
//
////////////////////////////////////////////////////////////////////////////////

struct CassoExplorerLaunchOptions
{
    HWND          owner     = nullptr;
    bool          hasOwner  = false;
    bool          newWindow = false;
    std::wstring  titlePrefix;
    std::wstring  openPath;
    std::wstring  refusal;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell
//
//  The app: every Casso Explorer window, in one process, and the state they
//  share -- COM, the preferences, the CassoLink connections, the running
//  Cassos -- with what each window has of its own: a browser, its actions,
//  a folder watcher, and the intent channel for its writes.
//
//  ONE PROCESS. A second launch finds a window of this class and sends its
//  command line to that window's app, which opens the window; the launch
//  then exits. ONE WINDOW PER OWNER: a launch from an emulator that a window
//  already serves fronts that window instead, and opens its path there. The
//  owner is stored as a window property, which the launch reads back by
//  enumerating windows of this class.
//
//  THE SESSION IS THE WINDOWS LEFT STANDING. File > Exit, and Windows
//  signing out or shutting down, write every window for the next launch. A
//  window closed while others stay open is left out; the last window closed
//  is written, with every window closed in a quick run before it, as the
//  taskbar's Close all windows closes them one at a time. The session is
//  written as each window changes, so a process that never closes cleanly
//  reopens where it was.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerShell : public ICassoExplorerHost
{
public:
    CassoExplorerShell();
    ~CassoExplorerShell() override;

    CassoExplorerShell (const CassoExplorerShell &)             = delete;
    CassoExplorerShell & operator= (const CassoExplorerShell &) = delete;

    //  Reads the arguments after the program name. An argument it does not
    //  take, or an owner that is not a decimal handle, fills `refusal` and
    //  fails with E_INVALIDARG.
    static HRESULT  ParseArguments (const std::vector<std::wstring> & arguments, CassoExplorerLaunchOptions & outOptions);

    //  The arguments of a command line, split as the C runtime splits them.
    static std::vector<std::wstring>  SplitArguments (const std::wstring & commandLine);

    //  A launch's options as the command line that parses back to them, for
    //  the app already running.
    static std::wstring  MakeForwardedArguments (const CassoExplorerLaunchOptions & options);

    //  `path` made full against this process's current folder, since the app
    //  it goes to has its own; empty for an empty path or one with no full form.
    static std::wstring  GetFullPath (const std::wstring & path);

    //  `text` to `hwnd` in a WM_COPYDATA with `id`, as UTF-16, waiting a few
    //  seconds at most for a window that has stopped answering.
    static void  ForwardToWindow (HWND hwnd, ULONG_PTR id, const std::wstring & text);

    //  The windows the first launch opens: the last session's, the first of
    //  them with the launch's path and owner; or, with none saved, one on the
    //  tabs in the preferences from before there were sessions; or one on Home.
    //  `--new-window` opens one on its path alone.
    static std::vector<CassoExplorerWindowRequest>  PlanFirstLaunch (const CassoExplorerLaunchOptions              & options,
                                                                     const std::vector<CassoExplorerSessionWindow> & session,
                                                                     const CassoExplorerPrefs                      & prefs);

    //  A window closed while others stayed open, and when.
    struct ClosedWindow
    {
        ULONGLONG                   closedMs = 0;
        CassoExplorerSessionWindow  window;
    };

    //  The windows closed in the run that ends at `nowMs`, oldest first: back
    //  from `nowMs`, each closed no more than kCloseRunMs before the next.
    static std::vector<CassoExplorerSessionWindow>  GetClosedRun (const std::vector<ClosedWindow> & closed, ULONGLONG nowMs);

    static constexpr ULONGLONG  kCloseRunMs = 3000;

    //  The window another launch opens in the running app, stepped down and
    //  right from `from`, the window it was opened over.
    static CassoExplorerWindowRequest  PlanForwardedLaunch (const CassoExplorerLaunchOptions & options, const CassoExplorerPrefs::Placement & from);

    //  The window already serving `owner`, or null.
    static HWND  FindWindowForOwner (HWND owner);

    //  Restores a minimized window and brings it forward.
    static void  FrontWindow (HWND hwnd);

    //  The palette Light, Dark or Follow system selects. The window applies
    //  Casso's own themes itself; for any other name this follows the system.
    static const DxuiTheme &  ChooseTheme (const std::string    & theme,
                                           bool                   systemDark,
                                           const DxuiLightTheme & light,
                                           const DxuiDarkTheme  & dark);

    //  The caption: the launcher's label first, as the emulator composes its
    //  own, so the two windows from one session read alike. A debug build
    //  passes its build identity, which follows a [Debug] tag as in Casso's.
    static std::wstring  ComposeTitle (const std::wstring & titlePrefix, const std::wstring & buildInfo);

    //  The host's drive roots, such as C:\, in drive-letter order.
    static std::vector<std::wstring>  GetDriveRoots();

    //  The name a folder's desktop.ini gives it, such as "Camera Roll", or
    //  empty for none.
    static std::wstring  GetLocalizedFolderName (const std::wstring & path);
    static std::wstring  GetPlainFolderName     (const std::wstring & path);

    //  EHM hooks for a process with no dialog host of its own.
    static void  NotifyUser      (const wchar_t * message);
    static void  ReportAssertion (const wchar_t * message);

    HRESULT  Initialize     (HINSTANCE instance, const CassoExplorerLaunchOptions & options, const CassoExplorerPrefs & prefs, int showCommand);
    int      RunMessageLoop ();
    HWND     GetWindow      () const;

    //  ICassoExplorerHost
    CassoExplorerWindow *  OpenWindow    (CassoExplorerWindowRequest request) override;
    CassoExplorerWindow *  GetWindowFor  (HWND hwnd) override;
    void                   ForEachWindow (const std::function<void (CassoExplorerWindow & window)> & fn) override;
    void                   CloseWindow   (CassoExplorerWindow & window) override;
    void                   ExitAll       () override;
    void                   SaveSession   () override;
    void                   RunLaunch     (const std::wstring & arguments) override;

    static constexpr const wchar_t *  kWindowClass   = L"CassoExplorerWindow";
    static constexpr const wchar_t *  kOwnerProperty = L"CassoExplorerOwner";
    static constexpr const wchar_t *  kAppName       = L"Casso Explorer";

    static constexpr int  kDefaultWidthDip  = 1280;
    static constexpr int  kDefaultHeightDip = 720;

private:
    //  One window and what it has of its own, released in the reverse of the
    //  order it was made in.
    struct WindowSlot;

    void  ConfigureBrowser       (CassoExplorerBrowser & browser);
    void  ReleaseClosedWindows   ();
    bool  HasOpenWindows         () const;
    std::vector<CassoExplorerSessionWindow>  GetOpenSession () const;

    //  The open windows, then `closed`, written as the session.
    void  SaveSessionWith (const std::vector<CassoExplorerSessionWindow> & closed);

    Win32FileSystem                           m_fs;
    Win32DiskFileIo                           m_fileIo;
    Win32ByteStreamFactory                    m_streams;
    CassoLinkFolder                           m_linkFolder;
    CassoLinkConnections                      m_links;
    Win32ProcessProbe                         m_processProbe;
    RunningCassoRegistry                      m_running;
    std::vector<std::unique_ptr<WindowSlot>>  m_windows;
    std::vector<ClosedWindow>                 m_closed;
    std::vector<std::wstring>                 m_knownFolders;
    bool                                      m_oleInitialized = false;
    bool                                      m_exiting        = false;
    bool                                      m_initializing   = false;
    HINSTANCE                                 m_instance       = nullptr;
    int                                       m_showCommand    = SW_SHOWNORMAL;
    CassoExplorerLaunchOptions                m_options;
    CassoExplorerPrefs                        m_prefs;
    std::wstring                              m_baseDir;
};
