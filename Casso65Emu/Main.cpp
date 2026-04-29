#include "Pch.h"

#include "Core/MachineConfig.h"
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
    std::string & outMachine,
    std::string & outDisk1,
    std::string & outDisk2,
    std::string & outError)
{
    HRESULT hr = S_OK;

    int argc = 0;
    LPWSTR * argv = CommandLineToArgvW (lpCmdLine, &argc);

    if (argv == nullptr)
    {
        return S_OK;
    }

    for (int i = 0; i < argc; i++)
    {
        std::wstring arg (argv[i]);

        if (arg == L"--machine" && i + 1 < argc)
        {
            std::wstring val (argv[++i]);
            outMachine = std::string (val.begin (), val.end ());
        }
        else if (arg == L"--disk1" && i + 1 < argc)
        {
            std::wstring val (argv[++i]);
            outDisk1 = std::string (val.begin (), val.end ());
        }
        else if (arg == L"--disk2" && i + 1 < argc)
        {
            std::wstring val (argv[++i]);
            outDisk2 = std::string (val.begin (), val.end ());
        }
    }

    LocalFree (argv);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowError
//
////////////////////////////////////////////////////////////////////////////////

static void ShowError (const std::string & message)
{
    std::wstring wide (message.begin (), message.end ());
    MessageBoxW (nullptr, wide.c_str (), L"Casso65 Error", MB_ICONERROR | MB_OK);
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

    std::string machineName;
    std::string disk1Path;
    std::string disk2Path;
    std::string error;

    // Initialize COM for WASAPI
    CoInitializeEx (nullptr, COINIT_MULTITHREADED);

    // Parse command line
    CHR (ParseCommandLine (lpCmdLine, machineName, disk1Path, disk2Path, error));

    // Default machine if not specified
    if (machineName.empty ())
    {
        machineName = "apple2plus";
    }

    {
        // Resolve paths
        std::string basePath = GetExecutableDirectory ();
        std::string configPath = basePath + "/machines/" + machineName + ".json";

        // Load config file
        std::ifstream configFile (configPath);

        if (!configFile.good ())
        {
            ShowError (std::format (
                "Unknown machine '{}'. Config file not found: {}",
                machineName, configPath));
            hr = E_FAIL;
            goto Error;
        }

        std::stringstream ss;
        ss << configFile.rdbuf ();
        std::string jsonText = ss.str ();

        // Parse config
        MachineConfig config;
        hr = MachineConfigLoader::Load (jsonText, basePath, config, error);

        if (FAILED (hr))
        {
            ShowError (std::format ("Failed to load machine config: {}", error));
            goto Error;
        }

        // Validate disk images
        if (!disk1Path.empty ())
        {
            std::ifstream disk (disk1Path, std::ios::binary | std::ios::ate);

            if (!disk.good ())
            {
                ShowError (std::format ("Disk image not found: {}", disk1Path));
                hr = E_FAIL;
                goto Error;
            }

            auto size = disk.tellg ();

            if (size != 143360)
            {
                ShowError (std::format (
                    "Disk image '{}' is not a valid .dsk file (expected 143360 bytes, got {} bytes)",
                    disk1Path, static_cast<int64_t> (size)));
                hr = E_FAIL;
                goto Error;
            }
        }

        // Initialize emulator
        EmulatorShell shell;
        CHR (shell.Initialize (hInstance, config, basePath, disk1Path, disk2Path));

        // Run message loop
        return shell.RunMessageLoop ();
    }

Error:
    CoUninitialize ();
    return FAILED (hr) ? 1 : 0;
}
