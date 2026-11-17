#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PathResolver
//
//  Where Casso looks for its files, and how it stores paths so a folder can be
//  moved.
//
//  Assets are found by SEARCHING a list of bases -- the exe directory, the
//  working directory, and their parents -- rather than from one install root.
//  That is what lets the same binary run from an installed layout, from a
//  build output directory, and from a source tree, each finding the assets
//  beside it.
//
//  User-writable state goes to %LOCALAPPDATA% instead, and that split matters:
//  the install tree may be read-only or shared between users, so prefs and
//  downloaded assets cannot live there.
//
//  Paths that are PERSISTED go through MakeExeRelativePath and come back
//  through its inverse, so a casso.exe plus its Disks/ tree stays portable
//  across a move or a copy to another machine. Anything outside the exe
//  subtree stays absolute, since it has no portable spelling.
//
//  Every entry point is static -- there is no resolver state, only queries
//  about the environment -- and the fallbacks are layered so a failure of the
//  preferred mechanism degrades rather than returning nothing.
//
////////////////////////////////////////////////////////////////////////////////

class PathResolver
{
public:

    // Build the list of base directories to search (exe dir, cwd, parents)
    static vector<fs::path> BuildSearchPaths (const fs::path & exeDir, const fs::path & cwd);

    // Find a file by searching base paths + relative path.
    // Returns the full resolved path, or empty path if not found.
    static fs::path FindFile (const vector<fs::path> & searchPaths, const fs::path & relativePath);

    static fs::path FindOrCreateAssetDir (const vector<fs::path> & searchPaths,
                                          const fs::path         & relativeDir,
                                          const fs::path         & fallbackBase);

    // Get the executable's directory
    static fs::path GetExecutableDirectory ();

    // Get the current working directory
    static fs::path GetWorkingDirectory ();

    // Returns %LOCALAPPDATA%\<appName>\, creating the directory if it
    // doesn't already exist. Falls back to the %LOCALAPPDATA% env var
    // and finally %USERPROFILE%\AppData\Local if SHGetKnownFolderPath
    // fails (extremely rare on supported Windows). Empty path returned
    // only if every fallback fails.
    //
    // When the CASSO_PROFILE_DIR environment variable is set, that folder is
    // returned in place of %LOCALAPPDATA%\<appName>\. It is for a test copy
    // of Casso Explorer run beside the owner's. Its prefs, session and logs
    // go to that folder, so it doesn't start the owner's saved connections.
    static fs::path GetLocalAppDataDir (const std::wstring & appName);

    static constexpr const wchar_t * kpszProfileVariable = L"CASSO_PROFILE_DIR";

    // Whether CASSO_PROFILE_DIR is set.
    static bool     IsProfileFolderSet ();

    // Convert an absolute path to one relative to the executable
    // directory when the input lives under (or beside) the exe. Used
    // by the per-machine disk-path persistence so the casso.exe +
    // Disks/ tree stays portable across moves. Returns the input
    // unchanged if it's already relative, lives outside the exe
    // subtree, or escapes via `..`.
    static std::wstring MakeExeRelativePath (const std::wstring & absolutePath);

    // Inverse of MakeExeRelativePath: relative entries are joined
    // with the exe directory; absolute entries pass through.
    static std::wstring ResolveExeRelativePath (const std::wstring & storedPath);

    // A UNC path, or one on a drive letter mapped to a network share. A
    // drive letter's kind is known locally, so the query never waits on the
    // network.
    static bool IsNetworkPath (const std::wstring & path);
};
