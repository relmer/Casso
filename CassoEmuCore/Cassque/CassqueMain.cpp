#include "Pch.h"

#include "AssetBootstrap.h"
#include "Cassque/CassqueShell.h"
#include "Cassque/Model/CassquePrefs.h"
#include "Config/Win32FileSystem.h"





////////////////////////////////////////////////////////////////////////////////
//
//  wmain
//
//  Cassque's process entry point, in the same order as the emulator's.
//
//  WMAIN RATHER THAN wWinMain, because the emulator's wWinMain lives in this
//  same library. A library holding two definitions of one symbol keeps only
//  the first, so both executables would link the emulator's. Cassque links
//  with the wide console startup and the Windows subsystem, which gives it
//  wmain with no console and the arguments already split.
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

int wmain (int argc, wchar_t * argv[])
{
    //  What a refused command line exits with, as the emulator does.
    constexpr int  kRefusedCommandLineStatus = 2;



    HRESULT                        hr          = S_OK;
    HINSTANCE                      instance    = GetModuleHandleW (nullptr);
    std::vector<std::wstring>      arguments;
    CassqueLaunchOptions           options;
    CassquePrefs                   prefs;
    Win32FileSystem                fs;
    STARTUPINFOW                   startup     = { sizeof (startup) };
    int                            showCommand = SW_SHOWDEFAULT;
    int                            exitCode    = 0;
    HWND                           existing    = nullptr;
    HRESULT                        hrOptional  = S_OK;
    std::unique_ptr<CassqueShell>  shell       = std::make_unique<CassqueShell>();



    (void) SetProcessDpiAwarenessContext (DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    SetNotifyFunction (&CassqueShell::NotifyUser);
    SetBreakpointFunction (&CassqueShell::ReportAssertion);

    if (argc > 1)
    {
        arguments.assign (argv + 1, argv + argc);
    }

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

    hrOptional = AssetBootstrap::EnsureThemes (instance);
    IGNORE_RETURN_VALUE (hrOptional, S_OK);

    hrOptional = prefs.Load (AssetBootstrap::GetAssetBaseDirectory().wstring(), fs);
    IGNORE_RETURN_VALUE (hrOptional, S_OK);

    GetStartupInfoW (&startup);

    if ((startup.dwFlags & STARTF_USESHOWWINDOW) != 0)
    {
        showCommand = startup.wShowWindow;
    }

    hr = shell->Initialize (instance, options, prefs, showCommand);
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
