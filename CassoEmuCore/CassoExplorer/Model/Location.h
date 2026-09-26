#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Location
//
//  What a tab is looking at: a host folder, a disk image, a directory inside
//  one, or one of the tree's roots. The image path is a host path; the inner
//  path is the directory's name as the volume stores it. A root's path is its
//  id in the tree, such as TreeModel::kThisPcRootId.
//
////////////////////////////////////////////////////////////////////////////////

struct Location
{
    enum class Kind { None, HostFolder, DiskImage, DiskDirectory, Root };

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
    static Location  MakeRoot          (const std::wstring & id)                                { return Location { Kind::Root,          id,   std::string() }; }
};
