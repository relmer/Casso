#include "Pch.h"

#include "AssetBootstrap.h"
#include "Cassque/CassqueShell.h"
#include "Cassque/Model/CassquePrefs.h"
#include "Config/Win32FileSystem.h"





////////////////////////////////////////////////////////////////////////////////
//
//  wCassqueMain
//
//  Cassque's process entry point, in the same order as the emulator's.
//
//  The name is Cassque's own rather than wWinMain because the emulator's entry
//  point lives in this same library, and a library keeps only one definition
//  of a symbol. Cassque.vcxproj maps the C runtime's call to wWinMain onto this
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

extern "C" int WINAPI wCassqueMain (
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_     LPWSTR    lpCmdLine,
    _In_     int       nCmdShow)
{
    //  What a refused command line exits with, as the emulator does.
    constexpr int  kRefusedCommandLineStatus = 2;



    HRESULT                        hr          = S_OK;
    int                            argc        = 0;
    LPWSTR *                       argv        = nullptr;
    std::vector<std::wstring>      arguments;
    CassqueLaunchOptions           options;
    CassquePrefs                   prefs;
    Win32FileSystem                fs;
    int                            exitCode    = 0;
    HWND                           existing    = nullptr;
    HRESULT                        hrOptional  = S_OK;
    std::unique_ptr<CassqueShell>  shell       = std::make_unique<CassqueShell>();



    (void) SetProcessDpiAwarenessContext (DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    SetNotifyFunction (&CassqueShell::NotifyUser);
    SetBreakpointFunction (&CassqueShell::ReportAssertion);

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

    hr = CassqueShell::ParseArguments (arguments, options);

    if (FAILED (hr))
    {
        CassqueShell::NotifyUser (options.refusal.c_str());
        exitCode = kRefusedCommandLineStatus;
        hr       = S_OK;
    }

    BAIL_OUT_IF (exitCode != 0, S_OK);

    existing = CassqueShell::FindWindowForOwner (options.owner);

    if (existing != nullptr)
    {
        CassqueShell::FrontWindow (existing);
    }

    BAIL_OUT_IF (existing != nullptr, S_OK);

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
