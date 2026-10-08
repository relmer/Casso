#include "Pch.h"

#include "Core/ThreadName.h"

#include "AssetBootstrap.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "Config/Win32FileSystem.h"





////////////////////////////////////////////////////////////////////////////////
//
//  wCassoExplorerMain
//
//  CassoExplorer's process entry point, in the same order as the emulator's.
//
//  The name is CassoExplorer's own rather than wWinMain because the emulator's entry
//  point lives in this same library, and a library keeps only one definition
//  of a symbol. CassoExplorer.vcxproj maps the C runtime's call to wWinMain onto this
//  function with /ALTERNATENAME, which is also why it has C linkage.
//
//  DPI awareness first, for the reason the emulator gives: without per-monitor
//  v2 Windows bitmap-scales the window. The EHM hooks next, so a failure from
//  here on is reported. A second launch for the same emulator fronts the
//  window that is already serving it and exits before doing any other work.
//
//  Themes are extracted as the emulator extracts them, from the same embedded
//  copies, so whichever program starts first leaves the same files on disk.
//  Neither that nor the preferences is fatal: a browser with default settings
//  is still a browser.
//
////////////////////////////////////////////////////////////////////////////////

extern "C" int WINAPI wCassoExplorerMain (
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_     LPWSTR    lpCmdLine,
    _In_     int       nCmdShow)
{
    //  What a refused command line exits with, as the emulator does.
    constexpr int  kRefusedCommandLineStatus = 2;



    HRESULT                                hr         = S_OK;
    int                                    argc       = 0;
    LPWSTR                               * argv       = nullptr;
    std::vector<std::wstring>              arguments;
    CassoExplorerLaunchOptions             options;
    CassoExplorerPrefs                     prefs;
    Win32FileSystem                        fs;
    int                                    exitCode   = 0;
    HWND                                   existing   = nullptr;
    HRESULT                                hrOptional = S_OK;
    std::unique_ptr<CassoExplorerShell>    shell      = std::make_unique<CassoExplorerShell>();



    hr = ThreadName::SetForCurrentThread (L"Casso Explorer UI");
    IGNORE_RETURN_VALUE (hr, S_OK);

    (void) SetProcessDpiAwarenessContext (DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    SetNotifyFunction (&CassoExplorerShell::NotifyUser);
    SetBreakpointFunction (&CassoExplorerShell::ReportAssertion);

    //  lpCmdLine arrives as one string. The full command line is split by the
    //  runtime's quoting rules instead, and the program name dropped.
    UNREFERENCED_PARAMETER (hPrevInstance);
    UNREFERENCED_PARAMETER (lpCmdLine);

    argv = CommandLineToArgvW (GetCommandLineW(), &argc);
    CWR (argv != nullptr);

    if (argc > 1)
    {
        arguments.assign (argv + 1, argv + argc);
    }

    LocalFree (argv);
    argv = nullptr;

    hr = CassoExplorerShell::ParseArguments (arguments, options);

    if (FAILED (hr))
    {
        CassoExplorerShell::NotifyUser (options.refusal.c_str());
        exitCode = kRefusedCommandLineStatus;
        hr       = S_OK;
    }

    BAIL_OUT_IF (exitCode != 0, S_OK);

    existing = CassoExplorerShell::FindWindowForOwner (options.owner);

    if (existing != nullptr)
    {
        CassoExplorerShell::FrontWindow (existing);

        //  The window open already takes the path; a full one, since its
        //  process has its own current folder.
        if (!options.openPath.empty())
        {
            std::wstring    full   (32768, L'\0');
            DWORD           length = GetFullPathNameW (options.openPath.c_str(), (DWORD) full.size(), full.data(), nullptr);
            COPYDATASTRUCT  copy   = {};
            DWORD_PTR       result = 0;

            full.resize (length < full.size() ? length : 0);

            copy.dwData = CassoExplorerWindow::kOpenPathCopyId;
            copy.cbData = (DWORD) (full.size() * sizeof (wchar_t));
            copy.lpData = full.data();

            if (!full.empty())
            {
                SendMessageTimeoutW (existing, WM_COPYDATA, 0, (LPARAM) &copy, SMTO_ABORTIFHUNG, 5000, &result);
            }
        }
    }

    BAIL_OUT_IF (existing != nullptr, S_OK);

    //  The shell's own dialogs this process opens -- a copy's conflicts and
    //  progress -- follow the system's dark mode, as Explorer's do. Windows
    //  exports the switch by ordinal only (uxtheme 135, SetPreferredAppMode);
    //  missing, the dialogs stay light, as before.
    {
        using SetPreferredAppModeFn = int (WINAPI *) (int);

        constexpr int  kAllowDark = 1;

        HMODULE                uxtheme = LoadLibraryExW (L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        SetPreferredAppModeFn  setMode = (uxtheme != nullptr) ? (SetPreferredAppModeFn) GetProcAddress (uxtheme, MAKEINTRESOURCEA (135)) : nullptr;

        if (setMode != nullptr)
        {
            setMode (kAllowDark);
        }
    }

    hrOptional = AssetBootstrap::EnsureThemes (hInstance);
    IGNORE_RETURN_VALUE (hrOptional, S_OK);

    hrOptional = prefs.Load (AssetBootstrap::GetAssetBaseDirectory().wstring(), fs);
    IGNORE_RETURN_VALUE (hrOptional, S_OK);

    hr = shell->Initialize (hInstance, options, prefs, nCmdShow);
    CHR (hr);

    exitCode = shell->RunMessageLoop();

Error:
    if (FAILED (hr))
    {
        exitCode = 1;
    }

    shell.reset();

    return exitCode;
}
