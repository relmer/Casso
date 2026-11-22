#include "Pch.h"

#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Browser/BrowserRowPaths.h"
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
        if (!m_loader.GetHostListings().HasLister())
        {
            outResult = m_fs.EnumerateAllEntries (path, outEntries);
            return true;
        }

        return m_loader.GetHostListings().TryTake (s_kHostTreeKey + path, path, outEntries, outResult);
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
        m_loader.GetNodes()[node.id] = node;
        outNodes.push_back (BrowserCells::ToTreeNode (node, m_sources.shellIcons));
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
        m_loader.GetNodes()[node.id] = node;
        out.push_back (BrowserCells::ToTreeNode (node, m_sources.shellIcons));
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
    auto      found    = m_loader.GetNodes().find (id);
    Location  location;



    if (found != m_loader.GetNodes().end())
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
    if (found != m_loader.GetNodes().end() && found->second.kind == TreeNode::Kind::DiskImage && found->second.loadError.empty())
    {
        HRESULT  hr = m_tree.ReadImageNode (found->second);

        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (found != m_loader.GetNodes().end() && !found->second.loadError.empty())
    {
        m_list.rows.clear();
        m_list.selectedRows.clear();
        m_list.listError = found->second.loadError;
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



    m_list.rows.clear();
    m_list.hostEntries.clear();
    m_list.rootChildren.clear();
    m_list.listing      = VolumeListing();
    m_list.kind         = VolumeKind::Unknown;
    m_list.isImage      = false;
    m_list.isAppleDrive = false;
    m_list.isPartitionList = false;
    m_list.listingKept  = false;
    m_list.writeProtected = false;
    m_list.listError.clear();
    m_list.selectedRows.clear();
    m_list.shellParent = Location();

    m_loader.RevisitLink (location);

    //  Returning to a tab on Home lists what Home last read, without reading it
    //  again.
    if (m_returningToTab && IsHomeLocation() && m_rowOrder.HasHomeRows())
    {
        m_list.shellParent = m_homeParent;
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
        m_scopedOut = std::erase_if (m_list.rows, [this] (const CatalogRow & row) { return !m_rowScope (row); });
    }

    m_nameFilter.CaptureSource (location, m_rowScope != nullptr, m_list.rows.size());

    if (!m_mountedImages.empty() && (location.kind == Location::Kind::HostFolder || (location.kind == Location::Kind::Root && location.path == TreeModel::kCassoRootId)))
    {
        for (CatalogRow & row : m_list.rows)
        {
            std::wstring  path  = !row.hostPath.empty() ? row.hostPath : BrowserRowPaths::JoinPath (location.path, row.name);
            auto          found = row.isDirectory ? m_mountedImages.end() : m_mountedImages.find (CassoImagesListing::MakePathKey (path));

            row.inDrive = (found != m_mountedImages.end()) ? found->second : std::wstring();
        }
    }

    if (m_nameFilter.IsFilterable())
    {
        m_nameFilter.SetSource (std::move (m_list.rows));
        m_nameFilter.Apply (m_list.rows);
    }

    m_columnFilters.MarkRowsLoaded();

    if (IsHomeLocation())
    {
        m_rowOrder.SetHomeRows (m_list.rows);
        m_homeParent = m_list.shellParent;
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
    m_sources.shellVerbs = verbs;
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



    for (CatalogRow & row : m_list.rows)
    {
        std::wstring  path = folder.empty() ? row.hostPath : BrowserRowPaths::JoinPath (folder, row.name);



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



    for (CatalogRow & row : m_list.rows)
    {
        std::wstring  path = !row.hostPath.empty()                  ? row.hostPath
                           : (at.kind == Location::Kind::HostFolder) ? BrowserRowPaths::JoinPath (at.path, row.name)
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
    m_list.selectedRows.clear();

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

    m_list.selectedRows.clear();

    m_rowOrder.SortRows();
    m_previewer.UpdatePreview();
    m_cells.UpdateStatus();
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
    m_loader.SetLinkVisit (GetLocation());
    m_returningToTab     = true;
    m_loader.ReloadAfterNavigation();
    m_returningToTab     = false;
    m_selection.SelectRowsByKeys (rowKeys);
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

    m_list.selectedRows.clear();
    m_nameFilter.Apply (m_list.rows);

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
//  CassoExplorerBrowser::OpenRow
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::OpenRow (int row)
{
    Location  target;



    if (row >= 0 && (size_t) row < m_list.rows.size() && m_list.rows[(size_t) row].isMissing)
    {
        return false;
    }

    //  A search's match inside a disk image opens the image with the file
    //  selected, where it previews and copies as any of the image's files.
    if (row >= 0 && (size_t) row < m_list.rows.size() && !m_list.rows[(size_t) row].imagePath.empty())
    {
        std::wstring  name      = m_list.rows[(size_t) row].name;
        std::wstring  image     = m_list.rows[(size_t) row].imagePath;
        std::string   directory = m_list.rows[(size_t) row].imageDirectory;

        //  A match in a directory opens that directory.
        m_model.NavigateTo (directory.empty() ? Location::MakeDiskImage (image) : Location::MakeDiskDirectory (image, directory));
        m_loader.ReloadAfterNavigation();
        m_selection.SelectRowsByKeys ({ L"0:" + name });
        return true;
    }

    //  A disk image opens as one even when it cannot be listed, so the reason
    //  appears in the list rather than another program opening it.
    if (!BrowserRowPaths::TryGetRowLocation (GetRowListing(), row, target) && !m_loader.TryGetImageLocation (row, target))
    {
        return false;
    }

    m_model.NavigateTo (target);
    m_loader.ReloadAfterNavigation();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetRowListing
//
////////////////////////////////////////////////////////////////////////////////

BrowserRowPaths::Listing CassoExplorerBrowser::GetRowListing()
{
    return BrowserRowPaths::Listing { GetLocation(), m_list.rows, m_list.rootChildren, m_list.isPartitionList, IsVolumeLocation(),
                                      [this] (const std::wstring & path) { return m_loader.CanListImage (path); } };
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
//  CassoExplorerBrowser::RenameSearchRow
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::RenameSearchRow (int row, const std::wstring & newName)
{
    CatalogRow  *  renamed = (row >= 0 && (size_t) row < m_list.rows.size()) ? &m_list.rows[(size_t) row] : nullptr;
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
        m_list.rows.erase (m_list.rows.begin() + row);
        m_list.selectedRows.clear();
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
    int                        first = m_list.selectedRows.empty() ? -1 : *std::min_element (m_list.selectedRows.begin(), m_list.selectedRows.end());



    for (int row : m_list.selectedRows)
    {
        names.push_back (m_selection.GetSelectionKey ((size_t) row));
    }

    m_model.InvalidateAllCatalogs();
    hr = Refresh();

    if (!keepSelection || names.empty())
    {
        return hr;
    }

    m_selection.SelectRowsByKeys (names);

    if (m_list.selectedRows.empty())
    {
        m_selection.SelectRowAt (first);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::TryGetNodePath
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::TryGetNodePath (const std::wstring & id, std::wstring & outPath) const
{
    auto  found = m_loader.GetNodes().find (id);



    if (found == m_loader.GetNodes().end() || found->second.location.kind != Location::Kind::HostFolder)
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
    auto  found = m_loader.GetNodes().find (id);
    bool  has   = found != m_loader.GetNodes().end() && found->second.location.kind != Location::Kind::None;



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
    auto  found = m_loader.GetNodes().find (id);
    bool  has   = found != m_loader.GetNodes().end();



    if (has)
    {
        outNode = BrowserCells::ToTreeNode (found->second, m_sources.shellIcons);
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
    auto  found = m_loader.GetNodes().find (id);



    return found != m_loader.GetNodes().end()
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
    auto  found = m_loader.GetNodes().find (id);



    return found != m_loader.GetNodes().end() && found->second.kind == TreeNode::Kind::KnownFolder;
}





