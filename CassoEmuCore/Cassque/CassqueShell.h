#pragma once

#include "Pch.h"

#include "Cassque/CassqueActions.h"
#include "Cassque/CassqueBrowser.h"
#include "Cassque/CassqueWindow.h"
#include "Cassque/Model/CassquePrefs.h"
#include "Config/Win32FileSystem.h"
#include "Seams/Win32DiskFileIo.h"
#include "Seams/Win32IntentChannel.h"
#include "Theme/DxuiDarkTheme.h"
#include "Theme/DxuiLightTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueLaunchOptions
//
//  What Cassque's command line asks for. The emulator launches it with
//  `--owner <window handle in decimal>` and, when it has one, `--title
//  <label>`; a launch from a shortcut has neither.
//
////////////////////////////////////////////////////////////////////////////////

struct CassqueLaunchOptions
{
    HWND          owner    = nullptr;
    bool          hasOwner = false;
    std::wstring  titlePrefix;
    std::wstring  refusal;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueShell
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

class CassqueShell
{
public:
    CassqueShell();
    ~CassqueShell();

    CassqueShell (const CassqueShell &)             = delete;
    CassqueShell & operator= (const CassqueShell &) = delete;

    //  Reads the arguments after the program name. An argument it does not
    //  take, or an owner that is not a decimal handle, fills `refusal` and
    //  fails with E_INVALIDARG.
    static HRESULT  ParseArguments (const std::vector<std::wstring> & arguments, CassqueLaunchOptions & outOptions);

    //  The window already serving `owner`, or null.
    static HWND  FindWindowForOwner (HWND owner);

    //  Restores a minimized window and brings it forward.
    static void  FrontWindow (HWND hwnd);

    //  The palette a preference selects. The emulator's own themes have no
    //  palette here yet, so they follow the system as FollowSystem does.
    static const DxuiTheme &  ChooseTheme (const std::string    & theme,
                                           bool                   systemDark,
                                           const DxuiLightTheme & light,
                                           const DxuiDarkTheme  & dark);

    //  The caption: the launcher's label first, as the emulator composes its
    //  own, so the two windows from one session read alike.
    static std::wstring  ComposeTitle (const std::wstring & titlePrefix);

    //  The host's drive roots, such as C:\, in drive-letter order.
    static std::vector<std::wstring>  GetDriveRoots();

    //  EHM hooks for a process with no dialog host of its own.
    static void  NotifyUser      (const wchar_t * message);
    static void  ReportAssertion (const wchar_t * message);

    HRESULT  Initialize     (HINSTANCE instance, const CassqueLaunchOptions & options, const CassquePrefs & prefs, int showCommand);
    int      RunMessageLoop ();
    HWND     GetWindow      () const;

    static constexpr const wchar_t *  kWindowClass   = L"CassqueWindow";
    static constexpr const wchar_t *  kOwnerProperty = L"CassqueOwner";
    static constexpr const wchar_t *  kAppName       = L"Cassque";

    static constexpr int  kDefaultWidthDip  = 1280;
    static constexpr int  kDefaultHeightDip = 720;

private:
    Win32FileSystem                 m_fs;
    Win32DiskFileIo                 m_fileIo;
    Win32IntentChannel              m_intentChannel;
    CassqueBrowser                  m_browser;
    CassqueActions                  m_actions;
    std::unique_ptr<CassqueWindow>  m_window;
    bool                            m_oleInitialized = false;
    CassqueLaunchOptions            m_options;
    CassquePrefs                    m_prefs;
    std::wstring                    m_baseDir;
};