#include "Pch.h"

#include "EhmTestHelper.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace
{
    HKEY           g_isolationHive        = nullptr;
    std::wstring   g_isolationRoot;

    void EnsureDirectory (const std::wstring & path)
    {
        std::error_code ec;
        fs::create_directories (path, ec);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ModuleSetup
//
////////////////////////////////////////////////////////////////////////////////

TEST_MODULE_INITIALIZE (ModuleSetup)
{
    DWORD  pid = GetCurrentProcessId();
    wchar_t tempPath[MAX_PATH] = {};
    std::wstring hivePath;

    GetTempPathW (MAX_PATH, tempPath);
    g_isolationRoot = std::wstring (tempPath) + L"CassoUnitTest-" + std::to_wstring (pid);

    EnsureDirectory (g_isolationRoot);
    EnsureDirectory (g_isolationRoot + L"\\Temp");
    EnsureDirectory (g_isolationRoot + L"\\AppData");
    EnsureDirectory (g_isolationRoot + L"\\LocalAppData");

    SetEnvironmentVariableW (L"TEMP",          (g_isolationRoot + L"\\Temp").c_str());
    SetEnvironmentVariableW (L"TMP",           (g_isolationRoot + L"\\Temp").c_str());
    SetEnvironmentVariableW (L"APPDATA",       (g_isolationRoot + L"\\AppData").c_str());
    SetEnvironmentVariableW (L"LOCALAPPDATA",  (g_isolationRoot + L"\\LocalAppData").c_str());
    SetEnvironmentVariableW (L"CASSO_TEST_ROOT", g_isolationRoot.c_str());

    hivePath = L"Software\\CassoUnitTest\\Isolation\\" + std::to_wstring (pid);
    RegCreateKeyExW (HKEY_CURRENT_USER,
                     hivePath.c_str(),
                     0,
                     nullptr,
                     REG_OPTION_VOLATILE,
                     KEY_ALL_ACCESS,
                     nullptr,
                     &g_isolationHive,
                     nullptr);
    if (g_isolationHive != nullptr)
    {
        RegOverridePredefKey (HKEY_CURRENT_USER, g_isolationHive);
    }

    UnitTestHelpers::SetupForUnitTests ();
}





TEST_MODULE_CLEANUP (ModuleCleanup)
{
    RegOverridePredefKey (HKEY_CURRENT_USER, nullptr);

    if (g_isolationHive != nullptr)
    {
        RegCloseKey (g_isolationHive);
        g_isolationHive = nullptr;
    }
}
