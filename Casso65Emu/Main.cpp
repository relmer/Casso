#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Ehm.h"
#include "Shell/EmulatorShell.h"

#pragma comment(lib, "ole32.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  GetExecutableDirectory
//
////////////////////////////////////////////////////////////////////////////////

static std::string GetExecutableDirectory ()
{
    char path[MAX_PATH] = {};
    GetModuleFileNameA (nullptr, path, MAX_PATH);

    std::string dir (path);
    size_t pos = dir.find_last_of ("\\/");

    if (pos != std::string::npos)
    {
        dir = dir.substr (0, pos);
    }

    return dir;
}





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
//  WideToNarrow — convert std::wstring to std::string (UTF-8)
//
////////////////////////////////////////////////////////////////////////////////

static std::string WideToNarrow (const std::wstring & wide)
{
    if (wide.empty ())
    {
        return "";
    }

    int len = WideCharToMultiByte (CP_UTF8, 0, wide.c_str (), -1, NULL, 0, NULL, NULL);
    std::string narrow (len - 1, '\0');
    WideCharToMultiByte (CP_UTF8, 0, wide.c_str (), -1, narrow.data (), len, NULL, NULL);
    return narrow;
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



    // Initialize COM for WASAPI
    CHRA (CoInitializeEx (nullptr, COINIT_MULTITHREADED));

    // Parse command line
    CHR (ParseCommandLine (lpCmdLine, machineName, disk1Path, disk2Path));

    // Default machine if not specified
    if (machineName.empty ())
    {
        machineName = L"apple2plus";
    }

    {
        // Resolve paths
        std::string basePath   = GetExecutableDirectory ();
        std::string configPath = basePath + "/machines/" + WideToNarrow (machineName) + ".json";

        // Load config file
        std::ifstream configFile (configPath);
        CBRN (configFile.good (),
              std::format (L"Unknown machine '{}'. Config file not found:\n{}", machineName,
                           std::wstring (configPath.begin (), configPath.end ())).c_str ());

        std::stringstream ss;
        ss << configFile.rdbuf ();
        std::string jsonText = ss.str ();

        // Parse config
        MachineConfig config;
        hr = MachineConfigLoader::Load (jsonText, basePath, config, error);
        CHRN (hr, std::format (L"Failed to load machine config:\n{}",
                               std::wstring (error.begin (), error.end ())).c_str ());

        // Validate disk images
        if (!disk1Path.empty ())
        {
            std::ifstream disk (WideToNarrow (disk1Path), std::ios::binary | std::ios::ate);
            CBRN (disk.good (),
                  std::format (L"Disk image not found:\n{}", disk1Path).c_str ());

            auto size = disk.tellg ();
            CBRN (size == 143360,
                  std::format (L"Disk image '{}' is not a valid .dsk file\n(expected 143360 bytes, got {} bytes)",
                               disk1Path, static_cast<int64_t> (size)).c_str ());
        }

        // Initialize emulator
        EmulatorShell shell;
        CHRN (shell.Initialize (hInstance, config, basePath,
                                WideToNarrow (disk1Path), WideToNarrow (disk2Path)),
              L"Failed to initialize emulator");

        // Run message loop
        return shell.RunMessageLoop ();
    }

Error:
    CoUninitialize ();
    return FAILED (hr) ? 1 : 0;
}
