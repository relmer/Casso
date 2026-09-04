#include "Pch.h"

#include "AssetBootstrap.h"
#include "CommandLineHelp.h"
#include "CommandLineParser.h"
#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
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
#include "Ui/Chrome/CassoTheme.h"
#include "Window/DxuiMessageBox.h"

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
//  not be reached by a test. Everything this function still does is the
//  platform edge: the wide-to-narrow conversion, and one Windows quirk.
//
//  THE QUIRK IS AN EMPTY COMMAND LINE. Given nothing, CommandLineToArgvW hands
//  back the path of the running executable -- unquoted, so an install under
//  `C:\Program Files` comes back as the TWO arguments `C:\Program` and
//  `Files\...\Casso.exe`. Afterwards that is indistinguishable from arguments a
//  person typed. It cost nothing while an unrecognized argument was dropped;
//  now that one is refused, an ordinary double-click would open a dialog
//  complaining about an argument nobody passed and start no emulator. So the
//  empty line is answered here, before Windows gets a chance to fill it in.
//  Measured, not assumed: `argc` comes back 2 for an empty string, and 1 with
//  an empty argument for a line of only spaces.
//
//  The conversion goes through the process's narrow code page, not UTF-8:
//  TextEncoding::WideToNarrow says why, and why that heals itself if the
//  process code page ever becomes UTF-8.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT ParseCommandLine (
    LPWSTR                                lpCmdLine,
    CommandLineOptions::EmulatorOptions & outParsed)
{
    HRESULT                   hr      = S_OK;
    int                       argc    = 0;
    LPWSTR                  * argv    = nullptr;
    const wchar_t           * scan    = lpCmdLine;
    std::vector<std::string>  narrow;
    std::vector<char *>       pointers;
    bool                      hasArgs = false;



    outParsed = CommandLineOptions::EmulatorOptions();

    for (; scan != nullptr && *scan != L'\0'; scan++)
    {
        if (!iswspace (*scan))
        {
            hasArgs = true;
            break;
        }
    }

    BAIL_OUT_IF (!hasArgs, S_OK);

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

    outParsed = CommandLineParser::ParseEmulator ((int) pointers.size(), pointers.data());

    LocalFree (argv);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowCommandLineDialog
//
//  The usage text, in the themed box the rest of startup uses, with a refusal
//  above it when there is one.
//
//  IT RUNS BEFORE ANYTHING ELSE OF STARTUP, which is why it loads the theme
//  itself rather than taking one. Nothing has read preferences yet at this
//  point and no window exists; GlobalUserPrefs::Load only reads, so a command
//  line refused on the way in cannot disturb what is stored. A theme that
//  cannot be read falls back to the same default CassoTheme::MakeByName gives
//  every other caller.
//
//  THE TEXT IS BUILT IN CORE, from the parser's own option table, so this
//  function chooses a window and a glyph and describes nothing.
//
////////////////////////////////////////////////////////////////////////////////

static void ShowCommandLineDialog (const CommandLineOptions::EmulatorOptions & parsed)
{
    GlobalUserPrefs  prefs;
    Win32FileSystem  fsPrefs;
    std::wstring     title   = std::wstring (L"Casso ") + s_kchEmDash + L" Command Line";
    std::wstring     body;
    std::string      usage   = CommandLineHelp::BuildEmulatorHelp (parsed.flagPrefix);
    HRESULT          hrPrefs = prefs.Load (AssetBootstrap::GetAssetBaseDirectory().wstring(),
                                           fsPrefs);
    bool             refused = parsed.verdict
                            == CommandLineOptions::EmulatorOptions::Verdict::Refused;
    CassoTheme       theme;



    IGNORE_RETURN_VALUE (hrPrefs, S_OK);

    theme = CassoTheme::MakeByName (prefs.activeTheme);

    if (refused)
    {
        body  = TextEncoding::NarrowToWide (parsed.refusalMessage);
        body += L"\n\n";
    }

    body += TextEncoding::NarrowToWide (usage);

    (void) DxuiMessageBox (nullptr, &theme, body.c_str(), title.c_str(),
                           MB_OK | (refused ? MB_ICONWARNING : MB_ICONINFORMATION));
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

            // A failed download is shown by the picker itself, which then
            // comes back; what reaches here is a machine whose embedded
            // config could not be read.
            CHRN (hr, format (L"Boot disk picker failed:\n{}",
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
//  ReportAssertion
//
//  The GUI host for a failed EHM assertion: show the text with Abort / Retry
//  / Ignore, or break straight through when a debugger is attached, since the
//  dialog would only obscure the stack you came for.
//
//  EACH ASSERTION IS SHOWN ONCE PER RUN, and never while another one is
//  already on screen. Both ceilings exist because assertions fire from the
//  PAINT PATH, where nothing fails only once.
//
//  The nesting is the worse of the two, and is caused by the dialog itself: a
//  task-modal message box runs its own message loop, so it dispatches the very
//  WM_PAINT whose failed assertion put it on screen, which fails again and
//  stacks a second box on the first. Launching Casso minimized used to bury
//  the screen in some thirty of them within seconds, each one hiding what the
//  first had to say. So a failure raised while this function is already
//  reporting is logged and passed over.
//
//  Answering Ignore then has to mean "and stop telling me", because the frame
//  after the one you dismissed fails identically. Remembering the message text
//  keys that on the assertion SITE -- file, line, and expression are all in it
//  -- so a second, different failure still gets its dialog.
//
//  Suppressing a report is not suppressing the failure: every path here
//  returns to EHM's normal error path, exactly as Ignore does, and DEBUGMSG
//  has already logged the text.
//
////////////////////////////////////////////////////////////////////////////////

static void ReportAssertion (const wchar_t * message)
{
    // `reporting` is deliberately NOT thread_local: one assertion dialog at a
    // time is the intent, and a second thread failing while the first holds
    // the screen would stack a box behind a modal one nobody can reach.
    static std::atomic<bool>          reporting;
    static std::mutex                 seenLock;
    static std::set<std::wstring>     seen;
    std::wstring                      text;
    int                               choice = IDIGNORE;
    bool                              wasNew = false;



    if (IsDebuggerPresent())
    {
        __debugbreak();
        return;
    }

    if (reporting.exchange (true))
    {
        return;
    }

    {
        std::lock_guard<std::mutex>  guard (seenLock);

        wasNew = seen.insert ((message != nullptr) ? message : L"").second;
    }

    if (wasNew)
    {
        text  = L"An internal assertion failed:\n\n";
        text += (message != nullptr && message[0] != L'\0') ? message : L"(no detail)";
        text += L"\n\n"
                L"Abort  = quit now\n"
                L"Retry  = break (attach a debugger first to inspect)\n"
                L"Ignore = try to continue (this assertion will not be shown again)";

        choice = MessageBoxW (NULL, text.c_str(), L"Casso \x2014 assertion failed",
                              MB_ABORTRETRYIGNORE | MB_ICONERROR | MB_DEFBUTTON1 | MB_TASKMODAL);
    }

    reporting = false;

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
//  at all. ReportAssertion above is that hook, and says what it shows and how
//  often.
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
    //  What a command line this program could not take exits with. CassoCli
    //  documents 2 as "produced no output", which is exactly what a refused
    //  invocation of the emulator does.
    constexpr int  kRefusedCommandLineStatus = 2;



    HRESULT                              hr            = S_OK;
    wstring                              machineName;
    wstring                              disk1Path;
    wstring                              disk2Path;
    size_t                               traceCapacity = 0;
    bool                                 noImageWatch  = false;
    int                                  exitCode      = 0;
    bool                                 userExited    = false;
    bool                                 answered      = false;
    CommandLineOptions::EmulatorOptions  parsed;
    MachineConfig                        config;
    std::unique_ptr<EmulatorShell>       shell         = std::make_unique<EmulatorShell>();



    UNREFERENCED_PARAMETER (hPrevInstance);
    shell->SetStartupShowCommand (nCmdShow);

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
    // them and the shell replays them once there is a window. When startup
    // fails before there is one, the Error tail shows the queue in a system
    // box instead.

    SetNotifyFunction (&EmulatorShell::NotifyUser);

    // Register the GUI assertion host, so a failed EHM assertion (the *A macro
    // variants, or a bare ASSERT) surfaces its text instead of raising a raw
    // int 3 that becomes a silent "Casso.exe has stopped working". See
    // ReportAssertion.
    SetBreakpointFunction (&ReportAssertion);

    // Parse command line. A help request is answered and a command line that
    // could not be read is refused BEFORE any of the startup work below, so
    // neither one downloads an asset, writes a preference, or builds a machine
    // on the way to a dialog that was going to stop startup anyway.
    hr = ParseCommandLine (lpCmdLine, parsed);
    CHR (hr);

    answered = parsed.verdict != CommandLineOptions::EmulatorOptions::Verdict::Clean;

    if (answered)
    {
        ShowCommandLineDialog (parsed);

        // A help request was answered, so it succeeded. A refusal exits on the
        // status CassoCli spends on a command line it could not take, rather
        // than on the 1 the tail below gives a genuine failure.
        exitCode = (parsed.verdict == CommandLineOptions::EmulatorOptions::Verdict::Help)
                 ? 0 : kRefusedCommandLineStatus;
    }

    BAIL_OUT_IF (answered, S_OK);

    machineName   = TextEncoding::NarrowToWide (parsed.machine);
    disk1Path     = TextEncoding::NarrowToWide (parsed.disk1);
    disk2Path     = TextEncoding::NarrowToWide (parsed.disk2);
    traceCapacity = parsed.traceEntries;
    noImageWatch  = parsed.noImageWatch;

    shell->SetImageWatchDisabled (noImageWatch);

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

    // Load machine configuration. A user who dismissed one of the
    // startup dialogs wants out, so exit cleanly without a follow-up
    // error MessageBox.
    hr = LoadMachineConfig (hInstance, machineName, disk1Path, nullptr, userExited, config);
    CHR (hr);
    BAIL_OUT_IF (userExited, S_OK);

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

    // Success falls into the same tail the bails jump to: clearing the trace
    // back-pointer must happen exactly once, on every path, before the shell
    // it points at is destroyed.
Error:
    s_pTraceShell = nullptr;

    // exitCode is still 0 for any bail, including BAIL_OUT_IF (userExited) --
    // dismissing a startup dialog is a clean exit, not a failure.
    //
    // Anything still queued was never shown, because the window that drains
    // the queue never appeared. Show it the one way left before exiting with
    // a failure code.
    if (FAILED (hr))
    {
        EmulatorShell::ShowPendingNotificationsWithoutWindow();
        exitCode = 1;
    }

    return exitCode;
}





