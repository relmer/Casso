#include "Pch.h"
#include "StartupTrace.h"

#include "AssetBootstrap.h"
#include "CommandLineParser.h"
#include "Core/TextEncoding.h"
#include "Config/GlobalUserPrefs.h"
#include "Config/UserConfigStore.h"
#include "Config/Win32FileSystem.h"
#include "Core/MachineConfig.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Core/PathResolver.h"
#include "DiskSettings.h"
#include "EmulatorShell.h"
#include "Core/MachineScanner.h"
#include "Shell/DiskMru.h"

#pragma comment(lib, "ole32.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  ParseCommandLine
//
//  Reads the GUI shell's few command-line options: which machine to boot,
//  disks to mount, and the CPU trace ring size.
//
//  THE GRAMMAR LIVES IN CORE, shared with CassoCli, so both prefixes work at
//  both executables and CassoCli's help can write these flags with whichever
//  prefix its reader asked for. A hand-rolled loop here used to compare wide
//  literals, took only the `--` form for everything but `--trace`, and could
//  not be reached by a test.
//
//  The conversion goes through the process's narrow code page, not UTF-8:
//  TextEncoding::WideToNarrow says why, and why that heals itself if the
//  process code page ever becomes UTF-8.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT ParseCommandLine (
    LPWSTR         lpCmdLine,
    wstring & outMachine,
    wstring & outDisk1,
    wstring & outDisk2,
    size_t  & outTraceCapacity)
{
    HRESULT                              hr   = S_OK;
    int                                  argc = 0;
    LPWSTR                             * argv = nullptr;
    std::vector<std::string>             narrow;
    std::vector<char *>                  pointers;
    CommandLineOptions::EmulatorOptions  parsed;



    outTraceCapacity = 0;

    argv = CommandLineToArgvW (lpCmdLine, &argc);
    CWRA (argv);

    for (int i = 0; i < argc; i++)
    {
        narrow.push_back (TextEncoding::WideToNarrow (argv[i]));
    }

    for (std::string & arg : narrow)
    {
        pointers.push_back (arg.data());
    }

    parsed = CommandLineParser::ParseEmulator ((int) pointers.size(), pointers.data());

    outMachine       = TextEncoding::NarrowToWide (parsed.machine);
    outDisk1         = TextEncoding::NarrowToWide (parsed.disk1);
    outDisk2         = TextEncoding::NarrowToWide (parsed.disk2);
    outTraceCapacity = parsed.traceEntries;

    LocalFree (argv);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadMachineConfig
//
//  Everything that has to happen before a machine can be built: find its
//  config, download any missing assets, resolve a boot disk, and load.
//
//  This runs BEFORE the shell exists, which is why it owns the startup
//  dialogs. Both of them are UI-thread work that must complete before the
//  emulator window appears, and neither can be deferred into the running app.
//
//  Missing assets are gathered into a SINGLE themed dialog rather than being
//  prompted for one at a time -- ROMs and, with prior consent, the optional
//  Disk ][ drive audio -- downloading them together on a worker thread with
//  live progress. Prompting per file turns a first run into a sequence of
//  modal dialogs.
//
//  ROM search paths put the install root that actually contained the resolved
//  machine folder FIRST, ahead of the generic search paths, so a machine
//  found in a development tree loads its ROMs from that same tree rather than
//  from an installed copy elsewhere.
//
//  Asset decisions are made strictly from the EMBEDDED default for this
//  machine plus the user's stored audio consent, not from the on-disk config,
//  so a user-edited config cannot change what gets downloaded.
//
//  A user dismissing a startup dialog -- declining the download, or closing
//  the boot-disk picker -- is a clean shutdown REQUEST, not a failure, so it
//  travels back through outUserExited rather than as a result code. Folding it
//  into the HRESULT would make a deliberate choice look like an error and
//  produce a dialog complaining about it.
//
////////////////////////////////////////////////////////////////////////////////

// `outUserExited` reports that the user dismissed one of the startup
// dialogs (declined the ROM download, or closed the boot-disk picker).
// That is a clean shutdown request rather than a failure, so it travels
// separately from the result code.
static HRESULT LoadMachineConfig (
    HINSTANCE           hInstance,
    const wstring     & machineName,
    wstring           & inoutDisk1Path,
    HWND                hwndParent,
    bool              & outUserExited,
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
    bool                foundConfig    = false;
    string              error;



    outUserExited = false;

    // Build search paths and find machine config
    searchPaths    = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(),
                                                     PathResolver::GetWorkingDirectory());
    configRelPath  = fs::path ("Machines") / fs::path (machineName).string()
                                           / (fs::path (machineName).string() + ".json");
    configPath     = PathResolver::FindFile (searchPaths, configRelPath);

    foundConfig = !configPath.empty();
    CBRN (foundConfig,
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
        bool             hasDisk    = false;
        string           hasDiskErr;
        GlobalUserPrefs  prefs;
        Win32FileSystem  fs_io;
        std::wstring     assetBase;
        HRESULT          hrLoad;
        HRESULT          hrSave;
        HRESULT          hrHasDisk       = AssetBootstrap::HasDiskController (hInstance, machineName,
                                                                              hasDisk, hasDiskErr);
        assetBase = AssetBootstrap::GetAssetBaseDirectory().wstring();

        IGNORE_RETURN_VALUE (hrHasDisk, S_OK);

        hrLoad = prefs.Load (assetBase, fs_io);
        IGNORE_RETURN_VALUE (hrLoad, S_OK);

        // Download any missing required ROMs (and, with consent, Disk ][
        // drive audio) up front in one themed dialog. Boot-disk selection
        // is owned solely by the boot-disk picker below.
        hr = AssetBootstrap::RunStartupDownloader (hInstance, machineName, hwndParent,
                                                   romSearchPaths, romDir, hasDisk,
                                                   prefs, outUserExited, error);

        hrSave = prefs.Save (assetBase, fs_io);
        IGNORE_RETURN_VALUE (hrSave, S_OK);

        CHRN (hr, format (L"Asset download failed:\n{}",
                          wstring (error.begin(), error.end())).c_str());

        // User chose Exit rather than downloading. Stop here with no
        // config; wWinMain shuts down quietly.
        BAIL_OUT_IF (outUserExited, S_OK);
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
            vector<DiskMru::Entry> mruPruned;
            HRESULT                hrPrefs    = S_OK;
            bool                   userClosed = false;

            diskDir = AssetBootstrap::GetDiskDirectory();

            hrPrefs = prefs.Load (AssetBootstrap::GetAssetBaseDirectory().wstring(), fs_prefs);
            IGNORE_RETURN_VALUE (hrPrefs, S_OK);

            mru       = DiskMru::FromUtf8 (prefs.recentDisks, prefs.recentDiskLoadedAt);
            mruPruned = mru.Prune ([] (const fs::path & p)
                                   {
                                       return fs::exists (p)
                                              && !AssetBootstrap::IsForeignCheckoutDisk (p);
                                   });

            AssetBootstrap::AppendSiblingDisksFromMruFolders (mruPruned);
            AssetBootstrap::AppendBundledDemoDisks (mruPruned);

            hr = AssetBootstrap::PromptBootDiskMru (
                hInstance, hwndParent, machineName, mruPruned, diskDir, prefs.activeTheme, downloaded, userClosed, error);

            CHRN (hr, format (L"Boot disk download failed:\n{}",
                              wstring (error.begin(), error.end())).c_str());

            // Closing the boot-disk picker is the same clean-shutdown
            // request as choosing Exit above.
            outUserExited = userClosed;
            BAIL_OUT_IF (userClosed, S_OK);

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

    // Apply the user's per-machine delta (e.g. a slot disabled in Settings >
    // Hardware) to the base config before building, so machine-level edits
    // persist across launches -- matching MachineManager::SwitchMachine for an
    // in-session reboot. Falls back to the base text on any merge failure.
    {
        Win32FileSystem  fsMerge;
        JsonValue        defaultJson;
        JsonValue        mergedJson;
        JsonParseError   parseErr;
        HRESULT          hrParse     = S_OK;
        HRESULT          hrMerge     = S_OK;
        UserConfigStore  storeMerge (AssetBootstrap::GetAssetBaseDirectory().wstring());
        hrMerge = E_FAIL;

        // The merge only runs when the parse produced something to merge, so
        // hrMerge starts failed rather than being tested unconditionally.
        hrParse = JsonParser::Parse (jsonText, defaultJson, parseErr);

        if (SUCCEEDED (hrParse))
        {
            hrMerge = storeMerge.Load (fs::path (machineName).string(), defaultJson, fsMerge, mergedJson);
        }

        if (SUCCEEDED (hrMerge) && mergedJson.GetType() == JsonType::Object)
        {
            jsonText = JsonWriter::Write (mergedJson);
        }
    }

    hr = MachineConfigLoader::Load (jsonText,
                                    fs::path (machineName).string(),
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
//  Process entry point. Everything here is startup ORDERING -- each step is
//  placed before something that depends on it.
//
//  DPI awareness is set FIRST, and programmatically rather than through a
//  manifest entry. Without per-monitor v2, Windows bitmap-scales the whole
//  window on a high-DPI display and every pixel the renderer draws comes out
//  blurry. Doing it in code keeps the manifest minimal; v2 has existed since
//  Windows 10 1703, below Casso's supported floor, so its failure path is
//  unreachable in practice.
//
//  The EHM notify and breakpoint hooks are installed before anything can fail,
//  so an error during startup is still reported through the UI.
//
//  The breakpoint hook exists because a failed assertion in a debug build
//  otherwise raises a raw int 3 -- fine under a debugger, but with none
//  attached it becomes a bare "Casso.exe has stopped working" with no detail
//  at all. Showing the assertion text and offering Abort / Retry / Ignore lets
//  the user quit, attach a debugger and break, or continue on EHM's normal
//  error path the way a release build would. With a debugger already attached
//  it breaks directly: the dialog would only obscure the stack you came for.
//
//  --trace is applied before the CPU thread starts, because both halves must
//  be in place first -- the ring has to be sized, and the crash-time filter
//  installed so an illegal opcode or any unhandled exception still flushes the
//  trace to a file on the way out.
//
//  Asset bootstrap runs before the machine loads, so a loose casso.exe with no
//  Machines/ or Themes/ folder extracts its embedded copies and has both a
//  machine to boot and chrome to render on a first launch. User-authored theme
//  directories are preserved -- only built-in ones are touched.
//
//  The shell is heap-allocated so the crash filter can reach it through a file
//  scope pointer, and so its destructor runs before the process exits rather
//  than during static teardown.
//
////////////////////////////////////////////////////////////////////////////////

int WINAPI wWinMain (
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_     LPWSTR    lpCmdLine,
    _In_     int       nCmdShow)
{
    HRESULT                         hr            = S_OK;
    wstring                         machineName;
    wstring                         disk1Path;
    wstring                         disk2Path;
    size_t                          traceCapacity = 0;
    int                             exitCode      = 0;
    bool                            userExited    = false;
    MachineConfig                   config;
    std::unique_ptr<EmulatorShell>  shell         = std::make_unique<EmulatorShell>();



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

    // Route every EHM user-facing error (CHRN / CBRN) through Casso's own
    // themed dialog rather than a system message box. Installed here, before
    // anything can fail, so a command-line or machine-config failure is
    // reported too; those happen before the shell exists, so the sink queues
    // them and the shell replays them once there is a window.
    StartupTrace::Stamp ("wWinMain entry");

    SetNotifyFunction (&EmulatorShell::NotifyUser);

    // Register a GUI assertion breakpoint. In debug builds a failed EHM
    // assertion (the *A macro variants, or a bare ASSERT) otherwise breaks
    // via a raw int 3 -- fine under a debugger, but with none attached it
    // becomes a silent "Casso.exe has stopped working" WER crash with no
    // detail. Instead surface the assertion text and let the user choose
    // Abort (quit) / Retry (break, e.g. after attaching a debugger) /
    // Ignore (continue on EHM's normal error path, as a release build would).
    SetBreakpointFunction ([] (const wchar_t * message)
    {
        // With a debugger attached, break at the assertion site as before --
        // the dialog would only get in the way of the stack you came for.
        if (IsDebuggerPresent())
        {
            __debugbreak();
        }
        else
        {
            std::wstring text = L"An internal assertion failed:\n\n";
            text += (message != nullptr && message[0] != L'\0') ? message : L"(no detail)";
            text += L"\n\n"
                    L"Abort  = quit now\n"
                    L"Retry  = break (attach a debugger first to inspect)\n"
                    L"Ignore = try to continue";

            int choice = MessageBoxW (NULL, text.c_str(), L"Casso \x2014 assertion failed",
                                      MB_ABORTRETRYIGNORE | MB_ICONERROR | MB_DEFBUTTON1 | MB_TASKMODAL);

            if (choice == IDABORT)
            {
                TerminateProcess (GetCurrentProcess(), 3);
            }
            else if (choice == IDRETRY)
            {
                __debugbreak();   // no-op crash if still no debugger; lets you attach one
            }

            // IDIGNORE: fall through -- EHM continues on its normal error path.
        }
    });

    // Parse command line
    StartupTrace::Stamp ("hooks installed");

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
        HRESULT  hrBoot   = AssetBootstrap::EnsureMachineConfigs (hInstance);
        HRESULT  hrThemes = S_OK;
        HRESULT  hrSounds = S_OK;



        IGNORE_RETURN_VALUE (hrBoot, S_OK);

        // Extract the three built-in UI themes alongside the
        // machine configs so the very first launch has chrome to
        // render. User-authored Themes/<MyTheme>/ entries are
        // preserved — the planner only ever touches built-in dirs.
        hrThemes = AssetBootstrap::EnsureThemes (hInstance);
        IGNORE_RETURN_VALUE (hrThemes, S_OK);

        // Extract the ImageWriter II mechanical sound set next to the machine
        // configs and themes so the printer preview has audio on first launch.
        hrSounds = AssetBootstrap::EnsureImageWriterSounds (hInstance);
        IGNORE_RETURN_VALUE (hrSounds, S_OK);
    }

    StartupTrace::Stamp ("asset bootstrap (configs/themes/sounds)");

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

    // Resolve the requested machine (from --machine or last-selected prefs)
    // to a canonical on-disk name. The filesystem is case-insensitive, so a
    // mis-cased --machine value like "apple2e" still loads its config -- but
    // FindRomSpec and MachineDisplayName match names exactly, so a lowercase
    // name would report every per-machine ROM missing. Canonicalize against
    // the scan so downstream lookups agree. An unmatched / empty request
    // falls back to Apple //e (else the first discovered machine, else the
    // Apple2e literal) so the LoadMachineConfig flow can still offer the ROM
    // / sample-disk downloads instead of bailing with a dead-end MessageBox.
    {
        constexpr std::wstring_view  s_kPreferredDefaultMachine = L"Apple2e";

        vector<fs::path> scanPaths = PathResolver::BuildSearchPaths (
            PathResolver::GetExecutableDirectory(),
            PathResolver::GetWorkingDirectory());

        vector<MachineInfo> discovered = MachineScanner::Scan (
            scanPaths,
            &MachineScanner::ListDirectory,
            &MachineScanner::ReadFile);

        machineName = MachineScanner::SelectCanonical (
            discovered, machineName, s_kPreferredDefaultMachine);
    }

    StartupTrace::Stamp ("machine scan + canonicalize");

    // Load machine configuration. A user who dismissed one of the
    // startup dialogs wants out, so exit cleanly without a follow-up
    // error MessageBox.
    hr = LoadMachineConfig (hInstance, machineName, disk1Path, nullptr, userExited, config);
    CHR (hr);
    BAIL_OUT_IF (userExited, S_OK);

    StartupTrace::Stamp ("LoadMachineConfig (roms/prefs/disk pre-flight)");

    // Initialize emulator. EmulatorShell::Initialize records the
    // chosen machine into GlobalUserPrefs.lastSelectedMachine and
    // flushes it to UserPrefs.json so the next launch boots the
    // same machine without --machine.
    hr = shell->Initialize (hInstance, machineName, config,
                            fs::path (disk1Path).string(),
                            fs::path (disk2Path).string());
    CHRN (hr, L"Failed to initialize emulator");

    // Run message loop
    StartupTrace::Stamp ("shell->Initialize returned");
    StartupTrace::Dump();

    exitCode = shell->RunMessageLoop();

    // --trace graceful-exit dump. No-op (one-shot guard) if a crash
    // already flushed the ring via TraceCrashFilter.
    if (shell->IsTracing())
    {
        shell->DumpTrace (L"exit");
    }

    // Success falls into the same tail the bails jump to: clearing the trace
    // back-pointer must happen exactly once, on every path, before the shell
    // it points at is destroyed.
Error:
    s_pTraceShell = nullptr;

    // exitCode is still 0 for any bail, including BAIL_OUT_IF (userExited) --
    // dismissing a startup dialog is a clean exit, not a failure.
    if (FAILED (hr))
    {
        exitCode = 1;
    }

    return exitCode;
}





