#include "Pch.h"

#include "CassoExplorer/Browser/BrowserRowPaths.h"
#include "CassoExplorer/Browser/BrowserLoader.h"
#include "CassoExplorer/Model/BrowserModel.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Archives/ContainerPath.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"
#include "Devices/Disk/Partitions/PhysicalCards.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowPaths::TryGetRowLocation
//
//  A root's child, a folder in a host folder, or a disk image in one that can
//  be listed.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserRowPaths::TryGetRowLocation (const Listing & listing, int row, Location & outLocation)
{
    const Location  & location = listing.location;
    bool              found    = true;



    if (row < 0 || row >= (int) listing.rows.size())
    {
        found = false;
    }
    else if (listing.isPartitionList)
    {
        //  A partition of an image opens as the list of its volumes, and a
        //  volume or a card's partition as its catalog.
        outLocation = (listing.rows[row].isPartition && !PhysicalCards::IsOnCard (listing.rows[row].containerPath)) ? Location::MakePartition (listing.rows[row].containerPath)
                                                                                                         : Location::MakeDiskImage (listing.rows[row].containerPath);
    }
    else if (!listing.isVolume && !listing.rootChildren.empty() && listing.rows[row].sourceIndex < listing.rootChildren.size())
    {
        //  Not for a disk's entries, whatever root was listed before.
        outLocation = listing.rootChildren[listing.rows[row].sourceIndex].location;
    }
    else if ((location.kind == Location::Kind::DiskImage || location.kind == Location::Kind::DiskDirectory) && listing.rows[row].isDirectory)
    {
        //  A directory inside an image opens below the one listed.
        std::string  inner = (location.kind == Location::Kind::DiskDirectory) ? location.innerPath : std::string();

        inner       += (inner.empty() ? "" : "/") + TextEncoding::WideToNarrow (listing.rows[row].name);
        outLocation  = Location::MakeDiskDirectory (location.path, inner);
    }
    else if (location.kind == Location::Kind::DiskImage || location.kind == Location::Kind::DiskDirectory)
    {
        //  A file on an image that is itself an archive or an image, such as
        //  a ShrinkIt archive, opens in place, as one in a ZIP file does.
        std::wstring  inner = (location.kind == Location::Kind::DiskDirectory) ? TextEncoding::NarrowToWide (location.innerPath) + L"/" : std::wstring();
        std::wstring  path  = location.path + (wchar_t) ContainerPath::kPathSeparator + inner + listing.rows[row].name;

        found = listing.canListImage (path);

        if (found)
        {
            outLocation = Location::MakeDiskImage (path);
        }
    }
    else if (location.kind == Location::Kind::AppleDrive && listing.rows[row].isDirectory)
    {
        outLocation = Location::MakeAppleDrive (location.path, location.innerPath + (location.innerPath.empty() ? "" : "/") + TextEncoding::WideToNarrow (listing.rows[row].name));
    }
    else if (location.kind == Location::Kind::ShellFolder)
    {
        //  A folder on a disk opens as one, with everything a disk folder has;
        //  any other folder opens through the shell.
        const CatalogRow &  item = listing.rows[row];

        if (item.isDirectory)
        {
            outLocation = item.hostPath.empty() ? Location::MakeShellFolder (item.shellId, item.name) : Location::MakeHostFolder (item.hostPath);
        }
        else if (item.isDiskImage && listing.canListImage (BrowserLoader::GetImagePath (item)))
        {
            outLocation = Location::MakeDiskImage (BrowserLoader::GetImagePath (item));
        }
        else
        {
            found = false;
        }
    }
    else if (location.kind == Location::Kind::Root && location.path == TreeModel::kCassoRootId)
    {
        //  The Casso node's images, each by its own path.
        found = listing.rows[row].isDiskImage && !listing.rows[row].hostPath.empty() && listing.canListImage (listing.rows[row].hostPath);

        if (found)
        {
            outLocation = Location::MakeDiskImage (listing.rows[row].hostPath);
        }
    }
    else if (location.kind != Location::Kind::HostFolder)
    {
        found = false;
    }
    else if (listing.rows[row].isDirectory)
    {
        outLocation = Location::MakeHostFolder (JoinPath (location.path, listing.rows[row].name));
    }
    else if (listing.rows[row].isDiskImage && listing.canListImage (JoinPath (location.path, listing.rows[row].name)))
    {
        outLocation = Location::MakeDiskImage (JoinPath (location.path, listing.rows[row].name));
    }
    else if (listing.rows[row].isArchive)
    {
        outLocation = Location::MakeShellFolder (JoinPath (location.path, listing.rows[row].name), listing.rows[row].name);
    }
    else
    {
        found = false;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowPaths::TryGetRowPath
//
//  A host item's full path; an entry inside an image after the image's
//  address, as the address bar writes a directory inside one.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserRowPaths::TryGetRowPath (const Listing & listing, int row, std::wstring & outPath)
{
    const Location  & location = listing.location;
    bool              found    = row >= 0 && row < (int) listing.rows.size();



    if (!found)
    {
        return false;
    }

    if (listing.rows[row].cassoLinkItem != CatalogRow::CassoLinkItem::None)
    {
        outPath = listing.rows[row].hostPath;
        found   = listing.rows[row].cassoLinkItem != CatalogRow::CassoLinkItem::AddMachine;
    }
    else if (listing.isVolume)
    {
        //  A disk's entry, in an image or on a linked drive, goes by the
        //  disk's address, whatever root was listed before.
        outPath = JoinPath (BrowserModel::FormatAddress (location), listing.rows[row].name);
    }
    else if (!listing.rootChildren.empty() && listing.rows[row].sourceIndex < listing.rootChildren.size())
    {
        outPath = BrowserModel::FormatAddress (listing.rootChildren[listing.rows[row].sourceIndex].location);
    }
    else if ((listing.rows[row].homeGroup > 0 || (location.kind == Location::Kind::Root && location.path == TreeModel::kCassoRootId)) && !listing.rows[row].hostPath.empty())
    {
        //  Home's items and the Casso node's images, each by its own path.
        outPath = listing.rows[row].hostPath;
    }
    else if (location.kind == Location::Kind::HostFolder)
    {
        outPath = JoinPath (location.path, listing.rows[row].name);
    }
    else if (location.kind == Location::Kind::ShellFolder && !listing.rows[row].hostPath.empty())
    {
        outPath = listing.rows[row].hostPath;
    }
    else
    {
        found = false;
    }

    return found && !outPath.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowPaths::GetEntryPath
//
////////////////////////////////////////////////////////////////////////////////

std::string BrowserRowPaths::GetEntryPath (const Location & location, const FileEntry & entry)
{
    if ((location.kind == Location::Kind::DiskDirectory || location.kind == Location::Kind::AppleDrive) && !location.innerPath.empty())
    {
        return location.innerPath + "/" + entry.name;
    }

    return entry.name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowPaths::JoinPath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserRowPaths::JoinPath (const std::wstring & folder, const std::wstring & name)
{
    std::wstring  joined = folder;



    if (!joined.empty() && joined.back() != L'\\')
    {
        joined += L'\\';
    }

    return joined + name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowPaths::GetParentFolder
//
//  A drive root such as C:\ has no parent; the folder directly under it
//  keeps the trailing separator, since C: alone means the current directory.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BrowserRowPaths::GetParentFolder (const std::wstring & path)
{
    std::wstring  trimmed = path;
    size_t        slash   = 0;



    while (trimmed.size() > 3 && trimmed.back() == L'\\')
    {
        trimmed.pop_back();
    }

    if (trimmed.size() <= 3)
    {
        return std::wstring();
    }

    slash = trimmed.rfind (L'\\');

    if (slash == std::wstring::npos)
    {
        return std::wstring();
    }

    return (slash <= 2) ? trimmed.substr (0, slash + 1) : trimmed.substr (0, slash);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowPaths::IsPartitionVolume
//
//  True for the path of a partition's volume, whose last step is a
//  partition step such as `#3.po`, and false for a file in an archive.
//
////////////////////////////////////////////////////////////////////////////////

bool BrowserRowPaths::IsPartitionVolume (const std::wstring & path)
{
    size_t  bar   = path.rfind (L'|');
    int     index = 0;



    return bar != std::wstring::npos && PartitionAccess::TryParseStepName (TextEncoding::WideToNarrow (path.substr (bar + 1)), index);
}





