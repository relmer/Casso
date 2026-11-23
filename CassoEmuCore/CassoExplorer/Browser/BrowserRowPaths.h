#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/Location.h"

struct CatalogRow;
struct FileEntry;
struct TreeNode;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowPaths
//
//  The location a list row opens and the row's path, from the listing the
//  row is in, and the host path math the browser uses.
//
////////////////////////////////////////////////////////////////////////////////

class BrowserRowPaths
{
public:
    //  The listing a row is read from: the location listed, its rows, a
    //  root's children, what kind of list it is, and whether an image can
    //  be listed.
    struct Listing
    {
        Location                                     location;
        const std::vector<CatalogRow>              & rows;
        const std::vector<TreeNode>                & rootChildren;
        bool                                         isPartitionList = false;
        bool                                         isVolume        = false;
        std::function<bool (const std::wstring &)>   canListImage;
    };

    //  The location a row opens, without opening it: a folder, a disk image,
    //  or a directory inside one. False for anything else.
    static bool  TryGetRowLocation (const Listing & listing, int row, Location & outLocation);

    //  A row's path, as Copy as path copies it: a host item's full path, or an
    //  entry inside an image after the image's address.
    static bool  TryGetRowPath     (const Listing & listing, int row, std::wstring & outPath);

    //  An entry's path as the volume layer addresses it: its name in the volume
    //  directory, or the listed subdirectory's path and then its name.
    static std::string   GetEntryPath      (const Location & location, const FileEntry & entry);

    //  The folder a host path is in, or empty at a drive root.
    static std::wstring  GetParentFolder   (const std::wstring & path);
    static std::wstring  JoinPath          (const std::wstring & folder, const std::wstring & name);
    static bool          IsPartitionVolume (const std::wstring & path);
};
