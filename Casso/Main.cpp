#include "Pch.h"

#include "AssetBootstrap.h"
#include "Core/MachineConfig.h"
#include "Core/PathResolver.h"
#include "DiskSettings.h"
#include "Ehm.h"
#include "EmulatorShell.h"
#include "MachinePickerDialog.h"
#include "RegistrySettings.h"

#pragma comment(lib, "ole32.lib")


static constexpr LPCWSTR kLastMachineValue = L"LastMachine";





////////////////////////////////////////////////////////////////////////////////
//
//  ParseCommandLine
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT ParseCommandLine (
    LPWSTR         lpCmdLine,
    wstring & outMachine,
    wstring & outDisk1,
    wstring & outDisk2)
{
    HRESULT   hr   = S_OK;
    int       argc = 0;
    LPWSTR  * argv = nullptr;



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

    // Pre-flight: detect missing ROMs and offer to download them
    // BEFORE we open the on-disk JSON. The download set is decided
    // strictly from the embedded default for `machineName`, so if
    // the user has edited their on-disk JSON they're responsible
    // for any extra ROMs they reference.
    romDir = AssetBootstrap::GetAssetBaseDirectory (romSearchPaths,
                                                    PathResolver::GetExecutableDirectory());

    hr = AssetBootstrap::CheckAndFetchRoms (hInstance, machineName, hwndParent,
                                            romSearchPaths, romDir, error);
    BAIL_OUT_IF (hr == S_FALSE, S_FALSE);
    CHRN (hr, format (L"ROM download failed:\n{}",
                      wstring (error.begin(), error.end())).c_str());

    // Boot-disk pre-flight: if the user didn't pass --disk1 and the
    // registry has no remembered disk for this machine (or the
    // remembered path no longer points at a real file), and the
    // machine has a Disk ][ controller, offer to download a stock
    // Apple system master disk. Without this the user just stares at
    // a spinning drive forever after first launch.
    if (inoutDisk1Path.empty())
    {
        hrSaved = DiskSettings::ReadSavedDiskPath (0, machineName, savedDisk);
        IGNORE_RETURN_VALUE (hrSaved, S_OK);

        // Treat a remembered-but-missing disk the same as "no remembered
        // disk", and clear the stale registry value so we don't keep
        // tripping over it on every launch.
        if (!savedDisk.empty() && !fs::exists (fs::path (savedDisk)))
        {
            HRESULT hrClear = DiskSettings::WriteSavedDiskPath (
                0, machineName, wstring());
            IGNORE_RETURN_VALUE (hrClear, S_OK);
            savedDisk.clear();
        }

        if (savedDisk.empty())
        {
            wstring  downloaded;

            diskDir = AssetBootstrap::GetDiskDirectory (
                romSearchPaths,
                PathResolver::GetExecutableDirectory());

            hr = AssetBootstrap::OfferBootDiskDownload (
                hInstance, machineName, hwndParent, diskDir, downloaded, error);

            // S_FALSE = "user said no" or "no disk controller for this
            // machine" — both are fine, just keep the slot empty.
            // Hard failure surfaces a notification and bails.
            if (hr != S_FALSE)
            {
                CHRN (hr, format (L"Boot disk download failed:\n{}",
                                  wstring (error.begin(), error.end())).c_str());
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

        auto        diskSize  = fs::file_size (diskPath);
        bool        validSize = (diskSize == 143360);

        CBRN (validSize,
              format (L"Disk image '{}' is not a valid .dsk file\n(expected 143360 bytes, got {} bytes)",
                      inoutDisk1Path, static_cast<int64_t> (diskSize)).c_str());
    }

Error:
    return hr;
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
    HRESULT        hr = S_OK;
    wstring        machineName;
    wstring        disk1Path;
    wstring        disk2Path;
    MachineConfig  config;
    EmulatorShell  shell;



    UNREFERENCED_PARAMETER (hPrevInstance);
    UNREFERENCED_PARAMETER (nCmdShow);

#ifdef _DEBUG
    // Enable frequent heap validation to catch corruption near its source
    // _CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
#endif

    // Register GUI error notification so EHM errors show a MessageBox
    SetNotifyFunction ([] (const wchar_t * message)
    {
        MessageBoxW (NULL, message, L"Casso Emulator", MB_OK | MB_ICONERROR);
    });

    // Parse command line
    hr = ParseCommandLine (lpCmdLine, machineName, disk1Path, disk2Path);
    CHR (hr);

    // Make sure a Machines/ directory exists with at least the stock
    // JSON configs (extracts embedded resources on first run if the
    // user is running a loose casso.exe with no Machines/ folder).
    {
        vector<fs::path> bootstrapPaths = PathResolver::BuildSearchPaths (
            PathResolver::GetExecutableDirectory(),
            PathResolver::GetWorkingDirectory());

        HRESULT hrBoot = AssetBootstrap::EnsureMachineConfigs (
            hInstance,
            bootstrapPaths,
            PathResolver::GetExecutableDirectory());
        IGNORE_RETURN_VALUE (hrBoot, S_OK);
    }

    // Resolve machine name: command line > registry > picker dialog
    if (machineName.empty())
    {
        RegistrySettings::ReadString (kLastMachineValue, machineName);
    }

    if (machineName.empty() ||
        PathResolver::FindFile (
            PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(),
                                            PathResolver::GetWorkingDirectory()),
            fs::path ("Machines") / fs::path (machineName).string()
                                  / (fs::path (machineName).string() + ".json")).empty())
    {
        machineName = MachinePickerDialog::Show (nullptr, machineName);
        CBR (!machineName.empty());
    }

    // Load machine configuration. S_FALSE here means the user
    // declined the missing-ROM download prompt — exit cleanly
    // without a follow-up error MessageBox.
    hr = LoadMachineConfig (hInstance, machineName, disk1Path, nullptr, config);
    CHR (hr);
    BAIL_OUT_IF (hr == S_FALSE, S_OK);

    // Remember last-used machine (don't pollute hr with the result)
    {
        HRESULT hrReg = RegistrySettings::WriteString (kLastMachineValue, machineName);
        IGNORE_RETURN_VALUE (hrReg, S_OK);
    }

    // Initialize emulator
    hr = shell.Initialize (hInstance, machineName, config,
                           fs::path (disk1Path).string(),
                           fs::path (disk2Path).string());
    CHRN (hr, L"Failed to initialize emulator");

    // Run message loop
    return shell.RunMessageLoop();

Error:
    return FAILED (hr) ? 1 : 0;
}





