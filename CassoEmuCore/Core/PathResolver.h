#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PathResolver
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
    static fs::path GetLocalAppDataDir (const std::wstring & appName);
};
