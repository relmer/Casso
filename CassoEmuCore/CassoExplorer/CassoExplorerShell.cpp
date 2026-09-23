#include "Pch.h"

#include "CassoExplorer/CassoExplorerShell.h"
#include "AssetBootstrap.h"
#include "BuildInfo.h"
#include "CassoExplorer/Model/KnownFolderStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::CassoExplorerShell
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerShell::CassoExplorerShell()
    : m_browser (m_fs, m_fileIo),
      m_actions (m_browser, m_fs)
{
    //  A write to an image tells any Casso with it mounted to reload it.
    m_browser.GetOperations().SetIntentChannel (&m_intentChannel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::~CassoExplorerShell
//
//  The window goes before COM, since its drop registration and popups are
//  released with it.
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerShell::~CassoExplorerShell()
{
    m_window.reset();

    if (m_oleInitialized)
    {
        OleUninitialize();
        m_oleInitialized = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::ParseArguments
//
//  `--owner` takes the decimal the emulator writes; `--title` takes the rest
//  of one argument, which the emulator quotes. Either may be given once.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerShell::ParseArguments (const std::vector<std::wstring> & arguments, CassoExplorerLaunchOptions & outOptions)
{
    HRESULT  hr    = S_OK;
    size_t   i     = 0;
    bool     valid = true;



    outOptions = CassoExplorerLaunchOptions();

    for (i = 0; i < arguments.size() && valid; i++)
    {
        const std::wstring &  argument = arguments[i];
        bool                  hasValue = i + 1 < arguments.size();

        if (argument == L"--owner" && hasValue && !outOptions.hasOwner)
        {
            const std::wstring &  value  = arguments[++i];
            wchar_t *             end    = nullptr;
            uint64_t              handle = 0;

            valid = !value.empty() && iswdigit (value[0]);

            if (valid)
            {
                handle = wcstoull (value.c_str(), &end, 10);
                valid  = end != nullptr && *end == L'\0' && handle != 0;
            }

            if (valid)
            {
                outOptions.owner    = (HWND) (UINT_PTR) handle;
                outOptions.hasOwner = true;
            }
            else
            {
                outOptions.refusal = L"--owner takes a window handle in decimal, not \"" + value + L"\".";
            }
        }
        else if (argument == L"--title" && hasValue && outOptions.titlePrefix.empty())
        {
            outOptions.titlePrefix = arguments[++i];
        }
        else
        {
            valid              = false;
            outOptions.refusal = L"Casso Explorer does not take \"" + argument + L"\" here. It takes --owner <handle> and --title <label>.";
        }
    }

    CBREx (valid, E_INVALIDARG);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::FindWindowForOwner
//
////////////////////////////////////////////////////////////////////////////////

HWND CassoExplorerShell::FindWindowForOwner (HWND owner)
{
    HWND  found = nullptr;
    HWND  next  = nullptr;



    if (owner == nullptr)
    {
        return nullptr;
    }

    do
    {
        next = FindWindowExW (nullptr, next, kWindowClass, nullptr);

        if (next != nullptr && (HWND) GetPropW (next, kOwnerProperty) == owner)
        {
            found = next;
        }
    }
    while (next != nullptr && found == nullptr);

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::FrontWindow
//
//  A process launched by the emulator's menu item holds the foreground
//  permission the click granted, so it can pass it to the window it found.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerShell::FrontWindow (HWND hwnd)
{
    BOOL  result = FALSE;



    if (IsIconic (hwnd))
    {
        result = ShowWindow (hwnd, SW_RESTORE);
        IGNORE_RETURN_VALUE (result, TRUE);
    }

    result = SetForegroundWindow (hwnd);
    IGNORE_RETURN_VALUE (result, TRUE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::ChooseTheme
//
////////////////////////////////////////////////////////////////////////////////

const DxuiTheme & CassoExplorerShell::ChooseTheme (const std::string   & theme,
                                             bool                  systemDark,
                                             const DxuiLightTheme & light,
                                             const DxuiDarkTheme  & dark)
{
    if (theme == CassoExplorerPrefs::kThemeLight)
    {
        return light;
    }

    if (theme == CassoExplorerPrefs::kThemeDark)
    {
        return dark;
    }

    if (systemDark)
    {
        return dark;
    }

    return light;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::ComposeTitle
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerShell::ComposeTitle (const std::wstring & titlePrefix, const std::wstring & buildInfo)
{
    std::wstring  title;



    if (!titlePrefix.empty())
    {
        title = titlePrefix + L" - ";
    }

    title += kAppName;

    //  Stated outright, since a version and a timestamp do not say "debug" to
    //  anyone reading them, and a debug build is several times slower.
    if (!buildInfo.empty())
    {
        title += L" [Debug] - " + buildInfo;
    }

    return title;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::NotifyUser
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerShell::NotifyUser (const wchar_t * message)
{
    int  choice = 0;



    choice = MessageBoxW (nullptr, message != nullptr ? message : L"", kAppName, MB_OK | MB_ICONERROR | MB_TASKMODAL);
    IGNORE_RETURN_VALUE (choice, IDOK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::ReportAssertion
//
//  One dialog at a time, and never while one is already up: a message box
//  runs its own loop and can dispatch the paint that failed, which would stack
//  a second box on the first. A debugger attached gets the break instead.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerShell::ReportAssertion (const wchar_t * message)
{
    static std::atomic<bool>  reporting;
    int                       choice = IDIGNORE;



    if (IsDebuggerPresent())
    {
        __debugbreak();
        return;
    }

    if (reporting.exchange (true))
    {
        return;
    }

    choice = MessageBoxW (nullptr, message != nullptr ? message : L"", L"Casso Explorer assertion failed",
                          MB_ABORTRETRYIGNORE | MB_ICONERROR | MB_TASKMODAL);

    reporting = false;

    if (choice == IDABORT)
    {
        ExitProcess (3);
    }

    if (choice == IDRETRY)
    {
        __debugbreak();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::GetDriveRoots
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> CassoExplorerShell::GetDriveRoots()
{
    std::vector<std::wstring>  roots;
    wchar_t                    buffer[512] = {};
    DWORD                      length      = GetLogicalDriveStringsW (ARRAYSIZE (buffer), buffer);
    const wchar_t *            root        = buffer;



    if (length == 0 || length > ARRAYSIZE (buffer))
    {
        return roots;
    }

    while (*root != L'\0')
    {
        roots.push_back (root);
        root += wcslen (root) + 1;
    }

    return roots;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::Initialize
//
//  COM first, because the drag source and drop target the browser carries
//  need OLE, and OLE initialization has to precede any window that registers
//  for drops. The known folders are read once here; the tree reads them from
//  the model after that.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerShell::Initialize (HINSTANCE instance, const CassoExplorerLaunchOptions & options, const CassoExplorerPrefs & prefs, int showCommand)
{
    HRESULT                               hr      = S_OK;
    std::vector<KnownFolderStore::Entry>  entries;
    std::vector<std::wstring>             folders;
    BOOL                                  result  = FALSE;



    m_options = options;
    m_prefs   = prefs;
    m_baseDir = AssetBootstrap::GetAssetBaseDirectory().wstring();

    hr = OleInitialize (nullptr);
    CHR (hr);

    m_oleInitialized = true;

    {
        KnownFolderStore  store (m_fs, m_baseDir);
        HRESULT           hrLoad = store.Load (entries);

        IGNORE_RETURN_VALUE (hrLoad, S_OK);
    }

    folders = KnownFolderStore::ListRootFolders (m_fs, m_baseDir, entries);

    m_browser.GetTreeModel().SetKnownFolders (folders);
    m_browser.GetTreeModel().SetDrives (GetDriveRoots());
    m_browser.GetTreeModel().SetDirectoryProbe ([] (const std::wstring & path)
    {
        DWORD  attributes = GetFileAttributesW (path.c_str());

        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    });

    {
        CassoExplorerWindow::Context  context;

        context.fs          = &m_fs;
        context.baseDir     = m_baseDir;
        context.owner       = m_options.owner;
        context.titlePrefix = m_options.titlePrefix;
        context.watcher     = &m_watcher;

        m_window = std::make_unique<CassoExplorerWindow> (m_browser, m_actions, m_prefs, context);
    }

#if defined (_DEBUG)
    hr = m_window->Open (instance, ComposeTitle (m_options.titlePrefix, GetCassoBuildInfo()), showCommand);
#else
    hr = m_window->Open (instance, ComposeTitle (m_options.titlePrefix, L""), showCommand);
#endif
    CHR (hr);

    //  A reload a write asks for names this window, so a conflict or a
    //  refusal comes back to be shown.
    m_intentChannel.SetSender (m_window->GetHwnd());

    if (m_options.hasOwner)
    {
        result = SetPropW (m_window->GetHwnd(), kOwnerProperty, (HANDLE) m_options.owner);
        CWR (result);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::GetWindow
//
////////////////////////////////////////////////////////////////////////////////

HWND CassoExplorerShell::GetWindow() const
{
    return (m_window != nullptr) ? m_window->GetHwnd() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::RunMessageLoop
//
//  The preferences are written once the loop ends, whatever ended it.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerShell::RunMessageLoop()
{
    MSG      message = {};
    BOOL     got     = FALSE;
    HRESULT  hr      = S_OK;



    for (;;)
    {
        got = GetMessageW (&message, nullptr, 0, 0);

        if (got == 0 || got == -1)
        {
            break;
        }

        TranslateMessage (&message);
        DispatchMessageW (&message);
    }

    if (m_window != nullptr && m_window->GetHwnd() != nullptr)
    {
        RemovePropW (m_window->GetHwnd(), kOwnerProperty);
    }

    hr = m_prefs.Save (m_baseDir, m_fs);
    IGNORE_RETURN_VALUE (hr, S_OK);

    return (got == -1) ? 1 : (int) message.wParam;
}
