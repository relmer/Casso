#include "Pch.h"

#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/CollectionQuery.h"
#include "CassoExplorer/Model/CassoImagesListing.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "CassoExplorer/Model/StockDisksListing.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Archives/ContainerPath.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"
#include "Devices/Disk/Partitions/PhysicalCards.h"

#pragma comment(lib, "shlwapi.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::CassoExplorerBrowser
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerBrowser::CassoExplorerBrowser (IFileSystem & fs, IDiskFileIo & fileIo)
    : m_fs         (fs),
      m_fileIo     (fileIo),
      m_operations (fileIo)
{
    //  The tree reads network folders through the same readers as the list,
    //  under a key of its own.
    m_tree.SetHostFetch ([this] (const std::wstring & path, std::vector<FileSystemEntry> & outEntries, HRESULT & outResult)
    {
        if (!m_loader.m_hostListings.HasLister())
        {
            outResult = m_fs.EnumerateAllEntries (path, outEntries);
            return true;
        }

        return m_loader.m_hostListings.TryTake (s_kHostTreeKey + path, path, outEntries, outResult);
    });

    m_tree.GetImageNodes().SetCatalog (&m_catalog);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetTreeRoots
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::GetTreeRoots (std::vector<DxuiTreeNode> & outNodes)
{
    std::vector<TreeNode>  roots;



    m_tree.GetRoots (roots);
    outNodes.clear();

    for (const TreeNode & node : roots)
    {
        m_loader.m_nodes[node.id] = node;
        outNodes.push_back (BrowserCells::ToTreeNode (node, m_shellIcons));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetTreeChildren
//
//  A node whose children will not load has none; the reason is on the node
//  itself, and shows in the list when the node is selected.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiTreeNode> CassoExplorerBrowser::GetTreeChildren (const std::wstring & id)
{
    HRESULT                    hr = S_OK;
    std::vector<TreeNode>      children;
    std::vector<DxuiTreeNode>  out;



    hr = m_tree.GetChildren (id, children);

    if (FAILED (hr))
    {
        return out;
    }

    for (const TreeNode & node : children)
    {
        m_loader.m_nodes[node.id] = node;
        out.push_back (BrowserCells::ToTreeNode (node, m_shellIcons));
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetLocation
//
////////////////////////////////////////////////////////////////////////////////

Location CassoExplorerBrowser::GetLocation() const
{
    return m_model.HasTabs() ? m_model.GetActiveTab().location : Location();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetRootId
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerBrowser::GetRootId() const
{
    Location  location = GetLocation();



    return (location.kind == Location::Kind::Root) ? location.path : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::SelectTreeNode
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerBrowser::SelectTreeNode (const std::wstring & id)
{
    auto      found    = m_loader.m_nodes.find (id);
    Location  location;



    if (found != m_loader.m_nodes.end())
    {
        location = found->second.location;
    }

    if (!m_model.HasTabs())
    {
        m_model.OpenTab (location);
    }
    else if (m_model.GetActiveTab().location != location)
    {
        m_model.NavigateTo (location);
    }

    //  An image's tree node is read when it is selected or expanded, so the
    //  list can state why one doesn't open.
    if (found != m_loader.m_nodes.end() && found->second.kind == TreeNode::Kind::DiskImage && found->second.loadError.empty())
    {
        HRESULT  hr = m_tree.ReadImageNode (found->second);

        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (found != m_loader.m_nodes.end() && !found->second.loadError.empty())
    {
        m_rows.clear();
        m_selectedRows.clear();
        m_loader.m_listError = found->second.loadError;
        m_previewer.UpdatePreview();
        m_cells.UpdateStatus();

        return S_OK;
    }

    return Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::Refresh
//
//  A directory inside an image shows the image's catalog: the volume reader
//  enumerates the volume as a whole.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerBrowser::Refresh()
{
    HRESULT   hr       = S_OK;
    Location  location = GetLocation();



    m_rows.clear();
    m_loader.m_hostEntries.clear();
    m_loader.m_rootChildren.clear();
    m_listing      = VolumeListing();
    m_kind         = VolumeKind::Unknown;
    m_isImage      = false;
    m_isAppleDrive = false;
    m_isPartitionList = false;
    m_listingKept  = false;
    m_loader.m_writeProtected = false;
    m_loader.m_listError.clear();
    m_selectedRows.clear();
    m_loader.m_shellParent = Location();

    m_loader.RevisitLink (location);

    //  Returning to a tab on Home lists what Home last read, without reading it
    //  again.
    if (m_returningToTab && IsHomeLocation() && m_rowOrder.HasHomeRows())
    {
        m_loader.m_shellParent = m_homeParent;
        m_columnFilters.MarkRowsLoaded();
        m_rowOrder.SortRows();
        m_previewer.UpdatePreview();
        m_cells.UpdateStatus();

        return S_OK;
    }

    switch (location.kind)
    {
        case Location::Kind::HostFolder:
            //  A location with no path, as a new tab has, is Home.
            hr = location.path.empty() ? m_loader.LoadShellFolder (IShellItemVerbs::kHomeId) : m_loader.LoadHostFolder (location.path);
            break;

        case Location::Kind::DiskImage:
        case Location::Kind::DiskDirectory:
            hr = m_loader.LoadImage (location.path, (location.kind == Location::Kind::DiskDirectory) ? location.innerPath : std::string());
            break;

        case Location::Kind::Partition:
            hr = m_loader.LoadPartition (location.path);
            break;

        case Location::Kind::Root:
            hr = m_loader.LoadRoot (location.path);
            break;

        case Location::Kind::RecycleBin:
            hr = m_loader.LoadRecycleBin();
            break;

        case Location::Kind::ShellFolder:
            hr = m_loader.LoadShellFolder (IsHomeLocation() ? std::wstring (IShellItemVerbs::kHomeId) : location.path);
            break;

        case Location::Kind::AppleDrive:
            hr = m_loader.LoadAppleDrive (location);
            break;

        //  No location, as a new tab has, is Home.
        default:
            hr = m_loader.LoadShellFolder (IShellItemVerbs::kHomeId);
            break;
    }

    //  A picker lists the host rows in its scope; a disk's entries and a
    //  root's drives aren't host rows.
    m_scopedOut = 0;

    if (m_rowScope && (location.kind == Location::Kind::HostFolder || (location.kind == Location::Kind::Root && location.path == TreeModel::kCassoRootId)))
    {
        m_scopedOut = std::erase_if (m_rows, [this] (const CatalogRow & row) { return !m_rowScope (row); });
    }

    m_nameFilter.CaptureSource (location, m_rowScope != nullptr, m_rows.size());

    if (!m_mountedImages.empty() && (location.kind == Location::Kind::HostFolder || (location.kind == Location::Kind::Root && location.path == TreeModel::kCassoRootId)))
    {
        for (CatalogRow & row : m_rows)
        {
            std::wstring  path  = !row.hostPath.empty() ? row.hostPath : JoinPath (location.path, row.name);
            auto          found = row.isDirectory ? m_mountedImages.end() : m_mountedImages.find (CassoImagesListing::MakePathKey (path));

            row.inDrive = (found != m_mountedImages.end()) ? found->second : std::wstring();
        }
    }

    if (m_nameFilter.IsFilterable())
    {
        m_nameFilter.SetSource (std::move (m_rows));
        m_nameFilter.Apply (m_rows);
    }

    m_columnFilters.MarkRowsLoaded();

    if (IsHomeLocation())
    {
        m_rowOrder.SetHomeRows (m_rows);
        m_homeParent = m_loader.m_shellParent;
    }

    m_rowOrder.SortRows();
    m_previewer.UpdatePreview();
    m_cells.UpdateStatus();

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::SetShellVerbs
//
//  The tree reads shell folders through the same readers as the list, each
//  under a key of its own.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::SetShellVerbs (IShellItemVerbs * verbs)
{
    m_shellVerbs = verbs;
    m_shellListings.SetLister ((verbs != nullptr) ? verbs->GetShellLister() : IShellItemVerbs::ShellLister());
    m_shellListings.SetProgressSetter ([] (CassoExplorerShellListings::Progress progress) { IShellItemVerbs::s_listingProgress = std::move (progress); });
    m_shellListings.SetStoppedSetter  ([] (CassoExplorerShellListings::Stopped stopped)   { IShellItemVerbs::s_listingStopped  = std::move (stopped); });
    m_tree.SetShellVerbs (verbs);
    m_hostFolders.SetShellVerbs (verbs);
    m_navigator.SetShellVerbs (verbs);

    m_tree.SetShellFetch ([this] (const std::wstring & id, std::vector<IShellItemVerbs::ShellFolderItem> & outItems)
    {
        HRESULT  hr = S_OK;

        return m_shellListings.TryTake (s_kTreeKey + id, id, outItems, hr);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::QueueVolumeFacts
//
//  Each image row's file system and volume name: the ones already read go
//  in the row now, and the rest are queued for the worker, in place of the
//  last folder's. `folder` is the folder the rows are in, or empty for rows
//  with their own path.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::QueueVolumeFacts (const std::wstring & folder)
{
    std::vector<ImageVolumeFacts::Request>  requests;
    VolumeFacts                             facts;



    for (CatalogRow & row : m_rows)
    {
        std::wstring  path = folder.empty() ? row.hostPath : JoinPath (folder, row.name);



        if (!row.isDiskImage || row.isDirectory || path.empty())
        {
            continue;
        }

        if (m_volumeFacts.TryGet (path, row.sizeBytes, row.modifiedUnix, facts))
        {
            row.collection.fileSystem = facts.fileSystem;
            row.collection.volumeName = facts.volumeName;
            continue;
        }

        requests.push_back (ImageVolumeFacts::Request { path, row.sizeBytes, row.modifiedUnix });
    }

    m_volumeFacts.Queue (requests);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::ApplyVolumeFacts
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::ApplyVolumeFacts()
{
    VolumeFacts  facts;
    Location     at        = GetLocation();
    bool         isChanged = false;



    for (CatalogRow & row : m_rows)
    {
        std::wstring  path = !row.hostPath.empty()                  ? row.hostPath
                           : (at.kind == Location::Kind::HostFolder) ? JoinPath (at.path, row.name)
                                                                     : std::wstring();



        if (!row.isDiskImage || row.isDirectory || path.empty() || !row.collection.fileSystem.empty())
        {
            continue;
        }

        if (m_volumeFacts.TryGet (path, row.sizeBytes, row.modifiedUnix, facts) && !(facts == VolumeFacts()))
        {
            row.collection.fileSystem = facts.fileSystem;
            row.collection.volumeName = facts.volumeName;
            isChanged                 = true;
        }
    }

    return isChanged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::IsHomeLocation
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::IsHomeLocation() const
{
    Location  location = GetLocation();



    return location.kind == Location::Kind::None
        || (location.kind == Location::Kind::HostFolder && location.path.empty())
        || (location.kind == Location::Kind::ShellFolder && (_wcsicmp (location.path.c_str(), IShellItemVerbs::kHomeId) == 0 || _wcsicmp (location.path.c_str(), IShellItemVerbs::kHomeTreeId) == 0));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::SetHomeTab
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::SetHomeTab (int tab)
{
    m_rowOrder.SetHomeTab (tab);
    m_selectedRows.clear();

    m_rowOrder.SortRows();
    m_previewer.UpdatePreview();
    m_cells.UpdateStatus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::RequestHomeRead
//
//  Reads Home in the background, and the list takes the result when it arrives
//  with the selection kept. A result that arrived while another tab was shown
//  is out of date, so it is dropped for a new read.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::RequestHomeRead()
{
    std::wstring                                   id  = IShellItemVerbs::kHomeId;
    std::vector<IShellItemVerbs::ShellFolderItem>  items;
    HRESULT                                        hr  = S_OK;



    if (m_shellListings.TryTake (s_kListKey + id, id, items, hr))
    {
        bool  started = m_shellListings.TryTake (s_kListKey + id, id, items, hr);

        IGNORE_RETURN_VALUE (started, false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::RemoveHomeRow
//
//  Takes one item off Home without reading Home again.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::RemoveHomeRow (const std::wstring & shellId)
{
    m_rowOrder.RemoveHomeRow (shellId);

    m_selectedRows.clear();

    m_rowOrder.SortRows();
    m_previewer.UpdatePreview();
    m_cells.UpdateStatus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::SelectRowsByKeys
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::SelectRowsByKeys (const std::vector<std::wstring> & names)
{
    std::unordered_set<std::wstring>  wanted (names.begin(), names.end());
    std::vector<std::wstring>         keys;
    std::vector<int>                  rows;
    size_t                            row = 0;



    GetRowKeys (keys);

    for (row = 0; row < keys.size(); row++)
    {
        if (wanted.find (keys[row]) != wanted.end())
        {
            rows.push_back ((int) row);
        }
    }

    SetSelectedRows (rows);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::ReturnToTab
//
//  Returning to a tab lists what was last read there; Refresh, or going to
//  the drive again, reads its disks.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::ReturnToTab (const std::vector<std::wstring> & rowKeys)
{
    m_loader.m_linkVisit = GetLocation();
    m_returningToTab     = true;
    m_loader.ReloadAfterNavigation();
    m_returningToTab     = false;
    SelectRowsByKeys (rowKeys);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetRowKeys
//
//  ONE PASS, NOT ONE PER ROW. GetSelectionKey counts a name's earlier
//  occurrences by walking every row, so asking it for each row in turn walked
//  the listing once per file, and a folder of a few thousand cost millions of
//  comparisons. The occurrences are counted as the rows go by instead.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::GetRowKeys (std::vector<std::wstring> & outKeys) const
{
    std::unordered_map<std::wstring, size_t>  seen;



    outKeys.clear();
    outKeys.reserve (m_rows.size());

    for (const CatalogRow & row : m_rows)
    {
        if (row.hostPath.starts_with (TreeModel::kLinkRootTag))
        {
            outKeys.push_back (row.hostPath);
            continue;
        }

        size_t  occurrence = seen[row.name]++;

        outKeys.push_back (std::to_wstring (occurrence) + L":" + row.name);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetSelectionKey
//
//  The occurrence counts rows of the same name that come before this one in
//  the listing rather than in the sorted view, so the key stays the same
//  across a re-sort and a reload. GetRowKeys must build the same keys.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerBrowser::GetSelectionKey (size_t row) const
{
    size_t  occurrence = 0;



    //  A linked machine's or drive's row is renamed once its disk is read,
    //  so it is found again by its id.
    if (m_rows[row].hostPath.starts_with (TreeModel::kLinkRootTag))
    {
        return m_rows[row].hostPath;
    }

    for (const CatalogRow & other : m_rows)
    {
        occurrence += (other.name == m_rows[row].name && other.sourceIndex < m_rows[row].sourceIndex) ? 1 : 0;
    }

    return std::to_wstring (occurrence) + L":" + m_rows[row].name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::SetSelectedRows
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::SetSelectedRows (const std::vector<int> & rows)
{
    std::vector<std::wstring>  names;
    std::set<std::wstring>     items;



    m_selectedRows.clear();

    //  An item that several groups list is acted on once, whichever of its
    //  rows are selected.
    for (int row : rows)
    {
        if (row >= 0 && row < (int) m_rows.size())
        {
            if (!items.insert (m_rows[(size_t) row].shellId + L"|" + m_rows[(size_t) row].hostPath + L"|" + m_rows[(size_t) row].folderPath + L"|" + m_rows[(size_t) row].name).second)
            {
                continue;
            }

            m_selectedRows.push_back (row);
            names.push_back (GetSelectionKey ((size_t) row));
        }
    }

    if (m_model.HasTabs())
    {
        m_model.SetSelection (names);
    }

    m_previewer.UpdatePreview();
    m_cells.UpdateStatus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::TryGetSelectedEntry
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::TryGetSelectedEntry (const FileEntry *& outEntry) const
{
    size_t  source = 0;



    outEntry = nullptr;

    if ((!m_isImage && !m_isAppleDrive) || m_selectedRows.size() != 1)
    {
        return false;
    }

    source = m_rows[m_selectedRows[0]].sourceIndex;

    if (source >= m_listing.entries.size())
    {
        return false;
    }

    outEntry = &m_listing.entries[source];

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::SetNameFilter
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::SetNameFilter (const std::wstring & text)
{
    if (!m_nameFilter.SetText (text))
    {
        return;
    }

    m_selectedRows.clear();
    m_nameFilter.Apply (m_rows);

    m_columnFilters.MarkRowsLoaded();
    m_rowOrder.SortRows();
    m_previewer.UpdatePreview();
    m_cells.UpdateStatus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::SetMountedImages
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::SetMountedImages (const std::map<std::wstring, std::wstring> & mounted)
{
    m_mountedImages.clear();

    for (const auto & [path, drive] : mounted)
    {
        m_mountedImages[CassoImagesListing::MakePathKey (path)] = drive;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::JoinPath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerBrowser::JoinPath (const std::wstring & folder, const std::wstring & name)
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
//  CassoExplorerBrowser::IsPartitionVolume
//
//  True for the path of a partition's volume, whose last step is a
//  partition step such as `#3.po`, and false for a file in an archive.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::IsPartitionVolume (const std::wstring & path)
{
    size_t  bar   = path.rfind (L'|');
    int     index = 0;



    return bar != std::wstring::npos && PartitionAccess::TryParseStepName (TextEncoding::WideToNarrow (path.substr (bar + 1)), index);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetParentFolder
//
//  A drive root such as C:\ has no parent; the folder directly under it
//  keeps the trailing separator, since C: alone means the current directory.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerBrowser::GetParentFolder (const std::wstring & path)
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
//  CassoExplorerBrowser::OpenRow
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::OpenRow (int row)
{
    Location  target;



    if (row >= 0 && (size_t) row < m_rows.size() && m_rows[(size_t) row].isMissing)
    {
        return false;
    }

    //  A search's match inside a disk image opens the image with the file
    //  selected, where it previews and copies as any of the image's files.
    if (row >= 0 && (size_t) row < m_rows.size() && !m_rows[(size_t) row].imagePath.empty())
    {
        std::wstring  name      = m_rows[(size_t) row].name;
        std::wstring  image     = m_rows[(size_t) row].imagePath;
        std::string   directory = m_rows[(size_t) row].imageDirectory;

        //  A match in a directory opens that directory.
        m_model.NavigateTo (directory.empty() ? Location::MakeDiskImage (image) : Location::MakeDiskDirectory (image, directory));
        m_loader.ReloadAfterNavigation();
        SelectRowsByKeys ({ L"0:" + name });
        return true;
    }

    //  A disk image opens as one even when it cannot be listed, so the reason
    //  appears in the list rather than another program opening it.
    if (!TryGetRowLocation (row, target) && !m_loader.TryGetImageLocation (row, target))
    {
        return false;
    }

    m_model.NavigateTo (target);
    m_loader.ReloadAfterNavigation();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetEntryPath
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoExplorerBrowser::GetEntryPath (const FileEntry & entry) const
{
    Location  location = GetLocation();



    if ((location.kind == Location::Kind::DiskDirectory || location.kind == Location::Kind::AppleDrive) && !location.innerPath.empty())
    {
        return location.innerPath + "/" + entry.name;
    }

    return entry.name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::TryGetRowLocation
//
//  A root's child, a folder in a host folder, or a disk image in one that can
//  be listed.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::TryGetRowLocation (int row, Location & outLocation)
{
    Location  location = GetLocation();
    bool      found    = true;



    if (row < 0 || row >= (int) m_rows.size())
    {
        found = false;
    }
    else if (m_isPartitionList)
    {
        //  A partition of an image opens as the list of its volumes, and a
        //  volume or a card's partition as its catalog.
        outLocation = (m_rows[row].isPartition && !PhysicalCards::IsOnCard (m_rows[row].containerPath)) ? Location::MakePartition (m_rows[row].containerPath)
                                                                                                         : Location::MakeDiskImage (m_rows[row].containerPath);
    }
    else if (!IsVolumeLocation() && !m_loader.m_rootChildren.empty() && m_rows[row].sourceIndex < m_loader.m_rootChildren.size())
    {
        //  Not for a disk's entries, whatever root was listed before.
        outLocation = m_loader.m_rootChildren[m_rows[row].sourceIndex].location;
    }
    else if ((location.kind == Location::Kind::DiskImage || location.kind == Location::Kind::DiskDirectory) && m_rows[row].isDirectory)
    {
        //  A directory inside an image opens below the one listed.
        std::string  inner = (location.kind == Location::Kind::DiskDirectory) ? location.innerPath : std::string();

        inner       += (inner.empty() ? "" : "/") + TextEncoding::WideToNarrow (m_rows[row].name);
        outLocation  = Location::MakeDiskDirectory (location.path, inner);
    }
    else if (location.kind == Location::Kind::DiskImage || location.kind == Location::Kind::DiskDirectory)
    {
        //  A file on an image that is itself an archive or an image, such as
        //  a ShrinkIt archive, opens in place, as one in a ZIP file does.
        std::wstring  inner = (location.kind == Location::Kind::DiskDirectory) ? TextEncoding::NarrowToWide (location.innerPath) + L"/" : std::wstring();
        std::wstring  path  = location.path + (wchar_t) ContainerPath::kPathSeparator + inner + m_rows[row].name;

        found = m_loader.CanListImage (path);

        if (found)
        {
            outLocation = Location::MakeDiskImage (path);
        }
    }
    else if (location.kind == Location::Kind::AppleDrive && m_rows[row].isDirectory)
    {
        outLocation = Location::MakeAppleDrive (location.path, location.innerPath + (location.innerPath.empty() ? "" : "/") + TextEncoding::WideToNarrow (m_rows[row].name));
    }
    else if (location.kind == Location::Kind::ShellFolder)
    {
        //  A folder on a disk opens as one, with everything a disk folder has;
        //  any other folder opens through the shell.
        const CatalogRow &  item = m_rows[row];

        if (item.isDirectory)
        {
            outLocation = item.hostPath.empty() ? Location::MakeShellFolder (item.shellId, item.name) : Location::MakeHostFolder (item.hostPath);
        }
        else if (item.isDiskImage && m_loader.CanListImage (BrowserLoader::GetImagePath (item)))
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
        found = m_rows[row].isDiskImage && !m_rows[row].hostPath.empty() && m_loader.CanListImage (m_rows[row].hostPath);

        if (found)
        {
            outLocation = Location::MakeDiskImage (m_rows[row].hostPath);
        }
    }
    else if (location.kind != Location::Kind::HostFolder)
    {
        found = false;
    }
    else if (m_rows[row].isDirectory)
    {
        outLocation = Location::MakeHostFolder (JoinPath (location.path, m_rows[row].name));
    }
    else if (m_rows[row].isDiskImage && m_loader.CanListImage (JoinPath (location.path, m_rows[row].name)))
    {
        outLocation = Location::MakeDiskImage (JoinPath (location.path, m_rows[row].name));
    }
    else if (m_rows[row].isArchive)
    {
        outLocation = Location::MakeShellFolder (JoinPath (location.path, m_rows[row].name), m_rows[row].name);
    }
    else
    {
        found = false;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::TryGetRowPath
//
//  A host item's full path; an entry inside an image after the image's
//  address, as the address bar writes a directory inside one.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::TryGetRowPath (int row, std::wstring & outPath) const
{
    Location  location = GetLocation();
    bool      found    = row >= 0 && row < (int) m_rows.size();



    if (!found)
    {
        return false;
    }

    if (m_rows[row].cassoLinkItem != CatalogRow::CassoLinkItem::None)
    {
        outPath = m_rows[row].hostPath;
        found   = m_rows[row].cassoLinkItem != CatalogRow::CassoLinkItem::AddMachine;
    }
    else if (IsVolumeLocation())
    {
        //  A disk's entry, in an image or on a linked drive, goes by the
        //  disk's address, whatever root was listed before.
        outPath = JoinPath (BrowserModel::FormatAddress (location), m_rows[row].name);
    }
    else if (!m_loader.m_rootChildren.empty() && m_rows[row].sourceIndex < m_loader.m_rootChildren.size())
    {
        outPath = BrowserModel::FormatAddress (m_loader.m_rootChildren[m_rows[row].sourceIndex].location);
    }
    else if ((m_rows[row].homeGroup > 0 || (location.kind == Location::Kind::Root && location.path == TreeModel::kCassoRootId)) && !m_rows[row].hostPath.empty())
    {
        //  Home's items and the Casso node's images, each by its own path.
        outPath = m_rows[row].hostPath;
    }
    else if (location.kind == Location::Kind::HostFolder)
    {
        outPath = JoinPath (location.path, m_rows[row].name);
    }
    else if (location.kind == Location::Kind::ShellFolder && !m_rows[row].hostPath.empty())
    {
        outPath = m_rows[row].hostPath;
    }
    else
    {
        found = false;
    }

    return found && !outPath.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::IsSearchRunning
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::IsSearchRunning()
{
    Location  location = GetLocation();



    return location.kind == Location::Kind::ShellFolder && (SearchQuery::IsId (location.path) || CollectionQuery::IsId (location.path)) && m_shellListings.IsReading (s_kListKey + location.path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::StopSearch
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::StopSearch()
{
    m_shellListings.Stop (s_kListKey + GetLocation().path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetSelectedEntries
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::GetSelectedEntries (std::vector<FileEntry> & outEntries) const
{
    outEntries.clear();

    if (!IsVolumeLocation())
    {
        return;
    }

    for (int row : m_selectedRows)
    {
        size_t  source = m_rows[row].sourceIndex;

        if (source < m_listing.entries.size())
        {
            outEntries.push_back (m_listing.entries[source]);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetSelectedHostPaths
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::GetSelectedHostPaths (std::vector<std::wstring> & outPaths) const
{
    Location  location = GetLocation();



    outPaths.clear();

    //  Home's and a search's items are anywhere on the disk, and a cloud item
    //  isn't on it.
    if (IsHomeLocation() || AreSelectedRowsSearchFiles())
    {
        for (int row : m_selectedRows)
        {
            if (!m_rows[row].hostPath.empty())
            {
                outPaths.push_back (m_rows[row].hostPath);
            }
        }

        return;
    }

    if (m_isImage || location.kind != Location::Kind::HostFolder)
    {
        return;
    }

    for (int row : m_selectedRows)
    {
        outPaths.push_back (JoinPath (location.path, m_rows[row].name));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::AreSelectedRowsSearchFiles
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::AreSelectedRowsSearchFiles() const
{
    if (m_selectedRows.empty() || !SearchQuery::IsId (GetLocation().path))
    {
        return false;
    }

    for (int row : m_selectedRows)
    {
        if (row < 0 || (size_t) row >= m_rows.size() || m_rows[(size_t) row].hostPath.empty() || !m_rows[(size_t) row].imagePath.empty())
        {
            return false;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::RenameSearchRow
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::RenameSearchRow (int row, const std::wstring & newName)
{
    CatalogRow  *  renamed = (row >= 0 && (size_t) row < m_rows.size()) ? &m_rows[(size_t) row] : nullptr;
    std::wstring   oldPath;
    std::wstring   scope;
    std::wstring   query;
    bool           parsed  = SearchQuery::TryParseId (GetLocation().path, scope, query);
    bool           byName  = parsed && !SearchQuery::GetOptions (GetLocation().path).contents && !SearchQuery::GetNameWords (query).empty();



    if (renamed == nullptr || renamed->hostPath.empty())
    {
        return false;
    }

    //  A row whose new name no longer matches is removed, as in File Explorer.
    if (byName && !SearchQuery::MatchesName (newName, SearchQuery::GetNameWords (query)))
    {
        m_rows.erase (m_rows.begin() + row);
        m_selectedRows.clear();
        return false;
    }

    oldPath           = renamed->hostPath;
    renamed->name     = newName;
    renamed->label.clear();
    renamed->hostPath = (std::filesystem::path (oldPath).parent_path() / newName).wstring();
    renamed->shellId  = (renamed->shellId == oldPath) ? renamed->hostPath : renamed->shellId;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::AreSelectedRowsImages
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::AreSelectedRowsImages() const
{
    bool  search = AreSelectedRowsSearchFiles();



    if (m_isImage || m_selectedRows.empty() || (GetLocation().kind != Location::Kind::HostFolder && !search))
    {
        return false;
    }

    for (int row : m_selectedRows)
    {
        if (!m_rows[row].isDiskImage || (search && !m_rows[row].containerPath.empty()))
        {
            return false;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::Reload
//
//  When every selected row is gone, as after a delete, the row now in the
//  first one's place is selected, as File Explorer selects it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerBrowser::Reload (bool keepSelection)
{
    HRESULT                    hr    = S_OK;
    std::vector<std::wstring>  names;
    int                        first = m_selectedRows.empty() ? -1 : *std::min_element (m_selectedRows.begin(), m_selectedRows.end());



    for (int row : m_selectedRows)
    {
        names.push_back (GetSelectionKey ((size_t) row));
    }

    m_model.InvalidateAllCatalogs();
    hr = Refresh();

    if (!keepSelection || names.empty())
    {
        return hr;
    }

    SelectRowsByKeys (names);

    if (m_selectedRows.empty())
    {
        SelectRowAt (first);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::SelectRowAt
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::SelectRowAt (int row)
{
    if (row >= 0 && !m_rows.empty())
    {
        SetSelectedRows ({ (std::min) (row, (int) m_rows.size() - 1) });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::TryGetNodePath
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::TryGetNodePath (const std::wstring & id, std::wstring & outPath) const
{
    auto  found = m_loader.m_nodes.find (id);



    if (found == m_loader.m_nodes.end() || found->second.location.kind != Location::Kind::HostFolder)
    {
        return false;
    }

    outPath = found->second.location.path;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::TryGetNodeLocation
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::TryGetNodeLocation (const std::wstring & id, Location & outLocation) const
{
    auto  found = m_loader.m_nodes.find (id);
    bool  has   = found != m_loader.m_nodes.end() && found->second.location.kind != Location::Kind::None;



    if (has)
    {
        outLocation = found->second.location;
    }

    return has;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::TryGetTreeNode
//
//  A tree node as it was last read, such as an image read when it was
//  selected.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::TryGetTreeNode (const std::wstring & id, DxuiTreeNode & outNode) const
{
    auto  found = m_loader.m_nodes.find (id);
    bool  has   = found != m_loader.m_nodes.end();



    if (has)
    {
        outNode = BrowserCells::ToTreeNode (found->second, m_shellIcons);
    }

    return has;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::CanAddToCasso
//
//  Any host folder that is not already known, wherever it shows in the tree.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::CanAddToCasso (const std::wstring & id) const
{
    auto  found = m_loader.m_nodes.find (id);



    return found != m_loader.m_nodes.end()
        && found->second.location.kind == Location::Kind::HostFolder
        && found->second.kind != TreeNode::Kind::KnownFolder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::CanRemoveFromCasso
//
//  A known folder, whether or not it is still there.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::CanRemoveFromCasso (const std::wstring & id) const
{
    auto  found = m_loader.m_nodes.find (id);



    return found != m_loader.m_nodes.end() && found->second.kind == TreeNode::Kind::KnownFolder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::TryGetSelectedShellIds
//
//  The selection by the shell's names for it, when every selected row is a
//  host file or an item the shell lists; false when any is inside a disk
//  image.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::TryGetSelectedShellIds (std::vector<std::wstring> & outIds) const
{
    const std::vector<CatalogRow> &  rows = GetRows();



    outIds.clear();

    if (GetLocation().kind == Location::Kind::HostFolder)
    {
        GetSelectedHostPaths (outIds);
        return !outIds.empty();
    }

    //  CassoLink's items drag as the files they are written to.
    if (TryGetSelectedCassoLinkPaths (outIds))
    {
        return true;
    }

    if (!HasShellRows (GetLocation().kind, IsHomeLocation()))
    {
        return false;
    }

    for (int row : GetSelectedRows())
    {
        if (row < 0 || (size_t) row >= rows.size() || !rows[(size_t) row].imagePath.empty())
        {
            outIds.clear();
            return false;
        }

        outIds.push_back (rows[(size_t) row].hostPath.empty() ? rows[(size_t) row].shellId : rows[(size_t) row].hostPath);
    }

    return !outIds.empty();
}
