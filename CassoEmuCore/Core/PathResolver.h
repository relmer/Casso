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

    // The one folder this process reads and writes everything under, created
    // when it doesn't exist. Chosen on the first call, the same afterward.
    // CASSO_PROFILE_DIR, when set, is the folder. Otherwise an official build
    // (signed, or running from an MSIX) uses %LOCALAPPDATA%\Casso, and any
    // other build uses CassoData at its repo root, or beside the module when
    // no repo is found. Empty only when an official build can't find
    // %LOCALAPPDATA%. The rules are in DataFolderResolver.
    static fs::path GetDataFolder ();

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

private:

    static constexpr const wchar_t * kpszFallbackModuleName = L"Casso.exe";

    static fs::path  ChooseDataFolder      ();
    static fs::path  GetLocalAppDataFolder ();
    static bool      HasGitEntry           (const fs::path & folder);
};
