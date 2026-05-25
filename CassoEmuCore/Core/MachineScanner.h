#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineInfo
//
//  One entry per discovered machine config. `fileName` is the subdirectory
//  name (e.g. "Apple2Plus") used to construct path Machines/<Name>/<Name>.json.
//  `displayName` is the human-readable name pulled from the JSON's "name"
//  field, falling back to `fileName` when the JSON can't be parsed.
//
////////////////////////////////////////////////////////////////////////////////

struct MachineInfo
{
    wstring   displayName;
    wstring   fileName;
    int       releaseYear = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineScanner
//
//  Pure scanner for machine configs laid out as
//  Machines/<Name>/<Name>.json under one of a list of search paths. The
//  first search path that contains a Machines/ directory wins (matches
//  Main.cpp's FindFile semantics so the picker and the loader can never
//  disagree about which config the user picked).
//
//  The directory lister and file reader functors are injected. Production
//  callers pass `&MachineScanner::ListDirectory` and `&MachineScanner::ReadFile`;
//  tests pass in-memory fakes.
//
////////////////////////////////////////////////////////////////////////////////

class MachineScanner
{
public:

    using DirectoryLister = std::function<vector<fs::path> (const fs::path &)>;
    using FileReader      = std::function<HRESULT (const fs::path &, string &)>;


    static vector<MachineInfo> Scan (
        const vector<fs::path> & searchPaths,
        const DirectoryLister  & lister,
        const FileReader       & reader);


    // Production lister: returns immediate subdirectories of `dir`, or
    // an empty vector when `dir` isn't a directory.
    static vector<fs::path> ListDirectory (const fs::path & dir);


    // Production reader: slurps the entire text file at `file` into
    // `outText`. Returns E_FAIL on any I/O error.
    static HRESULT ReadFile (const fs::path & file, string & outText);
};