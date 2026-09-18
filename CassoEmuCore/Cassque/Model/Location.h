#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Location
//
//  What a tab is looking at: a host folder, a disk image, or a directory
//  inside one. The image path is a host path; the inner path is the
//  directory's name as the volume stores it.
//
////////////////////////////////////////////////////////////////////////////////

struct Location
{
    enum class Kind { None, HostFolder, DiskImage, DiskDirectory };

    Kind          kind = Kind::None;
    std::wstring  path;
    std::string   innerPath;

    bool operator== (const Location & other) const
    {
        return kind == other.kind
            && _wcsicmp (path.c_str(), other.path.c_str()) == 0
            && _stricmp (innerPath.c_str(), other.innerPath.c_str()) == 0;
    }

    bool operator!= (const Location & other) const { return !(*this == other); }

    static Location  MakeHostFolder   (const std::wstring & path)                              { return Location { Kind::HostFolder,    path, std::string() }; }
    static Location  MakeDiskImage    (const std::wstring & path)                              { return Location { Kind::DiskImage,     path, std::string() }; }
    static Location  MakeDiskDirectory (const std::wstring & path, const std::string & inner)   { return Location { Kind::DiskDirectory, path, inner }; }
};
