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
    fs::path  found;
    fs::path  candidate;
    size_t    i = 0;



    // First hit wins -- searchPaths is in priority order. The loop condition
    // carries the found test so a hit stops the scan; every extra iteration
    // would be another filesystem round trip.
    for (i = 0; found.empty() && i < searchPaths.size(); i++)
    {
        candidate = searchPaths[i] / relativePath;

        if (fs::exists (candidate))
        {
            found = candidate;
        }
    }

    return found;
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
    fs::path           candidate;
    error_code         ec;
    size_t             i = 0;



    for (i = 0; target.empty() && i < searchPaths.size(); i++)
    {
        candidate = searchPaths[i] / relativeDir;

        if (fs::is_directory (candidate, ec))
        {
            target = candidate;
        }
    }

    // Nothing on disk matched the existing repo layout, so bootstrap one
    // under the fallback base. create_directories is a no-op if it races.
    if (target.empty())
    {
        target = fallbackBase / relativeDir;
        fs::create_directories (target, ec);
    }

    return target;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetExecutableDirectory
//
////////////////////////////////////////////////////////////////////////////////

fs::path PathResolver::GetExecutableDirectory()
{
    wchar_t buf[MAX_PATH] = {};



    GetModuleFileNameW (nullptr, buf, MAX_PATH);
    return fs::path (buf).parent_path();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetWorkingDirectory
//
////////////////////////////////////////////////////////////////////////////////

fs::path PathResolver::GetWorkingDirectory()
{
    return fs::current_path();
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

    // All three fallbacks failed, which should not happen on a normal user
    // profile. Yield the empty path rather than creating "\<appName>" at the
    // drive root.
    if (!result.empty())
    {
        result /= appName;
        fs::create_directories (result, ec);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeExeRelativePath
//
//  Rewrites a path as relative to the executable when it lives under the exe
//  directory, so a persisted path survives the folder being moved.
//
//  A path that ESCAPES via ".." stays absolute. Baking a climb-out into a
//  prefs file is worse than an absolute path: it breaks the moment the install
//  moves, and it breaks silently, resolving to some unrelated location rather
//  than failing.
//
//  Every failure yields the input unchanged, which is why result starts there
//  and only a clean relativization overwrites it. An absolute path always
//  works; a wrong relative one does not.
//
//  fs::relative is used with an error_code rather than the throwing overload,
//  since a path on a different volume has no relative form at all and that is
//  an ordinary case here, not an exception.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PathResolver::MakeExeRelativePath (const std::wstring & absolutePath)
{
    fs::path     input  = fs::path (absolutePath);
    fs::path     exeDir;
    fs::path     rel;
    error_code   ec;
    std::wstring first;
    std::wstring result = absolutePath;



    // Every failure yields the absolute path unchanged, so `result` starts
    // there and only a clean relativization overwrites it.
    if (!input.empty() && input.is_absolute())
    {
        exeDir = GetExecutableDirectory();
        rel    = fs::relative (input, exeDir, ec);

        if (!ec && !rel.empty())
        {
            // A path that escapes the exe directory (starts with "..") stays
            // absolute -- baking a brittle climb-out into the prefs file
            // breaks the moment the install moves.
            first = (rel.begin() == rel.end()) ? std::wstring() : rel.begin()->wstring();

            if (first != L"..")
            {
                result = rel.wstring();
            }
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveExeRelativePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PathResolver::ResolveExeRelativePath (const std::wstring & storedPath)
{
    fs::path  stored     = fs::path (storedPath);
    bool      isRelative = !storedPath.empty() && !stored.is_absolute();



    // The inverse of MakeExeRelativePath: only a stored relative path needs
    // the exe directory put back in front of it.
    return isRelative
               ? (GetExecutableDirectory() / stored).lexically_normal().wstring()
               : storedPath;
}
