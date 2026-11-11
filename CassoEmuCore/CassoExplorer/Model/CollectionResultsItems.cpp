#include "Pch.h"

#include "CassoExplorer/Model/CollectionResultsItems.h"
#include "CassoExplorer/Model/DiskIdentity.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Archives/ContainerPath.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Machines/Apple2/Common/AppleFileName.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionResultsItems::MakeDiskItem
//
////////////////////////////////////////////////////////////////////////////////

CollectionResultsItems::Item CollectionResultsItems::MakeDiskItem (const CollectionDisk & disk)
{
    Item    item;
    size_t  bar   = disk.path.rfind (ContainerPath::kPathSeparator);
    size_t  slash = disk.path.find_last_of ('\\');



    item.id        = TextEncoding::NarrowToWide (disk.path);
    item.name      = GetLeaf (disk.path);
    item.typeText  = TextEncoding::NarrowToWide (disk.container);
    item.sizeBytes = disk.bytes;

    //  A host file opens by its path; a disk in a container, by the path
    //  through the containers.
    if (bar == std::string::npos)
    {
        item.path   = item.id;
        item.folder = (slash == std::string::npos) ? std::wstring() : TextEncoding::NarrowToWide (disk.path.substr (0, slash));
    }
    else
    {
        item.containerPath = item.id;
        item.folder        = TextEncoding::NarrowToWide (disk.path.substr (0, bar));
    }

    item.collection.isDisk     = true;
    item.collection.container  = item.typeText;
    item.collection.fileSystem = TextEncoding::NarrowToWide (disk.fileSystem);
    item.collection.volumeName = TextEncoding::NarrowToWide (disk.volumeName);
    item.collection.files      = disk.files;
    item.collection.freeBytes  = disk.freeBytes;
    item.collection.crc32      = TextEncoding::NarrowToWide (DiskIdentity::FormatCrc32 (disk.crc32));
    item.collection.sha256     = TextEncoding::NarrowToWide (disk.sha256);
    item.collection.hasMore    = disk.hasMore;
    item.collection.csvLine    = CollectionResults::FormatDiskRow (disk);

    return item;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionResultsItems::MakeFileItem
//
//  A file on a disk, which opens and previews in its image as a search's
//  result inside an image does.
//
////////////////////////////////////////////////////////////////////////////////

CollectionResultsItems::Item CollectionResultsItems::MakeFileItem (const CollectionDisk & disk, const std::string & path)
{
    Item    item;
    size_t  slash = path.rfind ('/');



    for (const CollectionFile & file : disk.fileList)
    {
        if (file.path != path)
        {
            continue;
        }

        item.name           = AppleFileName::ToDisplay ((slash == std::string::npos) ? path : path.substr (slash + 1));
        item.typeText       = CatalogModel::GetTypeText (file.type, disk.kind);
        item.sizeBytes      = file.bytes;
        item.imagePath      = TextEncoding::NarrowToWide (disk.path);
        item.catalogIndex   = file.catalogIndex;
        item.imageDirectory = (slash == std::string::npos) ? std::string() : path.substr (0, slash);
        item.folder         = item.imagePath + (item.imageDirectory.empty() ? std::wstring() : L"\\" + TextEncoding::NarrowToWide (item.imageDirectory));
        item.id             = item.folder + L"\\" + item.name;
        item.modifiedLocal  = true;
        break;
    }

    return item;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionResultsItems::BuildDuplicates
//
////////////////////////////////////////////////////////////////////////////////

void CollectionResultsItems::BuildDuplicates (const std::vector<CollectionDisk> & disks, const std::vector<DuplicateSet> & sets, std::vector<Item> & outItems)
{
    outItems.clear();

    for (size_t at = 0; at < sets.size(); at++)
    {
        for (const DuplicateMember & member : sets[at].members)
        {
            Item  item = sets[at].isFile ? MakeFileItem (disks[member.disk], member.file) : MakeDiskItem (disks[member.disk]);

            item.collection.set     = (int) at + 1;
            item.collection.sha256  = TextEncoding::NarrowToWide (sets[at].sha256);
            item.collection.csvLine = CollectionResults::FormatDuplicateRow ((int) at + 1, sets[at], member, disks);

            outItems.push_back (std::move (item));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionResultsItems::Read
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CollectionResultsItems::Read (IDiskFileIo & io, const std::wstring & id, std::vector<Item> & outItems)
{
    HRESULT                      hr     = S_OK;
    CollectionQuery::Tool        tool   = CollectionQuery::Tool::DiskList;
    std::wstring                 folder;
    std::vector<CollectionDisk>  disks;
    std::vector<DuplicateSet>    sets;
    CollectionWalk::Visitor      visitor;
    bool                         parsed = CollectionQuery::TryParseId (id, tool, folder);



    outItems.clear();

    CBRAEx (parsed, E_INVALIDARG);

    visitor.isStopped = [] () { return IShellItemVerbs::s_listingStopped && IShellItemVerbs::s_listingStopped(); };

    visitor.onDisk = [&disks, &outItems, tool] (CollectionDisk && disk)
    {
        if (tool == CollectionQuery::Tool::DiskList)
        {
            outItems.push_back (MakeDiskItem (disk));

            if (outItems.size() % kProgressRows == 0 && IShellItemVerbs::s_listingProgress)
            {
                IShellItemVerbs::s_listingProgress (outItems);
            }
        }
        else
        {
            disks.push_back (std::move (disk));
        }
    };

    hr = CollectionWalk::Walk (io, TextEncoding::WideToNarrow (folder), visitor);
    CHR (hr);

    if (tool == CollectionQuery::Tool::Duplicates)
    {
        CollectionResults::FindDuplicates (disks, sets);
        BuildDuplicates (disks, sets, outItems);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionResultsItems::FormatCsv
//
////////////////////////////////////////////////////////////////////////////////

std::string CollectionResultsItems::FormatCsv (CollectionQuery::Tool tool, const std::vector<CatalogRow> & rows)
{
    std::vector<const CatalogRow *>  ordered;
    std::string                      text = (tool == CollectionQuery::Tool::DiskList) ? CollectionResults::kDiskListHeader : CollectionResults::kDuplicatesHeader;



    for (const CatalogRow & row : rows)
    {
        if (!row.collection.csvLine.empty() && !row.isGroupCopy)
        {
            ordered.push_back (&row);
        }
    }

    std::sort (ordered.begin(), ordered.end(), [] (const CatalogRow * a, const CatalogRow * b) { return a->sourceIndex < b->sourceIndex; });

    for (const CatalogRow * row : ordered)
    {
        text += row->collection.csvLine;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionResultsItems::GetLeaf
//
//  A disk's name: the last step of its path, without the folders before it
//  in a ZIP or on the PC.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CollectionResultsItems::GetLeaf (const std::string & path)
{
    size_t       bar  = path.rfind (ContainerPath::kPathSeparator);
    std::string  step = (bar == std::string::npos) ? path : path.substr (bar + 1);
    size_t       cut  = step.find_last_of ("\\/");



    return TextEncoding::NarrowToWide ((cut == std::string::npos) ? step : step.substr (cut + 1));
}
