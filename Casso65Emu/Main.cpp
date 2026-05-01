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
    LPWSTR lpCmdLine,
    std::wstring & outMachine,
    std::wstring & outDisk1,
    std::wstring & outDisk2)
{
    HRESULT hr = S_OK;

    int argc = 0;
    LPWSTR * argv = CommandLineToArgvW (lpCmdLine, &argc);

    CPRA (argv);

    for (int i = 0; i < argc; i++)
    {
        std::wstring arg (argv[i]);

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
//  wWinMain
//
////////////////////////////////////////////////////////////////////////////////

int WINAPI wWinMain (
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_     LPWSTR    lpCmdLine,
    _In_     int       nCmdShow)
{
    HRESULT hr = S_OK;

    UNREFERENCED_PARAMETER (hPrevInstance);
    UNREFERENCED_PARAMETER (nCmdShow);

    std::wstring machineName;
    std::wstring disk1Path;
    std::wstring disk2Path;
    std::string  error;



    // Register GUI error notification so EHM errors show a MessageBox
    SetNotifyFunction ([] (const wchar_t * message)
    {
        MessageBoxW (NULL, message, L"Casso65 Emulator", MB_OK | MB_ICONERROR);
    });

    // Initialize COM for WASAPI
    hr = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
    CHRA (hr);

    // Parse command line
    hr = ParseCommandLine (lpCmdLine, machineName, disk1Path, disk2Path);
    CHR (hr);

    // Default machine if not specified
    if (machineName.empty ())
    {
        machineName = L"apple2plus";
    }

    {
        // Build search paths and find machine config
        auto searchPaths = PathResolver::BuildSearchPaths (
            PathResolver::GetExecutableDirectory (),
            PathResolver::GetWorkingDirectory ());

        std::string narrowMachine = PathResolver::WideToNarrow (machineName);
        std::string configRelPath = "machines/" + narrowMachine + ".json";
        std::string configBase    = PathResolver::FindFile (searchPaths, configRelPath);

        CBRN (!configBase.empty (),
              std::format (L"Unknown machine '{}'. Config file not found.\n"
                           L"Searched for '{}' in exe directory, current directory, and parent directories.",
                           machineName,
                           std::wstring (configRelPath.begin (), configRelPath.end ())).c_str ());

        std::string configPath = configBase + "/" + configRelPath;

        // Load config file
        std::ifstream configFile (configPath);
        bool configGood = configFile.good ();
        CBRN (configGood,
              std::format (L"Cannot open machine config:\n{}",
                           std::wstring (configPath.begin (), configPath.end ())).c_str ());

        std::stringstream ss;
        ss << configFile.rdbuf ();
        std::string jsonText = ss.str ();

        // Parse config — prioritize the config's peer roms/ directory,
        // then fall back to other search paths
        std::vector<std::string> romSearchPaths = { configBase };

        for (const auto & p : searchPaths)
        {
            if (p != configBase)
            {
                romSearchPaths.push_back (p);
            }
        }

        MachineConfig config;
        hr = MachineConfigLoader::Load (jsonText, romSearchPaths, config, error);
        CHRN (hr, std::format (L"Failed to load machine config:\n{}",
                               std::wstring (error.begin (), error.end ())).c_str ());

        // Validate disk images
        if (!disk1Path.empty ())
        {
            std::ifstream disk (PathResolver::WideToNarrow (disk1Path), std::ios::binary | std::ios::ate);
            bool diskGood = disk.good ();
            CBRN (diskGood,
                  std::format (L"Disk image not found:\n{}", disk1Path).c_str ());

            auto size = disk.tellg ();
            bool validSize = (size == 143360);
            CBRN (validSize,
                  std::format (L"Disk image '{}' is not a valid .dsk file\n(expected 143360 bytes, got {} bytes)",
                               disk1Path, static_cast<int64_t> (size)).c_str ());
        }

        // Initialize emulator
        EmulatorShell shell;
        hr = shell.Initialize (hInstance, config,
                               PathResolver::WideToNarrow (disk1Path), PathResolver::WideToNarrow (disk2Path));
        CHRN (hr, L"Failed to initialize emulator");

        // Run message loop
        return shell.RunMessageLoop ();
    }

Error:
    CoUninitialize ();
    return FAILED (hr) ? 1 : 0;
}
