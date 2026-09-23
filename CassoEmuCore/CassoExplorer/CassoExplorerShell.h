#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "Config/Win32FileSystem.h"
#include "Seams/Win32DiskFileIo.h"
#include "Seams/Win32FolderWatcher.h"
#include "Seams/Win32IntentChannel.h"
#include "Theme/DxuiDarkTheme.h"
#include "Theme/DxuiLightTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerLaunchOptions
//
//  What CassoExplorer's command line asks for. The emulator launches it with
//  `--owner <window handle in decimal>` and, when it has one, `--title
//  <label>`; a launch from a shortcut has neither.
//
////////////////////////////////////////////////////////////////////////////////

struct CassoExplorerLaunchOptions
{
    HWND          owner    = nullptr;
    bool          hasOwner = false;
    std::wstring  titlePrefix;
    std::wstring  refusal;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell
//
//  The browser's top-level window and the process state around it: COM, the
//  palette, and the placement the preferences remember.
//
//  ONE WINDOW PER OWNER. The emulator's menu item launches a new process each
//  time it is chosen, so the process looks for a window already serving the
//  same emulator first and fronts it rather than opening a second. The owner
//  is recorded as a window property, which any process can read back by
//  enumerating windows of this class.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerShell
{
public:
    CassoExplorerShell();
    ~CassoExplorerShell();

    CassoExplorerShell (const CassoExplorerShell &)             = delete;
    CassoExplorerShell & operator= (const CassoExplorerShell &) = delete;

    //  Reads the arguments after the program name. An argument it does not
    //  take, or an owner that is not a decimal handle, fills `refusal` and
    //  fails with E_INVALIDARG.
    static HRESULT  ParseArguments (const std::vector<std::wstring> & arguments, CassoExplorerLaunchOptions & outOptions);

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

    //  EHM hooks for a process with no dialog host of its own.
    static void  NotifyUser      (const wchar_t * message);
    static void  ReportAssertion (const wchar_t * message);

    HRESULT  Initialize     (HINSTANCE instance, const CassoExplorerLaunchOptions & options, const CassoExplorerPrefs & prefs, int showCommand);
    int      RunMessageLoop ();
    HWND     GetWindow      () const;

    static constexpr const wchar_t *  kWindowClass   = L"CassoExplorerWindow";
    static constexpr const wchar_t *  kOwnerProperty = L"CassoExplorerOwner";
    static constexpr const wchar_t *  kAppName       = L"Casso Explorer";

    static constexpr int  kDefaultWidthDip  = 1280;
    static constexpr int  kDefaultHeightDip = 720;

private:
    Win32FileSystem                       m_fs;
    Win32DiskFileIo                       m_fileIo;
    Win32FolderWatcher                    m_watcher;
    Win32IntentChannel                    m_intentChannel;
    CassoExplorerBrowser                  m_browser;
    CassoExplorerActions                  m_actions;
    std::unique_ptr<CassoExplorerWindow>  m_window;
    bool                                  m_oleInitialized = false;
    CassoExplorerLaunchOptions            m_options;
    CassoExplorerPrefs                    m_prefs;
    std::wstring                          m_baseDir;
};