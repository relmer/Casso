#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Location
//
//  What a tab is looking at: a host folder, a disk image, a directory inside
//  one, one of the tree's roots, the Recycle Bin, or any other folder the
//  shell has, such as Gallery or Network. The image path is a host path; the
//  inner path is the directory's name as the volume stores it. A root's path
//  is its id in the tree, such as TreeModel::kThisPcRootId, and the Recycle
//  Bin's is its own id in the tree. A shell folder's path is the shell's own
//  name for it, which reads nothing like a path, so it carries the name it
//  shows as well; the label plays no part in comparing two locations.
//
////////////////////////////////////////////////////////////////////////////////

struct Location
{
    enum class Kind { None, HostFolder, DiskImage, DiskDirectory, Root, RecycleBin, ShellFolder };

    Kind          kind = Kind::None;
    std::wstring  path;
    std::string   innerPath;
    std::wstring  label;

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
    static Location  MakeRecycleBin    ()                                                       { return Location { Kind::RecycleBin,    kRecycleBinId, std::string() }; }
    static Location  MakeShellFolder   (const std::wstring & id, const std::wstring & label)    { return Location { Kind::ShellFolder,   id,   std::string(), label }; }

    //  Whether text is the shell's own name for a folder rather than a path:
    //  a class id ("::{...}") or a shell: name.
    static bool      IsShellName       (const std::wstring & text)                              { return text.starts_with (L"::") || _wcsnicmp (text.c_str(), L"shell:", 6) == 0; }

    static constexpr const wchar_t *  kRecycleBinId = L"bin:";
};
