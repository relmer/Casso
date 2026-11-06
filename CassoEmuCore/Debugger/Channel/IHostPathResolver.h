#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HostKnownFolder
//
////////////////////////////////////////////////////////////////////////////////

enum class HostKnownFolder
{
    Documents,
    Startup,
    CommonStartup,
};





////////////////////////////////////////////////////////////////////////////////
//
//  IHostPathResolver
//
//  The host file system queries behind the debug channel's file access
//  policy: where an existing file or folder is once links and junctions are
//  followed, whether a path exists, and where a known folder is. The policy's
//  decisions are made on the results, so a test supplies them and the
//  decisions run without a file system.
//
//  A final path is a full path with a drive letter, or a UNC path written
//  `\\server\share\...` for a file on the network, as a mapped drive's files
//  are. Neither has the `\\?\` prefix.
//
////////////////////////////////////////////////////////////////////////////////

class IHostPathResolver
{
public:
    virtual ~IHostPathResolver() = default;

    //  False when nothing exists at the path.
    virtual bool  TryGetFinalPath   (const std::wstring & path, std::wstring & finalPath) = 0;

    //  A file or a folder.
    virtual bool  Exists            (const std::wstring & path) = 0;

    virtual bool  TryGetKnownFolder (HostKnownFolder folder, std::wstring & path) = 0;
};
