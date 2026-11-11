#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/CollectionQuery.h"
#include "CassoExplorer/Model/CollectionResults.h"
#include "Seams/IShellItemVerbs.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionResultsItems
//
//  Disk list's and Find duplicates' results as a shell folder's items. A
//  disk is an image the list opens, by its path or, inside a container, by
//  its ContainerPath. A file in a duplicate set is a file in its image, as
//  a search's result inside an image is. Each row has its line of Save as
//  CSV, so the window writes what `casso disk list` and `casso disk dupes`
//  write.
//
//  Read walks the folder on a listing's thread. Disk list's rows arrive
//  as the disks are found; Find duplicates' when the walk ends.
//
////////////////////////////////////////////////////////////////////////////////

class CollectionResultsItems
{
public:
    using Item = IShellItemVerbs::ShellFolderItem;

    static Item  MakeDiskItem    (const CollectionDisk & disk);
    static Item  MakeFileItem    (const CollectionDisk & disk, const std::string & path);
    static void  BuildDuplicates (const std::vector<CollectionDisk> & disks, const std::vector<DuplicateSet> & sets, std::vector<Item> & outItems);

    static HRESULT  Read (IDiskFileIo & io, const std::wstring & id, std::vector<Item> & outItems);

    //  The CSV of the rows, in the order the walk found them.
    static std::string  FormatCsv (CollectionQuery::Tool tool, const std::vector<CatalogRow> & rows);

    static constexpr size_t  kProgressRows = 32;

private:
    static std::wstring  GetLeaf (const std::string & path);
};
