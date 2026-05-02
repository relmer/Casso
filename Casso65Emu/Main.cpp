#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Core/PathResolver.h"
#include "Ehm.h"
#include "EmulatorShell.h"

#pragma comment(lib, "ole32.lib")





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
    const wstring     & machineName,
    const wstring     & disk1Path,
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
    string              error;



    // Build search paths and find machine config
    searchPaths    = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(),
                                                     PathResolver::GetWorkingDirectory());
    configRelPath  = fs::path ("machines") / (fs::path (machineName).string() + ".json");
    configPath     = PathResolver::FindFile (searchPaths, configRelPath);

    CBRN (!configPath.empty(),
          format (L"Unknown machine '{}'. Config file not found.\n"
                  L"Searched for '{}' in exe directory, current directory, and parent directories.",
                  machineName,
                  configRelPath.wstring()).c_str());

    // Load config file
    configFile.open (configPath);
    configGood = configFile.good();
    CBRN (configGood,
          format (L"Cannot open machine config:\n{}",
                  configPath.wstring()).c_str());

    ss << configFile.rdbuf();
    jsonText = ss.str();

    // Parse config — prioritize the config's peer roms/ directory,
    // then fall back to other search paths
    romSearchPaths.push_back (configPath.parent_path().parent_path());

    for (const auto & p : searchPaths)
    {
        if (p != romSearchPaths[0])
        {
            romSearchPaths.push_back (p);
        }
    }

    hr = MachineConfigLoader::Load (jsonText, romSearchPaths, outConfig, error);
    CHRN (hr, format (L"Failed to load machine config:\n{}",
                      wstring (error.begin(), error.end())).c_str());

    // Validate disk images
    if (!disk1Path.empty())
    {
        fs::path    diskPath  = fs::path (disk1Path);
        bool        diskGood  = fs::exists (diskPath);

        CBRN (diskGood,
              format (L"Disk image not found:\n{}", disk1Path).c_str());

        auto        diskSize  = fs::file_size (diskPath);
        bool        validSize = (diskSize == 143360);

        CBRN (validSize,
              format (L"Disk image '{}' is not a valid .dsk file\n(expected 143360 bytes, got {} bytes)",
                      disk1Path, static_cast<int64_t> (diskSize)).c_str());
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

    // Register GUI error notification so EHM errors show a MessageBox
    SetNotifyFunction ([] (const wchar_t * message)
    {
        MessageBoxW (NULL, message, L"Casso65 Emulator", MB_OK | MB_ICONERROR);
    });

    // Parse command line
    hr = ParseCommandLine (lpCmdLine, machineName, disk1Path, disk2Path);
    CHR (hr);

    // Default machine if not specified
    if (machineName.empty())
    {
        machineName = L"apple2plus";
    }

    // Load machine configuration
    hr = LoadMachineConfig (machineName, disk1Path, config);
    CHR (hr);

    // Initialize emulator
    hr = shell.Initialize (hInstance, config,
                           fs::path (disk1Path).string(),
                           fs::path (disk2Path).string());
    CHRN (hr, L"Failed to initialize emulator");

    // Run message loop
    return shell.RunMessageLoop();

Error:
    return FAILED (hr) ? 1 : 0;
}





