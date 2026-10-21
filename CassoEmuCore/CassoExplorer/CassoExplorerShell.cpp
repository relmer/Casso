#include "Pch.h"

#include "CassoExplorer/CassoExplorerShell.h"
#include "AssetBootstrap.h"
#include "BuildInfo.h"
#include "CassoExplorer/Model/KnownFolderStore.h"
#include "CassoExplorer/Model/LaunchCommand.h"
#include "Seams/Win32FolderWatcher.h"
#include "Seams/Win32IntentChannel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::WindowSlot
//
//  The window is last, since it has references to everything before it, and
//  is released first.
//
////////////////////////////////////////////////////////////////////////////////

struct CassoExplorerShell::WindowSlot
{
    std::unique_ptr<Win32FolderWatcher>    watcher;
    std::unique_ptr<Win32IntentChannel>    intents;
    std::unique_ptr<CassoExplorerBrowser>  browser;
    std::unique_ptr<CassoExplorerActions>  actions;
    std::unique_ptr<CassoExplorerWindow>   window;
    bool                                   closed = false;

    ~WindowSlot()
    {
        window.reset();
        actions.reset();
        browser.reset();
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::CassoExplorerShell
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerShell::CassoExplorerShell()
    : m_linkFolder (m_fs, m_fileIo),
      m_links (m_streams, m_linkFolder, m_fileIo),
      m_running (m_fs, m_processProbe, RunningCassoRegistry::GetDefaultFolder())
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::~CassoExplorerShell
//
//  The windows go before COM, since their drop registrations and popups are
//  released with them. Files this process put on the clipboard are flushed
//  first, so they paste after it exits.
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerShell::~CassoExplorerShell()
{
    HRESULT  hr = S_OK;



    m_windows.clear();

    if (m_oleInitialized)
    {
        hr = OleFlushClipboard();
        IGNORE_RETURN_VALUE (hr, S_OK);

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
        else if (argument == L"--new-window" && !outOptions.newWindow)
        {
            outOptions.newWindow = true;
        }
        else if (!argument.empty() && argument.rfind (L"--", 0) != 0 && outOptions.openPath.empty())
        {
            outOptions.openPath = argument;
        }
        else
        {
            valid              = false;
            outOptions.refusal = L"Casso Explorer does not take \"" + argument + L"\" here. It takes a path to open, --owner <handle>, --title <label> and --new-window.";
        }
    }

    CBREx (valid, E_INVALIDARG);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::SplitArguments
//
//  CommandLineToArgvW reads its first argument as a program name, so one is
//  supplied and dropped, and the first real argument is split by the rules
//  for every other.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> CassoExplorerShell::SplitArguments (const std::wstring & commandLine)
{
    int                        count = 0;
    LPWSTR                   * parts = CommandLineToArgvW ((L"CassoExplorer.exe " + commandLine).c_str(), &count);
    std::vector<std::wstring>  arguments;
    int                        i     = 0;



    for (i = 1; parts != nullptr && i < count; i++)
    {
        arguments.push_back (parts[i]);
    }

    LocalFree (parts);

    return arguments;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::MakeForwardedArguments
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerShell::MakeForwardedArguments (const CassoExplorerLaunchOptions & options)
{
    std::vector<std::wstring>  arguments;
    std::wstring               text;



    if (options.hasOwner)
    {
        arguments.push_back (L"--owner");
        arguments.push_back (std::to_wstring ((uint64_t) (UINT_PTR) options.owner));
    }

    if (!options.titlePrefix.empty())
    {
        arguments.push_back (L"--title");
        arguments.push_back (options.titlePrefix);
    }

    if (options.newWindow)
    {
        arguments.push_back (L"--new-window");
    }

    if (!options.openPath.empty())
    {
        arguments.push_back (options.openPath);
    }

    for (const std::wstring & argument : arguments)
    {
        text += (text.empty() ? L"" : L" ") + LaunchCommand::QuoteArgument (argument);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::GetFullPath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerShell::GetFullPath (const std::wstring & path)
{
    constexpr DWORD  kLongPathChars = 32768;



    std::wstring  full   (kLongPathChars, L'\0');
    DWORD         length = path.empty() ? 0 : GetFullPathNameW (path.c_str(), (DWORD) full.size(), full.data(), nullptr);



    full.resize (length < full.size() ? length : 0);

    return full;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::ForwardToWindow
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerShell::ForwardToWindow (HWND hwnd, ULONG_PTR id, const std::wstring & text)
{
    constexpr UINT  kSendTimeoutMs = 5000;



    COPYDATASTRUCT  copy   = {};
    DWORD_PTR       result = 0;
    LRESULT         sent   = 0;



    copy.dwData = id;
    copy.cbData = (DWORD) (text.size() * sizeof (wchar_t));
    copy.lpData = (void *) text.data();

    sent = SendMessageTimeoutW (hwnd, WM_COPYDATA, 0, (LPARAM) &copy, SMTO_ABORTIFHUNG, kSendTimeoutMs, &result);
    IGNORE_RETURN_VALUE (sent, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::PlanFirstLaunch
//
//  The tabs in the preferences are the first window's from the build before
//  sessions; they are read once, when there is no session yet, and opened
//  in one window at the preferences' placement.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<CassoExplorerWindowRequest> CassoExplorerShell::PlanFirstLaunch (
    const CassoExplorerLaunchOptions               & options,
    const std::vector<CassoExplorerSessionWindow>  & session,
    const CassoExplorerPrefs                       & prefs)
{
    std::vector<CassoExplorerWindowRequest>  requests;
    CassoExplorerWindowRequest               request;



    if (!options.newWindow && !session.empty())
    {
        for (const CassoExplorerSessionWindow & window : session)
        {
            CassoExplorerWindowRequest  restored;

            restored.tabs      = window.tabs;
            restored.activeTab = window.activeTab;
            restored.placement = window.placement;

            requests.push_back (std::move (restored));
        }
    }
    else
    {
        if (!options.newWindow)
        {
            for (const Location & location : prefs.tabs)
            {
                BrowserModel::Tab  tab;

                tab.location = location;
                request.tabs.push_back (std::move (tab));
            }

            request.placement = prefs.placement;
        }

        requests.push_back (std::move (request));
    }

    requests.front().openPath = options.openPath;
    requests.front().owner    = options.owner;

    return requests;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::PlanForwardedLaunch
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerWindowRequest CassoExplorerShell::PlanForwardedLaunch (const CassoExplorerLaunchOptions & options, const CassoExplorerPrefs::Placement & from)
{
    CassoExplorerWindowRequest  request;



    request.openPath  = options.openPath;
    request.owner     = options.owner;
    request.placement = from;
    request.cascade   = from.valid;

    return request;
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
//  CassoExplorerShell::GetPlainFolderName
//
//  The LocalizedResourceName of a folder's desktop.ini when it is text rather
//  than a reference to a module's string, which starts with an at sign.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerShell::GetPlainFolderName (const std::wstring & path)
{
    wchar_t       text[MAX_PATH] = {};
    std::wstring  ini            = path + L"\\desktop.ini";
    DWORD         length         = GetPrivateProfileStringW (L".ShellClassInfo", L"LocalizedResourceName", L"", text, ARRAYSIZE (text), ini.c_str());



    return (length > 0 && text[0] != L'@') ? std::wstring (text, length) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::GetLocalizedFolderName
//
//  SHGetLocalizedName returns the module and string id the desktop.ini
//  refers to; the string is loaded from that module as data, never run. A
//  desktop.ini whose name is plain text, as iCloud's is, has no module, and
//  its text is the name.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerShell::GetLocalizedFolderName (const std::wstring & path)
{
    HRESULT       hr                 = S_OK;
    wchar_t       module[MAX_PATH]   = {};
    wchar_t       expanded[MAX_PATH] = {};
    wchar_t       text[MAX_PATH]     = {};
    int           id                 = 0;
    HMODULE       library            = nullptr;
    int           length             = 0;
    std::wstring  name;



    hr = SHGetLocalizedName (path.c_str(), module, ARRAYSIZE (module), &id);
    BAIL_OUT_IF (FAILED (hr), S_OK);

    BAIL_OUT_IF (ExpandEnvironmentStringsW (module, expanded, ARRAYSIZE (expanded)) == 0, S_OK);

    library = LoadLibraryExW (expanded, nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    BAIL_OUT_IF (library == nullptr, S_OK);

    length = LoadStringW (library, (UINT) id, text, ARRAYSIZE (text));

    if (length > 0)
    {
        name.assign (text, (size_t) length);
    }

Error:
    if (library != nullptr)
    {
        FreeLibrary (library);
    }

    return name.empty() ? GetPlainFolderName (path) : name;
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
//  COM first, because the drag source and drop target in each browser
//  need OLE, and OLE initialization has to precede any window that registers
//  for drops. The known folders are read once here; each window's tree reads
//  them from its model after that. Then the windows the launch plans.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerShell::Initialize (HINSTANCE instance, const CassoExplorerLaunchOptions & options, const CassoExplorerPrefs & prefs, int showCommand)
{
    HRESULT                                  hr        = S_OK;
    HRESULT                                  hrSession = S_OK;
    std::vector<KnownFolderStore::Entry>     entries;
    std::vector<CassoExplorerSessionWindow>  session;
    std::vector<CassoExplorerWindowRequest>  requests;
    CassoExplorerWindow                    * window    = nullptr;
    bool                                     opened    = false;



    m_options     = options;
    m_prefs       = prefs;
    m_instance    = instance;
    m_showCommand = showCommand;
    m_baseDir     = AssetBootstrap::GetAssetBaseDirectory().wstring();

    hr = OleInitialize (nullptr);
    CHR (hr);

    m_oleInitialized = true;

    {
        KnownFolderStore  store (m_fs, m_baseDir);
        bool     pruned  = false;
        HRESULT  hrPrune = store.RemoveMissing (pruned);
        HRESULT  hrLoad  = store.Load (entries);

        IGNORE_RETURN_VALUE (hrPrune, S_OK);

        IGNORE_RETURN_VALUE (hrLoad, S_OK);
    }

    m_knownFolders = KnownFolderStore::ListRootFolders (m_fs, m_baseDir, entries);

    //  A session that cannot be read is no session.
    hrSession = CassoExplorerSession::Load (m_fs, m_baseDir, session);
    IGNORE_RETURN_VALUE (hrSession, S_OK);

    requests = PlanFirstLaunch (m_options, session, m_prefs);

    //  Saved once every window is open, rather than as each opens and before
    //  the rest of the session has.
    m_initializing = true;

    for (CassoExplorerWindowRequest & request : requests)
    {
        window = OpenWindow (std::move (request));
        opened = opened || window != nullptr;
    }

    m_initializing = false;
    CBR (opened);

    SaveSession();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::ConfigureBrowser
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerShell::ConfigureBrowser (CassoExplorerBrowser & browser)
{
    browser.GetTreeModel().SetKnownFolders (m_knownFolders);
    browser.GetTreeModel().SetDrives (GetDriveRoots());
    browser.GetTreeModel().SetLocalizedNameLookup (&CassoExplorerShell::GetLocalizedFolderName);
    browser.GetTreeModel().SetDirectoryProbe ([] (const std::wstring & path)
    {
        DWORD  attributes = GetFileAttributesW (path.c_str());

        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::OpenWindow
//
//  A write to an image signals any Casso with it mounted to reload it, and
//  the reply is sent to the window that wrote. CassoLink notifications are
//  posted to the newest window, which passes them to every window.
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerWindow * CassoExplorerShell::OpenWindow (CassoExplorerWindowRequest request)
{
    HRESULT                        hr      = S_OK;
    std::unique_ptr<WindowSlot>    slot    = std::make_unique<WindowSlot>();
    CassoExplorerWindow::Context   context;
    CassoExplorerWindow          * window  = nullptr;
    UINT_PTR                       owner   = (UINT_PTR) request.owner;   // a value, read before the request is moved
    BOOL                           marked  = FALSE;



    slot->watcher = std::make_unique<Win32FolderWatcher>();
    slot->intents = std::make_unique<Win32IntentChannel>();
    slot->browser = std::make_unique<CassoExplorerBrowser> (m_fs, m_fileIo);
    slot->actions = std::make_unique<CassoExplorerActions> (*slot->browser, m_fs);

    slot->browser->GetOperations().SetIntentChannel (slot->intents.get());
    ConfigureBrowser (*slot->browser);

    context.fs          = &m_fs;
    context.baseDir     = m_baseDir;
    context.titlePrefix = m_options.titlePrefix;
    context.request     = std::move (request);
    context.host        = this;
    context.watcher     = slot->watcher.get();
    context.links       = &m_links;
    context.linkFolder  = &m_linkFolder;
    context.running     = &m_running;

    slot->window = std::make_unique<CassoExplorerWindow> (*slot->browser, *slot->actions, m_prefs, std::move (context));

#if defined (_DEBUG)
    hr = slot->window->Open (m_instance, ComposeTitle (m_options.titlePrefix, GetCassoBuildInfo()), m_showCommand);
#else
    hr = slot->window->Open (m_instance, ComposeTitle (m_options.titlePrefix, L""), m_showCommand);
#endif
    CHR (hr);

    slot->intents->SetSender (slot->window->GetHwnd());

    if (owner != 0)
    {
        marked = SetPropW (slot->window->GetHwnd(), kOwnerProperty, (HANDLE) owner);
        CWR (marked);
    }

    window = slot->window.get();
    m_windows.push_back (std::move (slot));

    //  The launch's nCmdShow value applies to the first window; the rest open
    //  normally.
    m_showCommand = SW_SHOWNORMAL;

    if (!m_initializing)
    {
        SaveSession();
    }

Error:
    return window;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::GetWindowFor
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerWindow * CassoExplorerShell::GetWindowFor (HWND hwnd)
{
    CassoExplorerWindow  * found = nullptr;



    for (const std::unique_ptr<WindowSlot> & slot : m_windows)
    {
        if (!slot->closed && slot->window->GetHwnd() == hwnd)
        {
            found = slot->window.get();
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::ForEachWindow
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerShell::ForEachWindow (const std::function<void (CassoExplorerWindow & window)> & fn)
{
    for (const std::unique_ptr<WindowSlot> & slot : m_windows)
    {
        if (!slot->closed)
        {
            fn (*slot->window);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::CloseWindow
//
//  The last window closed is written to the session; one closed while others
//  stay open is left out of it and added to m_closed, so the last close
//  writes every window of a quick run. CassoLink notifications move to another window. The
//  window itself is released once its own message has returned.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerShell::CloseWindow (CassoExplorerWindow & window)
{
    WindowSlot  * closing = nullptr;
    bool          last    = false;



    for (const std::unique_ptr<WindowSlot> & slot : m_windows)
    {
        if (!slot->closed && slot->window.get() == &window)
        {
            closing = slot.get();
        }
    }

    if (closing == nullptr)
    {
        return;
    }

    last = std::count_if (m_windows.begin(), m_windows.end(), [] (const std::unique_ptr<WindowSlot> & slot) { return !slot->closed; }) == 1;

    //  The last close's session is final, as File > Exit's is: any save after
    //  it, with nothing left open, would write an empty session over it.
    if (last)
    {
        SaveSessionWith (GetClosedRun (m_closed, GetTickCount64()));
        m_exiting = true;
    }
    else
    {
        m_closed.push_back (ClosedWindow { GetTickCount64(), window.GetSessionWindow() });
    }

    closing->closed = true;
    window.CloseForApp();

    if (!last)
    {
        SaveSession();
    }

    ForEachWindow ([] (CassoExplorerWindow & open) { open.TakeLinkNotifications(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::ExitAll
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerShell::ExitAll()
{
    if (m_exiting)
    {
        return;
    }

    SaveSession();
    m_exiting = true;

    for (const std::unique_ptr<WindowSlot> & slot : m_windows)
    {
        if (!slot->closed)
        {
            slot->closed = true;
            slot->window->CloseForApp();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::SaveSession
//
//  Once every window is closing for the app, the session written then is final.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerShell::SaveSession()
{
    SaveSessionWith ({});
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::SaveSessionWith
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerShell::SaveSessionWith (const std::vector<CassoExplorerSessionWindow> & closed)
{
    HRESULT                                  hr      = S_OK;
    std::vector<CassoExplorerSessionWindow>  session = GetOpenSession();



    //  Not while the session's windows are opening: the first one open would
    //  write a session without the rest.
    if (m_exiting || m_initializing)
    {
        return;
    }

    session.insert (session.end(), closed.begin(), closed.end());

    hr = CassoExplorerSession::Save (m_fs, m_baseDir, session);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = m_prefs.Save (m_baseDir, m_fs);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::GetClosedRun
//
////////////////////////////////////////////////////////////////////////////////

std::vector<CassoExplorerSessionWindow> CassoExplorerShell::GetClosedRun (const std::vector<ClosedWindow> & closed, ULONGLONG nowMs)
{
    std::vector<CassoExplorerSessionWindow>  run;
    ULONGLONG                                later = nowMs;
    size_t                                   i     = closed.size();



    while (i > 0 && closed[i - 1].closedMs <= later && later - closed[i - 1].closedMs <= kCloseRunMs)
    {
        i--;
        later = closed[i].closedMs;
        run.insert (run.begin(), closed[i].window);
    }

    return run;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::GetOpenSession
//
////////////////////////////////////////////////////////////////////////////////

std::vector<CassoExplorerSessionWindow> CassoExplorerShell::GetOpenSession() const
{
    std::vector<CassoExplorerSessionWindow>  session;



    for (const std::unique_ptr<WindowSlot> & slot : m_windows)
    {
        if (!slot->closed)
        {
            session.push_back (slot->window->GetSessionWindow());
        }
    }

    return session;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::RunLaunch
//
//  Another launch's window steps down and right from the window in front,
//  and comes forward itself. A command line that does not parse opens
//  nothing; the launch that sent it had already checked it.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerShell::RunLaunch (const std::wstring & arguments)
{
    HRESULT                        hr      = S_OK;
    CassoExplorerLaunchOptions     options;
    CassoExplorerPrefs::Placement  from;
    CassoExplorerWindow          * front   = GetWindowFor (GetForegroundWindow());
    CassoExplorerWindow          * opened  = nullptr;



    hr = ParseArguments (SplitArguments (arguments), options);
    CHR (hr);

    for (const std::unique_ptr<WindowSlot> & slot : m_windows)
    {
        if (!slot->closed && front == nullptr)
        {
            front = slot->window.get();
        }
    }

    if (front != nullptr)
    {
        from = front->GetPlacement();
    }

    opened = OpenWindow (PlanForwardedLaunch (options, from));
    CBR (opened != nullptr);

    FrontWindow (opened->GetHwnd());

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::HasOpenWindows
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerShell::HasOpenWindows() const
{
    return std::any_of (m_windows.begin(), m_windows.end(), [] (const std::unique_ptr<WindowSlot> & slot) { return !slot->closed; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::ReleaseClosedWindows
//
//  Between messages, never from inside one of the window's own.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerShell::ReleaseClosedWindows()
{
    for (const std::unique_ptr<WindowSlot> & slot : m_windows)
    {
        if (slot->closed && slot->window != nullptr && slot->window->GetHwnd() != nullptr)
        {
            RemovePropW (slot->window->GetHwnd(), kOwnerProperty);
        }
    }

    std::erase_if (m_windows, [] (const std::unique_ptr<WindowSlot> & slot) { return slot->closed; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::GetWindow
//
//  The first open window, or null.
//
////////////////////////////////////////////////////////////////////////////////

HWND CassoExplorerShell::GetWindow() const
{
    HWND  hwnd = nullptr;



    for (const std::unique_ptr<WindowSlot> & slot : m_windows)
    {
        if (!slot->closed && hwnd == nullptr)
        {
            hwnd = slot->window->GetHwnd();
        }
    }

    return hwnd;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShell::RunMessageLoop
//
//  The loop runs while any window is open. The preferences are written once
//  it ends, whatever ended it.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerShell::RunMessageLoop()
{
    MSG      message = {};
    BOOL     got     = TRUE;
    HRESULT  hr      = S_OK;



    for (;;)
    {
        ReleaseClosedWindows();

        if (!HasOpenWindows())
        {
            break;
        }

        got = GetMessageW (&message, nullptr, 0, 0);

        if (got == 0 || got == -1)
        {
            break;
        }

        TranslateMessage (&message);
        DispatchMessageW (&message);
    }

    for (const std::unique_ptr<WindowSlot> & slot : m_windows)
    {
        slot->closed = true;
    }

    ReleaseClosedWindows();

    hr = m_prefs.Save (m_baseDir, m_fs);
    IGNORE_RETURN_VALUE (hr, S_OK);

    return (got == -1) ? 1 : (got == 0) ? (int) message.wParam : 0;
}





