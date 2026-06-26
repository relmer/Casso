#include "Pch.h"

#include "AssetBootstrap.h"
#include "Config/GlobalUserPrefs.h"
#include "Config/UserConfigStore.h"
#include "Config/Win32FileSystem.h"
#include "Core/MachineConfig.h"
#include "Core/PathResolver.h"
#include "DiskSettings.h"
#include "EmulatorShell.h"
#include "Core/MachineScanner.h"
#include "Shell/DiskMru.h"

#pragma comment(lib, "ole32.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  ParseTraceSize
//
//  Parse a --trace size override like "20M", "500000", or "2G" into a
//  ring-buffer entry count. Suffixes K/M/G multiply by 1e3/1e6/1e9.
//
////////////////////////////////////////////////////////////////////////////////

static size_t ParseTraceSize (const wstring & text)
{
    unsigned long long  value = 0;
    wchar_t           * end   = nullptr;



    if (text.empty())
    {
        return 0;
    }

    value = wcstoull (text.c_str(), &end, 10);

    if (end != nullptr && *end != L'\0')
    {
        switch (towupper (*end))
        {
            case L'K':  value *= 1000ull;        break;
            case L'M':  value *= 1000000ull;     break;
            case L'G':  value *= 1000000000ull;  break;
            default:                             break;
        }
    }

    return (size_t) value;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ParseCommandLine
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT ParseCommandLine (
    LPWSTR         lpCmdLine,
    wstring & outMachine,
    wstring & outDisk1,
    wstring & outDisk2,
    size_t  & outTraceCapacity)
{
    // ~1 minute of emulated 6502 time (~340K instructions/sec). Each ring
    // entry is ~10 bytes, so the default ring is ~200 MB.
    static constexpr size_t  s_kTraceDefaultEntries = 20000000;

    HRESULT   hr   = S_OK;
    int       argc = 0;
    LPWSTR  * argv = nullptr;



    outTraceCapacity = 0;

    argv = CommandLineToArgvW (lpCmdLine, &argc);
    CWRA (argv);

    for (int i = 0; i < argc; i++)
    {
        wstring arg (argv[i]);

        if (arg == L"--machine" && i + 1 < argc)
        {
            outMachine = argv[++i];
        }
        else if (arg == L"--disk1" && i + 1 < argc)
        {
            outDisk1 = argv[++i];
        }
        else if (arg == L"--disk2" && i + 1 < argc)
        {
            outDisk2 = argv[++i];
        }
        else if (arg == L"--trace" || arg == L"/trace")
        {
            outTraceCapacity = s_kTraceDefaultEntries;

            // Optional space-separated numeric override: "--trace 50M".
            if (i + 1 < argc && iswdigit (argv[i + 1][0]))
            {
                outTraceCapacity = ParseTraceSize (argv[++i]);
            }
        }
        else if (arg.rfind (L"--trace=", 0) == 0 || arg.rfind (L"/trace=", 0) == 0)
        {
            outTraceCapacity = ParseTraceSize (arg.substr (arg.find (L'=') + 1));
        }
    }

    LocalFree (argv);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadMachineConfig
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT LoadMachineConfig (
    HINSTANCE           hInstance,
    const wstring     & machineName,
    wstring           & inoutDisk1Path,
    HWND                hwndParent,
    MachineConfig     & outConfig)
{
    HRESULT             hr             = S_OK;
    vector<fs::path>    searchPaths;
    fs::path            configRelPath;
    fs::path            configPath;
    ifstream            configFile;
    bool                configGood     = false;
    stringstream        ss;
    string              jsonText;
    vector<fs::path>    romSearchPaths;
    fs::path            romDir;
    fs::path            diskDir;
    wstring             savedDisk;
    HRESULT             hrSaved        = S_OK;
    string              error;


    // Build search paths and find machine config
    searchPaths    = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(),
                                                     PathResolver::GetWorkingDirectory());
    configRelPath  = fs::path ("Machines") / fs::path (machineName).string()
                                           / (fs::path (machineName).string() + ".json");
    configPath     = PathResolver::FindFile (searchPaths, configRelPath);

    CBRN (!configPath.empty(),
          format (L"Unknown machine '{}'. Config file not found.\n"
                  L"Searched for '{}' in exe directory, current directory, and parent directories.",
                  machineName,
                  configRelPath.wstring()).c_str());

    // Build ROM search paths — prioritize the install root that
    // contains the per-machine Machines/<Name>/ folder we just
    // resolved (parent of "Machines"), then fall back to the
    // generic search paths.
    romSearchPaths.push_back (configPath.parent_path().parent_path().parent_path());

    for (const auto & p : searchPaths)
    {
        if (p != romSearchPaths[0])
        {
            romSearchPaths.push_back (p);
        }
    }

    // Pre-flight: detect everything missing (ROMs + optional Disk II
    // drive audio) and present a SINGLE themed dialog that downloads
    // it all on a worker thread with live progress. Decisions for the
    // download set are made strictly from the embedded default for
    // `machineName` and the user's prior audio-consent choice.
    romDir = AssetBootstrap::GetAssetBaseDirectory();

    {
        bool             hasDisk         = false;
        string           hasDiskErr;
        HRESULT          hrHasDisk       = AssetBootstrap::HasDiskController (hInstance, machineName,
                                                                              hasDisk, hasDiskErr);
        GlobalUserPrefs  prefs;
        Win32FileSystem  fs_io;
        std::wstring     assetBase       = AssetBootstrap::GetAssetBaseDirectory().wstring();
        wstring          downloadedDisk;
        fs::path         bootDiskDir     = AssetBootstrap::GetDiskDirectory();
        bool             offerBootDisk   = false;
        HRESULT          hrLoad;
        HRESULT          hrSave;

        IGNORE_RETURN_VALUE (hrHasDisk, S_OK);

        hrLoad = prefs.Load (assetBase, fs_io);
        IGNORE_RETURN_VALUE (hrLoad, S_OK);

        // Read the per-machine saved disk path up front so we can ask
        // the unified downloader to also fetch a stock boot disk on
        // first launch (no --disk1, no remembered disk, machine has a
        // Disk ][ controller). Doing this here keeps the entire
        // first-launch experience inside one themed dialog.
        if (inoutDisk1Path.empty())
        {
            UserConfigStore  store (assetBase);

            hrSaved = DiskSettings::ReadSavedDiskPath (store, fs_io, 0, machineName, savedDisk);
            IGNORE_RETURN_VALUE (hrSaved, S_OK);

            if (!savedDisk.empty() && !fs::exists (fs::path (savedDisk)))
            {
                HRESULT hrClear = DiskSettings::WriteSavedDiskPath (
                    store, fs_io, 0, machineName, wstring());
                IGNORE_RETURN_VALUE (hrClear, S_OK);
                savedDisk.clear();
            }

            offerBootDisk = savedDisk.empty() && hasDisk;
        }

        hr = AssetBootstrap::RunStartupDownloader (hInstance, machineName, hwndParent,
                                                   romSearchPaths, romDir, hasDisk,
                                                   offerBootDisk, bootDiskDir,
                                                   prefs, downloadedDisk, error);

        hrSave = prefs.Save (assetBase, fs_io);
        IGNORE_RETURN_VALUE (hrSave, S_OK);

        BAIL_OUT_IF (hr == S_FALSE, S_FALSE);
        CHRN (hr, format (L"Asset download failed:\n{}",
                          wstring (error.begin(), error.end())).c_str());

        // If the unified downloader pulled a boot disk, treat it as
        // disk1 so the legacy picker downstream short-circuits.
        if (!downloadedDisk.empty())
        {
            inoutDisk1Path = downloadedDisk;
        }
    }

    // Boot-disk pre-flight: if the user didn't pass --disk1 and there's
    // no remembered disk for this machine in UserPrefs (or the
    // remembered path no longer points at a real file), and the
    // machine has a Disk ][ controller, offer to download a stock
    // Apple system master disk. Without this the user just stares at
    // a spinning drive forever after first launch.
    if (inoutDisk1Path.empty())
    {
        Win32FileSystem  fs_io;
        UserConfigStore  store (AssetBootstrap::GetAssetBaseDirectory().wstring());

        hrSaved = DiskSettings::ReadSavedDiskPath (store, fs_io, 0, machineName, savedDisk);
        IGNORE_RETURN_VALUE (hrSaved, S_OK);

        // Treat a remembered-but-missing disk the same as "no
        // remembered disk", and clear the stale value so we don't keep
        // tripping over it on every launch.
        if (!savedDisk.empty() && !fs::exists (fs::path (savedDisk)))
        {
            HRESULT hrClear = DiskSettings::WriteSavedDiskPath (
                store, fs_io, 0, machineName, wstring());
            IGNORE_RETURN_VALUE (hrClear, S_OK);
            savedDisk.clear();
        }

        if (savedDisk.empty())
        {
            wstring                downloaded;
            GlobalUserPrefs        prefs;
            Win32FileSystem        fs_prefs;
            DiskMru                mru;
            vector<fs::path>       mruExisting;
            HRESULT                hrPrefs    = S_OK;
            bool                   userClosed = false;

            diskDir = AssetBootstrap::GetDiskDirectory();

            hrPrefs = prefs.Load (AssetBootstrap::GetAssetBaseDirectory().wstring(), fs_prefs);
            IGNORE_RETURN_VALUE (hrPrefs, S_OK);

            mru         = DiskMru::FromUtf8 (prefs.recentDisks);
            mruExisting = mru.Prune ([] (const fs::path & p) { return fs::exists (p); });

            hr = AssetBootstrap::PromptBootDiskMru (
                hInstance, hwndParent, machineName, mruExisting, diskDir, prefs.activeTheme, downloaded, userClosed, error);

            CHRN (hr, format (L"Boot disk download failed:\n{}",
                              wstring (error.begin(), error.end())).c_str());

            if (userClosed)
            {
                hr = S_FALSE;
                goto Error;
            }

            if (!downloaded.empty())
            {
                inoutDisk1Path = downloaded;
            }

            hr = S_OK;
        }
    }

    // Now load the on-disk config file and parse it
    configFile.open (configPath);
    configGood = configFile.good();
    CBRN (configGood,
          format (L"Cannot open machine config:\n{}",
                  configPath.wstring()).c_str());

    ss << configFile.rdbuf();
    jsonText = ss.str();

    hr = MachineConfigLoader::Load (jsonText,
                                    fs::path (machineName).string (),
                                    romSearchPaths,
                                    outConfig,
                                    error);
    CHRN (hr, format (L"Failed to load machine config:\n{}",
                      wstring (error.begin(), error.end())).c_str());

    // Validate disk images
    if (!inoutDisk1Path.empty())
    {
        fs::path    diskPath  = fs::path (inoutDisk1Path);
        bool        diskGood  = fs::exists (diskPath);

        CBRN (diskGood,
              format (L"Disk image not found:\n{}", inoutDisk1Path).c_str());

        // Format-specific validation (size, header, magic) happens
        // inside DiskImageStore::Mount, which dispatches on the file
        // extension (.dsk / .do / .po / .woz / ...). Don't pre-flight
        // a size check here -- WOZ and ProDOS images aren't 143360
        // bytes and used to be rejected as "not a valid .dsk file".
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Trace crash handler
//
//  When --trace is active, an unhandled exception (including the
//  illegal-opcode __debugbreak with no debugger attached) dumps the CPU
//  execution-trace ring to a file before the process dies. The filter
//  runs on the faulting thread -- for a CPU fault that is the CPU thread
//  that owns the ring, so the dump is race-free. s_pTraceShell is set
//  once the shell exists; DumpTrace is one-shot and self-guards.
//
////////////////////////////////////////////////////////////////////////////////

static EmulatorShell * s_pTraceShell = nullptr;

static LONG WINAPI TraceCrashFilter (EXCEPTION_POINTERS * info)
{
    UNREFERENCED_PARAMETER (info);

    if (s_pTraceShell != nullptr)
    {
        s_pTraceShell->DumpTrace (L"crash");
    }

    return EXCEPTION_EXECUTE_HANDLER;
}




////////////////////////////////////////////////////////////////////////////////
//
//  wWinMain
//
////////////////////////////////////////////////////////////////////////////////

int WINAPI wWinMain (
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_     LPWSTR    lpCmdLine,
    _In_     int       nCmdShow)
{
    HRESULT                          hr = S_OK;
    wstring                          machineName;
    wstring                          disk1Path;
    wstring                          disk2Path;
    size_t                           traceCapacity = 0;
    int                              exitCode      = 0;
    MachineConfig                    config;
    std::unique_ptr<EmulatorShell>   shell = std::make_unique<EmulatorShell>();



    UNREFERENCED_PARAMETER (hPrevInstance);
    UNREFERENCED_PARAMETER (nCmdShow);

    // Per-monitor DPI awareness v2. Without this Windows bitmap-scales
    // the entire window up on high-DPI displays, which makes every DX
    // pixel we render blurry. Setting it programmatically (rather than
    // via a manifest entry) keeps the manifest minimal. v2 is available
    // on Windows 10 1703+, which is below Casso's supported floor, so
    // the failure path is unreachable in practice.
    (void) SetProcessDpiAwarenessContext (DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

#ifdef _DEBUG
    // Enable frequent heap validation to catch corruption near its source
    // _CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
#endif

    // Register GUI error notification so EHM errors show a MessageBox
    SetNotifyFunction ([] (const wchar_t * message)
    {
        MessageBoxW (NULL, message, L"Casso emulator", MB_OK | MB_ICONERROR);
    });

    // Parse command line
    hr = ParseCommandLine (lpCmdLine, machineName, disk1Path, disk2Path, traceCapacity);
    CHR (hr);

    // --trace: size the CPU ring and install the crash-time dump filter
    // before the CPU thread starts, so an illegal-opcode/__debugbreak or
    // any unhandled exception flushes the trace to a file on the way out.
    if (traceCapacity > 0)
    {
        shell->SetTraceCapacity (traceCapacity);
        SetUnhandledExceptionFilter (TraceCrashFilter);
        s_pTraceShell = shell.get();
    }

    // Make sure a Machines/ directory exists with at least the stock
    // JSON configs (extracts embedded resources on first run if the
    // user is running a loose casso.exe with no Machines/ folder).
    {
        HRESULT hrBoot   = AssetBootstrap::EnsureMachineConfigs (hInstance);
        HRESULT hrThemes = S_OK;



        IGNORE_RETURN_VALUE (hrBoot, S_OK);

        // Extract the three built-in UI themes alongside the
        // machine configs so the very first launch has chrome to
        // render. User-authored Themes/<MyTheme>/ entries are
        // preserved — the planner only ever touches built-in dirs.
        hrThemes = AssetBootstrap::EnsureThemes (hInstance);
        IGNORE_RETURN_VALUE (hrThemes, S_OK);
    }

    // Resolve machine name: command line > UserPrefs.json lastSelectedMachine > first discovered.
    if (machineName.empty())
    {
        GlobalUserPrefs   earlyPrefs;
        Win32FileSystem   earlyFs;
        std::wstring      assetBaseDir = AssetBootstrap::GetAssetBaseDirectory().wstring();
        HRESULT           hrLoad;

        hrLoad = earlyPrefs.Load (assetBaseDir, earlyFs);
        IGNORE_RETURN_VALUE (hrLoad, S_OK);
        machineName.assign (earlyPrefs.lastSelectedMachine.begin(),
                            earlyPrefs.lastSelectedMachine.end());
    }

    if (machineName.empty() ||
        PathResolver::FindFile (
            PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(),
                                            PathResolver::GetWorkingDirectory()),
            fs::path ("Machines") / fs::path (machineName).string()
                                  / (fs::path (machineName).string() + ".json")).empty())
    {
        // Legacy Win32 `MachinePickerDialog` is retired (FR-027). At
        // startup we deterministically pick a sensible default machine
        // from `MachineScanner::Scan`; the user can switch later via
        // the Settings panel. Apple //e is the modern Apple II family
        // member most users will want, so prefer it if discovered.
        // Otherwise fall back to the first scan result. If nothing
        // was discovered at all (Machines/ missing, asset bootstrap
        // wiped between runs) we still default to Apple2e so the
        // downstream LoadMachineConfig flow gets to offer the ROM /
        // sample-disk downloads instead of bailing with a dead-end
        // error MessageBox.
        constexpr std::wstring_view  s_kPreferredDefaultMachine = L"Apple2e";

        vector<fs::path> scanPaths = PathResolver::BuildSearchPaths (
            PathResolver::GetExecutableDirectory(),
            PathResolver::GetWorkingDirectory());

        vector<MachineInfo> discovered = MachineScanner::Scan (
            scanPaths,
            &MachineScanner::ListDirectory,
            &MachineScanner::ReadFile);

        machineName.clear();
        for (const MachineInfo & info : discovered)
        {
            if (info.fileName == s_kPreferredDefaultMachine)
            {
                machineName = info.fileName;
                break;
            }
        }
        if (machineName.empty() && !discovered.empty())
        {
            machineName = discovered.front().fileName;
        }
        if (machineName.empty())
        {
            machineName = std::wstring (s_kPreferredDefaultMachine);
        }
    }

    // Load machine configuration. S_FALSE here means the user
    // declined the missing-ROM download prompt — exit cleanly
    // without a follow-up error MessageBox.
    hr = LoadMachineConfig (hInstance, machineName, disk1Path, nullptr, config);
    CHR (hr);
    BAIL_OUT_IF (hr == S_FALSE, S_OK);

    // Initialize emulator. EmulatorShell::Initialize records the
    // chosen machine into GlobalUserPrefs.lastSelectedMachine and
    // flushes it to UserPrefs.json so the next launch boots the
    // same machine without --machine.
    hr = shell->Initialize (hInstance, machineName, config,
                            fs::path (disk1Path).string(),
                            fs::path (disk2Path).string());
    CHRN (hr, L"Failed to initialize emulator");

    // Run message loop
    exitCode = shell->RunMessageLoop();

    // --trace graceful-exit dump. No-op (one-shot guard) if a crash
    // already flushed the ring via TraceCrashFilter.
    if (shell->IsTracing())
    {
        shell->DumpTrace (L"exit");
    }

    s_pTraceShell = nullptr;
    return exitCode;

Error:
    s_pTraceShell = nullptr;
    return FAILED (hr) ? 1 : 0;
}





