#include "Pch.h"

#include "PathResolver.h"

#include "Core/DataFolderResolver.h"
#include "Update/AuthenticodeVerifier.h"
#include "Update/InstallTypeDetector.h"
#include "Update/ModuleInstallEnvironment.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BuildSearchPaths
//
////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////
//
//  BuildSearchPaths
//
//  Returns the single user-writable asset directory, the data folder.
//  Casso uses no exe-adjacent or cwd-relative fallback path -- every file
//  it reads or writes lives in that one directory. The exeDir / cwd
//  parameters are accepted for API compatibility but ignored.
//
////////////////////////////////////////////////////////////////////////////////

vector<fs::path> PathResolver::BuildSearchPaths (
    const fs::path & /*exeDir*/,
    const fs::path & /*cwd*/)
{
    vector<fs::path>  searchBases;



    fs::path          dataFolder = GetDataFolder();



    if (!dataFolder.empty())
    {
        searchBases.push_back (dataFolder);
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
//  GetDataFolder
//
//  The one folder this process reads and writes everything under. It is chosen
//  on the first call and is the same on every later call from any thread. The
//  rules are in DataFolderResolver; ChooseDataFolder gathers their inputs.
//
////////////////////////////////////////////////////////////////////////////////

fs::path PathResolver::GetDataFolder()
{
    static const fs::path  folder = ChooseDataFolder();



    return folder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDataFolder
//
//  Reads CASSO_PROFILE_DIR, the install type of the module that contains this
//  code, the module's path and the local application data folder, applies
//  DataFolderResolver and creates the resulting folder.
//
//  The install type check verifies a signature, so it is skipped when
//  CASSO_PROFILE_DIR is set, since that variable decides the folder by itself.
//  A check that fails to run counts as a developer build, so a failure never
//  sends a developer build to the shipped folder.
//
//  An empty result means no rule produced a folder: an official build whose
//  local application data folder could not be found. Nothing is created then.
//
////////////////////////////////////////////////////////////////////////////////

fs::path PathResolver::ChooseDataFolder()
{
    HRESULT                   hr            = S_OK;
    ModuleInstallEnvironment  environment;
    AuthenticodeVerifier      verifier;
    DataFolderInputs          inputs;
    DataFolderResult          result;
    std::wstring              modulePath;
    wchar_t                   env[MAX_PATH] = {};
    DWORD                     envLen        = 0;
    error_code                ec;



    envLen = GetEnvironmentVariableW (kpszProfileVariable, env, MAX_PATH);
    if (envLen > 0 && envLen < MAX_PATH)
    {
        inputs.profileOverride = env;
    }

    inputs.localAppData = GetLocalAppDataFolder();
    inputs.hasGitEntry  = &PathResolver::HasGitEntry;
    inputs.installType  = InstallType::Developer;

    hr = environment.GetExecutablePath (modulePath);
    if (SUCCEEDED (hr))
    {
        inputs.modulePath = modulePath;
    }
    else
    {
        inputs.modulePath = GetExecutableDirectory() / kpszFallbackModuleName;
    }

    if (inputs.profileOverride.empty())
    {
        hr = InstallTypeDetector::Detect (environment, verifier, inputs.installType);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    result = DataFolderResolver::Resolve (inputs);

    if (!result.folder.empty())
    {
        fs::create_directories (result.folder, ec);
    }

    return result.folder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLocalAppDataFolder
//
//  The user's local application data folder, with three layered fallbacks
//  because this should not fail on a normal user profile:
//
//   1. SHGetKnownFolderPath (FOLDERID_LocalAppData) -- canonical API,
//      works on every supported Windows even with redirected profiles.
//   2. %LOCALAPPDATA% env var -- fine for the rare case where the
//      Known Folder API fails (e.g. service contexts that don't fall
//      out cleanly).
//   3. %USERPROFILE%\AppData\Local -- last-resort literal path.
//
//  Empty when all three fail.
//
////////////////////////////////////////////////////////////////////////////////

fs::path PathResolver::GetLocalAppDataFolder()
{
    HRESULT     hr            = S_OK;
    PWSTR       pszPath       = nullptr;
    fs::path    result;
    wchar_t     env[MAX_PATH] = {};
    DWORD       envLen        = 0;



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

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasGitEntry
//
//  A clone has .git as a folder and a worktree has it as a file, so either
//  counts.
//
////////////////////////////////////////////////////////////////////////////////

bool PathResolver::HasGitEntry (const fs::path & folder)
{
    error_code  ec;



    return fs::exists (folder / L".git", ec);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsProfileFolderSet
//
////////////////////////////////////////////////////////////////////////////////

bool PathResolver::IsProfileFolderSet()
{
    return GetEnvironmentVariableW (kpszProfileVariable, nullptr, 0) > 0;
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





////////////////////////////////////////////////////////////////////////////////
//
//  IsNetworkPath
//
////////////////////////////////////////////////////////////////////////////////

bool PathResolver::IsNetworkPath (const std::wstring & path)
{
    std::wstring  root;



    if (path.starts_with (L"\\\\?\\UNC\\"))
    {
        return true;
    }

    if (path.starts_with (L"\\\\"))
    {
        return !path.starts_with (L"\\\\?\\") && !path.starts_with (L"\\\\.\\");
    }

    if (path.size() < 2 || path[1] != L':')
    {
        return false;
    }

    root = path.substr (0, 2) + L"\\";

    return GetDriveTypeW (root.c_str()) == DRIVE_REMOTE;
}