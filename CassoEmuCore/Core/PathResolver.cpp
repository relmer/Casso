#include "Pch.h"

#include "PathResolver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BuildSearchPaths
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
//  BuildSearchPaths
//
//  Returns the single user-writable asset directory: %LOCALAPPDATA%\Casso\.
//  Casso uses no exe-adjacent or cwd-relative fallback path -- every file
//  it reads or writes lives in that one directory. The exeDir / cwd
//  parameters are accepted for API compatibility but ignored.
//
////////////////////////////////////////////////////////////////////////////////

vector<fs::path> PathResolver::BuildSearchPaths (
    const fs::path & /*exeDir*/,
    const fs::path & /*cwd*/)
{
    fs::path          localAppData = GetLocalAppDataDir (L"Casso");
    vector<fs::path>  searchBases;



    if (!localAppData.empty())
    {
        searchBases.push_back (localAppData);
    }

    return searchBases;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindFile
//
////////////////////////////////////////////////////////////////////////////////

fs::path PathResolver::FindFile (
    const vector<fs::path> & searchPaths,
    const fs::path & relativePath)
{
    for (const auto & base : searchPaths)
    {
        fs::path candidate = base / relativePath;

        if (fs::exists (candidate))
        {
            return candidate;
        }
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindOrCreateAssetDir
//
//  Find an existing directory matching `relativeDir` within
//  `searchPaths`. If none is found, return `fallbackBase /
//  relativeDir` (creating it on disk). Used to honor the existing
//  repo layout when present, or to bootstrap loose-exe layouts.
//
////////////////////////////////////////////////////////////////////////////////

fs::path PathResolver::FindOrCreateAssetDir (
    const vector<fs::path> & searchPaths,
    const fs::path         & relativeDir,
    const fs::path         & fallbackBase)
{
    fs::path           target;
    error_code         ec;

    

    for (const auto & base : searchPaths)
    {
        fs::path candidate = base / relativeDir;

        if (fs::is_directory (candidate, ec))
        {
            return candidate;
        }
    }

    target = fallbackBase / relativeDir;
    fs::create_directories (target, ec);
    return target;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetExecutableDirectory
//
////////////////////////////////////////////////////////////////////////////////

fs::path PathResolver::GetExecutableDirectory ()
{
    wchar_t buf[MAX_PATH] = {};



    GetModuleFileNameW (nullptr, buf, MAX_PATH);
    return fs::path (buf).parent_path ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetWorkingDirectory
//
////////////////////////////////////////////////////////////////////////////////

fs::path PathResolver::GetWorkingDirectory ()
{
    return fs::current_path ();
}




////////////////////////////////////////////////////////////////////////////////
//
//  GetLocalAppDataDir
//
//  Resolves %LOCALAPPDATA%\<appName>\, creating the directory tree if
//  it doesn't already exist. Three layered fallbacks because we never
//  want this to fail in normal user setups:
//
//   1. SHGetKnownFolderPath (FOLDERID_LocalAppData) -- canonical API,
//      works on every supported Windows even with redirected profiles.
//   2. %LOCALAPPDATA% env var -- fine for the rare case where the
//      Known Folder API fails (e.g. service contexts that don't fall
//      out cleanly).
//   3. %USERPROFILE%\AppData\Local -- last-resort literal path.
//
////////////////////////////////////////////////////////////////////////////////

fs::path PathResolver::GetLocalAppDataDir (const std::wstring & appName)
{
    HRESULT      hr      = S_OK;
    PWSTR        pszPath = nullptr;
    fs::path     result;
    error_code   ec;
    wchar_t      env[MAX_PATH] = {};
    DWORD        envLen  = 0;



    hr = SHGetKnownFolderPath (FOLDERID_LocalAppData, 0, nullptr, &pszPath);
    if (SUCCEEDED (hr) && pszPath != nullptr)
    {
        result = fs::path (pszPath);
        CoTaskMemFree (pszPath);
    }

    if (result.empty())
    {
        envLen = GetEnvironmentVariableW (L"LOCALAPPDATA", env, MAX_PATH);
        if (envLen > 0 && envLen < MAX_PATH)
        {
            result = fs::path (env);
        }
    }

    if (result.empty())
    {
        envLen = GetEnvironmentVariableW (L"USERPROFILE", env, MAX_PATH);
        if (envLen > 0 && envLen < MAX_PATH)
        {
            result = fs::path (env) / L"AppData" / L"Local";
        }
    }

    if (result.empty())
    {
        return result;
    }

    result /= appName;
    fs::create_directories (result, ec);
    return result;
}
