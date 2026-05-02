#include "Pch.h"

#include "PathResolver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BuildSearchPaths
//
////////////////////////////////////////////////////////////////////////////////

vector<fs::path> PathResolver::BuildSearchPaths (
    const fs::path & exeDir,
    const fs::path & cwd)
{
    vector<fs::path> searchBases = { exeDir, cwd };



    // Also try parent directories (handles running from x64/Debug/)
    for (const auto & base : { exeDir, cwd })
    {
        fs::path parent = base.parent_path ();

        if (!parent.empty () && parent != base)
        {
            searchBases.push_back (parent);

            fs::path grandparent = parent.parent_path ();

            if (!grandparent.empty () && grandparent != parent)
            {
                searchBases.push_back (grandparent);
            }
        }
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
