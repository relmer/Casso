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

    // Get the executable's directory
    static fs::path GetExecutableDirectory ();

    // Get the current working directory
    static fs::path GetWorkingDirectory ();
};
