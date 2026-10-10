#include "Pch.h"

#include "Core/AppleSingleCodec.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "Core/TextEncoding.h"

#pragma comment(lib, "shlwapi.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::CassoExplorerBrowser
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerBrowser::CassoExplorerBrowser (IFileSystem & fs, IDiskFileIo & fileIo)
    : m_fs         (fs),
      m_tree       (fs),
      m_operations (fileIo)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::ToTreeNode
//
//  Collapsed, with children left to the provider, so nothing below a root is
//  read until someone expands it.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTreeNode CassoExplorerBrowser::ToTreeNode (const TreeNode & node, IShellIcons * icons)
{
    DxuiTreeNode  out;



    out.id             = node.id;
    out.label          = node.label;
    out.dividerAbove   = node.dividerAbove;

    //  A drive under its name, as Explorer's tree shows it.
    if (node.kind == TreeNode::Kind::Drive && icons != nullptr)
    {
        IShellIcons::DriveInfo  drive;

        if (icons->GetDriveInfo (node.location.path, drive) && !drive.name.empty())
        {
            out.label = drive.name;
        }
    }

    out.expanded       = false;
    out.childrenLoaded = !node.canExpand;
    //  A known folder that is gone is dimmed. An image that will not open is
    //  not: the preview says why when it is chosen.
    out.dimmed         = node.missing;
    out.iconGhosted    = node.hidden;
    out.iconBroken     = node.broken;

    if (icons != nullptr)
    {
        out.icon = GetNodeIcon (node, *icons);
    }

    return out;
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
        m_nodes[node.id] = node;
        outNodes.push_back (ToTreeNode (node, m_shellIcons));
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
        m_nodes[node.id] = node;
        out.push_back (ToTreeNode (node, m_shellIcons));
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
    auto      found    = m_nodes.find (id);
    Location  location;



    if (found != m_nodes.end())
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

    if (found != m_nodes.end() && !found->second.loadError.empty())
    {
        m_rows.clear();
        m_selectedRows.clear();
        m_listError = found->second.loadError;
        UpdatePreview();
        UpdateStatus();

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
    m_hostEntries.clear();
    m_rootChildren.clear();
    m_listing      = VolumeListing();
    m_kind         = VolumeKind::Unknown;
    m_isImage      = false;
    m_writeProtected = false;
    m_listError.clear();
    m_selectedRows.clear();
    m_shellParent = Location();

    switch (location.kind)
    {
        case Location::Kind::HostFolder:
            hr = LoadHostFolder (location.path);
            break;

        case Location::Kind::DiskImage:
        case Location::Kind::DiskDirectory:
            hr = LoadImage (location.path, (location.kind == Location::Kind::DiskDirectory) ? location.innerPath : std::string());
            break;

        case Location::Kind::Root:
            hr = LoadRoot (location.path);
            break;

        case Location::Kind::RecycleBin:
            hr = LoadRecycleBin();
            break;

        case Location::Kind::ShellFolder:
            hr = LoadShellFolder (location.path);
            break;

        default:
            break;
    }

    SortRows();
    UpdatePreview();
    UpdateStatus();

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::ReloadAfterNavigation
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::ReloadAfterNavigation()
{
    HRESULT  hr = Refresh();



    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::LoadHostFolder
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerBrowser::LoadHostFolder (const std::wstring & path)
{
    HRESULT  hr    = S_OK;
    size_t   index = 0;



    hr = m_fs.EnumerateAllEntries (path, m_hostEntries);

    if (FAILED (hr))
    {
        m_listError = L"This folder could not be read.";
        return hr;
    }

    std::erase_if (m_hostEntries, [this] (const FileSystemEntry & entry) { return !m_folderOptions.IsShown (entry); });

    for (index = 0; index < m_hostEntries.size(); index++)
    {
        const FileSystemEntry &  entry   = m_hostEntries[index];
        bool                     isImage = !entry.isFolder && TreeModel::IsSupportedImage (entry.name);

        m_rows.push_back (CatalogModel::FromHostEntry (entry, isImage, index));

        //  The shell's own type name, as Explorer's Type column shows.
        if (m_shellIcons != nullptr)
        {
            std::wstring  typeName = m_shellIcons->GetTypeName (JoinPath (path, entry.name), entry.isFolder);

            if (!typeName.empty())
            {
                m_rows.back().typeText = std::move (typeName);
            }
        }
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::LoadShellFolder
//
//  What the shell lists in a folder that is not on a disk. An item that is a
//  file or folder on one keeps its path, so it opens and previews as it does
//  in its own folder, and a disk image among them opens as an image.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerBrowser::LoadShellFolder (const std::wstring & id)
{
    HRESULT                                        hr = S_OK;
    std::vector<IShellItemVerbs::ShellFolderItem>  items;



    if (m_shellVerbs == nullptr)
    {
        m_listError = L"This folder could not be read.";
        return E_NOTIMPL;
    }

    //  A slow folder, Network above all, reads on a thread of its own; the
    //  list says so meanwhile, as Explorer's does, and fills when it arrives.
    if (!m_shellListings.TryTake (s_kListKey + id, id, items, hr))
    {
        m_listError = L"Working on it...";
        return S_OK;
    }

    if (FAILED (hr))
    {
        m_listError = L"This folder could not be read.";
        return hr;
    }

    //  Where Up goes: a folder on a disk opens as one.
    {
        IShellItemVerbs::ShellFolderItem  parent;
        HRESULT                           hrUp = m_shellVerbs->GetShellParent (id, parent);

        if (hrUp == S_OK)
        {
            m_shellParent = parent.path.empty() ? Location::MakeShellFolder (parent.id, parent.name) : Location::MakeHostFolder (parent.path);
        }
    }

    for (size_t index = 0; index < items.size(); index++)
    {
        CatalogRow  row;

        row.name         = items[index].name;
        row.typeText     = items[index].typeText;
        row.sizeBytes    = items[index].sizeBytes;
        row.modifiedUnix = items[index].modifiedUnix;
        row.hasModified  = items[index].hasModified;
        row.isDirectory  = items[index].isFolder;
        //  A folder that is a file too has no folder on the disk to open as one.
        row.hostPath     = (items[index].isFolder && items[index].isFile) ? std::wstring() : items[index].path;
        row.shellId      = items[index].id;
        row.folderPath   = items[index].folder;
        row.imagePath    = items[index].imagePath;
        row.imageCatalogIndex = items[index].catalogIndex;
        row.isDiskImage  = !row.isDirectory && !row.hostPath.empty() && TreeModel::IsSupportedImage (row.hostPath);
        row.sourceIndex  = index;

        m_rows.push_back (std::move (row));
    }

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
    m_tree.SetShellVerbs (verbs);

    m_tree.SetShellFetch ([this] (const std::wstring & id, std::vector<IShellItemVerbs::ShellFolderItem> & outItems)
    {
        HRESULT  hr = S_OK;

        return m_shellListings.TryTake (s_kTreeKey + id, id, outItems, hr);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::LoadRecycleBin
//
//  What the Recycle Bin holds on every drive, with where each item was
//  deleted from and when.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerBrowser::LoadRecycleBin()
{
    HRESULT                                     hr = S_OK;
    std::vector<IShellItemVerbs::RecycledItem>  items;



    BAIL_OUT_IF (m_shellVerbs == nullptr, S_OK);

    hr = m_shellVerbs->ListRecycled (items);

    if (FAILED (hr))
    {
        m_listError = L"The Recycle Bin could not be read.";
        return hr;
    }

    for (size_t index = 0; index < items.size(); index++)
    {
        CatalogRow  row;

        row.name           = items[index].name;
        row.typeText       = items[index].typeText;
        row.sizeBytes      = items[index].sizeBytes;
        row.modifiedUnix   = items[index].modifiedUnix;
        row.hasModified    = items[index].hasModified;
        row.isDirectory    = items[index].isFolder;
        row.recycledId     = items[index].id;
        row.originalFolder = items[index].originalFolder;
        row.deletedUnix    = items[index].deletedUnix;
        row.hasDeleted     = items[index].hasDeleted;
        row.sourceIndex    = index;

        m_rows.push_back (std::move (row));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetSelectedRecycledIds
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::GetSelectedRecycledIds (std::vector<std::wstring> & outIds) const
{
    outIds.clear();

    for (int row : m_selectedRows)
    {
        if (row >= 0 && (size_t) row < m_rows.size() && !m_rows[(size_t) row].recycledId.empty())
        {
            outIds.push_back (m_rows[(size_t) row].recycledId);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::TryGetRowEntry
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::TryGetRowEntry (int row, FileEntry & outEntry) const
{
    if (!m_isImage || row < 0 || (size_t) row >= m_rows.size() || m_rows[(size_t) row].sourceIndex >= m_listing.entries.size())
    {
        return false;
    }

    outEntry = m_listing.entries[m_rows[(size_t) row].sourceIndex];

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetAllRecycledIds
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::GetAllRecycledIds (std::vector<std::wstring> & outIds) const
{
    outIds.clear();

    for (const CatalogRow & row : m_rows)
    {
        if (!row.recycledId.empty())
        {
            outIds.push_back (row.recycledId);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetImageProblem
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerBrowser::GetImageProblem (const std::wstring & imagePath)
{
    VolumeListing           listing;
    VolumeKind              kind = VolumeKind::Unknown;
    DiskOperations::Result  result;



    if (CanListImage (imagePath))
    {
        return std::wstring();
    }

    result = m_operations.List (TextEncoding::WideToNarrow (imagePath), listing, kind);

    if (result.Succeeded() || result.message.find (s_kNoFileSystem) != std::string::npos)
    {
        return std::wstring();
    }

    return FormatImageError (imagePath, result.message);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::LoadRoot
//
//  The Casso root's known folders and This PC's drives, as folder rows.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerBrowser::LoadRoot (const std::wstring & id)
{
    HRESULT  hr    = S_OK;
    size_t   index = 0;



    hr = m_tree.GetChildren (id, m_rootChildren);
    CHR (hr);

    for (index = 0; index < m_rootChildren.size(); index++)
    {
        FileSystemEntry  entry;

        m_nodes[m_rootChildren[index].id] = m_rootChildren[index];

        entry.name     = m_rootChildren[index].label;
        entry.isFolder = true;

        m_rows.push_back (CatalogModel::FromHostEntry (entry, false, index));
        m_rows.back().hostPath = m_rootChildren[index].location.path;

        //  A drive's tile shows its name, its type and how full it is.
        if (m_rootChildren[index].kind == TreeNode::Kind::Drive && m_shellIcons != nullptr)
        {
            IShellIcons::DriveInfo  drive;

            if (m_shellIcons->GetDriveInfo (m_rootChildren[index].location.path, drive))
            {
                CatalogRow &  row = m_rows.back();

                row.name      = drive.name.empty() ? row.name : drive.name;
                row.typeText  = drive.typeName.empty() ? row.typeText : drive.typeName;
                row.isDrive   = true;
                row.sizeBytes = drive.totalBytes;
                row.freeBytes = drive.freeBytes;
            }
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::LoadImage
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerBrowser::LoadImage (const std::wstring & path, const std::string & directory)
{
    DiskOperations::Result  result;
    HRESULT                 hrReadOnly = S_OK;
    bool                    cached     = directory.empty() && m_model.TryGetCachedCatalog (path, m_listing, m_kind);



    if (!cached)
    {
        result = m_operations.List (TextEncoding::WideToNarrow (path), directory, m_listing, m_kind);

        if (!result.Succeeded())
        {
            m_listError = FormatImageError (path, result.message);
            return result.hr;
        }

        //  The cache holds an image's catalog, which is its volume directory,
        //  so a subdirectory's listing is read fresh each time.
        if (directory.empty())
        {
            m_model.CacheCatalog (path, m_listing, m_kind);
        }
    }

    m_isImage = true;
    CatalogModel::FromListing (m_listing, m_kind, m_rows);

    //  A read-only file refuses every write the runner would make, so the
    //  verbs that write are offered grayed rather than failing at the end.
    hrReadOnly = m_fs.GetReadOnlyAttribute (path, m_writeProtected);
    IGNORE_RETURN_VALUE (hrReadOnly, S_OK);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::LeaveMissingLocation
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::LeaveMissingLocation()
{
    Location      location = GetLocation();
    std::wstring  path     = location.path;
    size_t        slash    = 0;



    if (location.kind != Location::Kind::HostFolder && location.kind != Location::Kind::DiskImage &&
        location.kind != Location::Kind::DiskDirectory)
    {
        return false;
    }

    if (path.empty() || m_fs.Exists (path) || GetTreeModel().IsDirectory (path))
    {
        return false;
    }

    do
    {
        slash = path.find_last_of (L'\\');

        if (slash == std::wstring::npos)
        {
            return false;
        }

        //  A drive's root keeps its backslash.
        path = (slash <= 2) ? path.substr (0, slash + 1) : path.substr (0, slash);
    }
    while (!GetTreeModel().IsDirectory (path) && path.size() > 3);

    if (!GetTreeModel().IsDirectory (path))
    {
        return false;
    }

    NavigateToLocation (Location::MakeHostFolder (path));

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::SortRows
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::SortRows()
{
    if (!m_model.HasTabs())
    {
        return;
    }

    CatalogModel::Sort (m_rows, m_model.GetActiveTab().sortColumn, m_model.GetActiveTab().sortDescending);
    GroupRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GroupRows
//
//  Grouping orders the groups and keeps the sort within each: a stable sort
//  by group over rows already sorted by the column.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::GroupRows()
{
    RowGrouping::Field                  field      = m_model.GetActiveTab().groupBy;
    bool                                descending = m_model.GetActiveTab().groupDescending;
    RowGrouping::Today                  today      = RowGrouping::GetToday();
    std::vector<size_t>                 order;
    std::vector<RowGrouping::Group>     groups;
    std::vector<CatalogRow>             sorted;
    std::vector<RowGrouping::Group>     sortedGroups;



    m_rowGroups.clear();

    if (field == RowGrouping::Field::None)
    {
        return;
    }

    for (size_t i = 0; i < m_rows.size(); i++)
    {
        order.push_back (i);
        groups.push_back (RowGrouping::GetGroup (m_rows[i], field, today));
    }

    std::stable_sort (order.begin(), order.end(), [&] (size_t a, size_t b)
    {
        return RowGrouping::IsBefore (groups[a], groups[b], descending);
    });

    for (size_t i : order)
    {
        sorted.push_back (std::move (m_rows[i]));
        sortedGroups.push_back (groups[i]);
    }

    m_rows      = std::move (sorted);
    m_rowGroups = std::move (sortedGroups);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetListGroups
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Group> CassoExplorerBrowser::GetListGroups() const
{
    std::vector<DxuiListView::Group>  out;



    for (size_t i = 0; i < m_rowGroups.size(); i++)
    {
        if (i == 0 || !(m_rowGroups[i] == m_rowGroups[i - 1]))
        {
            out.push_back ({ m_rowGroups[i].label, (int) i });
        }
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::SetGroupBy
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::SetGroupBy (RowGrouping::Field field, bool descending)
{
    if (!m_model.HasTabs())
    {
        return;
    }

    m_model.SetGroup (field, descending);
    ResortKeepingSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::SetSortAndGroup
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::SetSortAndGroup (CatalogModel::Column column, bool descending, RowGrouping::Field group, bool groupDescending)
{
    const BrowserModel::Tab *  tab = m_model.HasTabs() ? &m_model.GetActiveTab() : nullptr;



    if (tab == nullptr ||
        (tab->sortColumn == column && tab->sortDescending == descending && tab->groupBy == group && tab->groupDescending == groupDescending))
    {
        return;
    }

    m_model.SetSort  (column, descending);
    m_model.SetGroup (group, groupDescending);
    ResortKeepingSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::SortByColumn
//
//  The same column again reverses the order; a new column starts ascending.
//  The selection follows its rows through the reorder.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::SortByColumn (int column)
{
    CatalogModel::Column  requested       = (CatalogModel::Column) column;
    bool                  descending      = false;



    if (!m_model.HasTabs() || column < 0 || column > (int) CatalogModel::Column::DateDeleted)
    {
        return;
    }

    descending = m_model.GetActiveTab().sortColumn == requested && !m_model.GetActiveTab().sortDescending;

    m_model.SetSort (requested, descending);
    ResortKeepingSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::ResortKeepingSelection
//
//  The selection follows its rows through the reorder.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::ResortKeepingSelection()
{
    std::vector<size_t>   selectedSources;



    for (int row : m_selectedRows)
    {
        selectedSources.push_back (m_rows[row].sourceIndex);
    }

    SortRows();

    m_selectedRows.clear();

    for (size_t row = 0; row < m_rows.size(); row++)
    {
        if (std::find (selectedSources.begin(), selectedSources.end(), m_rows[row].sourceIndex) != selectedSources.end())
        {
            m_selectedRows.push_back ((int) row);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetSelectionKey
//
//  The occurrence counts rows of the same name that come before this one in
//  the listing rather than in the sorted view, so the key stays the same
//  across a re-sort and a reload.
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
        size_t  occurrence = seen[row.name]++;

        outKeys.push_back (std::to_wstring (occurrence) + L":" + row.name);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetSelectionKey
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerBrowser::GetSelectionKey (size_t row) const
{
    size_t  occurrence = 0;



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



    m_selectedRows.clear();

    for (int row : rows)
    {
        if (row >= 0 && row < (int) m_rows.size())
        {
            m_selectedRows.push_back (row);
            names.push_back (GetSelectionKey ((size_t) row));
        }
    }

    if (m_model.HasTabs())
    {
        m_model.SetSelection (names);
    }

    UpdatePreview();
    UpdateStatus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::SetDisassemble
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::SetDisassemble (bool disassemble)
{
    if (!m_model.HasTabs())
    {
        return;
    }

    m_model.GetActiveTab().disassemble = disassemble;
    UpdatePreview();
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

    if (!m_isImage || m_selectedRows.size() != 1)
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
//  CassoExplorerBrowser::UpdatePreview
//
//  One selected file in an image previews its contents; one selected disk
//  image in a host folder previews its catalog. Anything else, including a
//  multiple selection, previews nothing.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::UpdatePreview()
{
    const FileEntry         * entry       = nullptr;
    Location                  location    = GetLocation();
    DiskOperations::Result    result;
    VolumeListing             listing;
    VolumeKind                kind        = VolumeKind::Unknown;
    std::wstring              imagePath;



    m_preview = PreviewContent();
    m_preview.kind = PreviewContent::Kind::Text;

    if (TryGetSelectedEntry (entry))
    {
        if (entry->isDirectory)
        {
            m_preview.kind    = PreviewContent::Kind::Error;
            m_preview.message = L"A folder. Open it to see what it holds.";
            return;
        }

        PreviewEntry (location.path, GetEntryPath (*entry), *entry, m_kind);
        return;
    }

    //  A search's match inside a disk image previews as it does in its image.
    if (m_selectedRows.size() == 1 && m_selectedRows[0] >= 0 && (size_t) m_selectedRows[0] < m_rows.size() && !m_rows[(size_t) m_selectedRows[0]].imagePath.empty())
    {
        PreviewSearchMatch (m_rows[(size_t) m_selectedRows[0]]);
        return;
    }

    if (m_isImage || location.kind != Location::Kind::HostFolder || m_selectedRows.size() != 1)
    {
        return;
    }

    //  Any other file in a host folder shows its bytes, which the window reads
    //  from the file as it draws them rather than loading them here.
    if (!m_rows[m_selectedRows[0]].isDiskImage)
    {
        if (!m_rows[m_selectedRows[0]].isDirectory && TryPreviewAppleSingle (location.path, m_rows[m_selectedRows[0]]))
        {
            return;
        }

        if (!m_rows[m_selectedRows[0]].isDirectory)
        {
            m_preview.kind     = PreviewContent::Kind::Hex;
            m_preview.hostPath = location.path;

            if (!m_preview.hostPath.empty() && m_preview.hostPath.back() != L'\\')
            {
                m_preview.hostPath += L'\\';
            }

            m_preview.hostPath += m_rows[m_selectedRows[0]].name;
        }

        return;
    }

    imagePath = location.path;

    if (!imagePath.empty() && imagePath.back() != L'\\')
    {
        imagePath += L'\\';
    }

    imagePath += m_rows[m_selectedRows[0]].name;

    if (!m_model.TryGetCachedCatalog (imagePath, listing, kind))
    {
        result = m_operations.List (TextEncoding::WideToNarrow (imagePath), listing, kind);

        if (!result.Succeeded() && PreviewDecoder::ParseDetails (result.message, m_preview.details))
        {
            m_preview.kind = PreviewContent::Kind::Details;
            return;
        }

        if (!result.Succeeded())
        {
            m_preview.kind    = PreviewContent::Kind::Error;
            m_preview.message = FormatImageError (imagePath, result.message);
            return;
        }

        m_model.CacheCatalog (imagePath, listing, kind);
    }

    PreviewDecoder::RenderCatalog (listing, kind, m_preview);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::PreviewEntry
//
//  One file in an image, read and rendered: its contents, the details of a
//  file that cannot be read, or why not.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::PreviewEntry (const std::wstring & imagePath, const std::string & entryPath, const FileEntry & entry, VolumeKind kind)
{
    DiskOperations::Result  result;
    FilePayload             payload;
    HRESULT                 hr          = S_OK;
    bool                    disassemble = m_model.HasTabs() && m_model.GetActiveTab().disassemble;



    result = m_operations.Read (TextEncoding::WideToNarrow (imagePath), entryPath, payload, entry.catalogIndex);

    if (!result.Succeeded() && PreviewDecoder::ParseDetails (result.message, m_preview.details))
    {
        m_preview.kind = PreviewContent::Kind::Details;
        return;
    }

    if (!result.Succeeded())
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = FormatImageError (imagePath, result.message);
        return;
    }

    hr = PreviewDecoder::Render (entry, kind, payload, disassemble, m_bus, m_preview);

    if (FAILED (hr) && m_preview.message.empty())
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = L"This file could not be previewed.";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::PreviewSearchMatch
//
//  A search's match inside a disk image, found again in its image's catalog
//  by where it was listed.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::PreviewSearchMatch (const CatalogRow & row)
{
    SearchMatch   match;
    std::wstring  error;



    if (!TryFindSearchMatch (row, match, error))
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = error;
        return;
    }

    if (match.entry.isDirectory)
    {
        m_preview.kind    = PreviewContent::Kind::Error;
        m_preview.message = L"A folder. Open it to see what it holds.";
        return;
    }

    PreviewEntry (match.imagePath, match.entry.name, match.entry, match.kind);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::TryFindSearchMatch
//
//  A search's match inside a disk image, found again in its image's catalog
//  by where it was listed. False, with why, when the image cannot be read or
//  no longer holds it.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::TryFindSearchMatch (const CatalogRow & row, SearchMatch & outMatch, std::wstring & outError)
{
    DiskOperations::Result  result;
    VolumeListing           listing;
    VolumeKind              kind  = VolumeKind::Unknown;
    bool                    found = false;



    if (row.imagePath.empty())
    {
        return false;
    }

    if (!m_model.TryGetCachedCatalog (row.imagePath, listing, kind))
    {
        result = m_operations.List (TextEncoding::WideToNarrow (row.imagePath), listing, kind);

        if (!result.Succeeded())
        {
            outError = FormatImageError (row.imagePath, result.message);
            return false;
        }

        m_model.CacheCatalog (row.imagePath, listing, kind);
    }

    for (const FileEntry & entry : listing.entries)
    {
        if (!found && entry.catalogIndex == row.imageCatalogIndex)
        {
            outMatch.imagePath = row.imagePath;
            outMatch.kind      = kind;
            outMatch.entry     = entry;
            found              = true;
        }
    }

    if (!found)
    {
        outError = L"This file is no longer in its disk image.";
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetSelectedSearchMatches
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::GetSelectedSearchMatches (std::vector<SearchMatch> & outMatches)
{
    SearchMatch   match;
    std::wstring  error;



    outMatches.clear();

    for (int row : m_selectedRows)
    {
        if (row >= 0 && (size_t) row < m_rows.size() && TryFindSearchMatch (m_rows[(size_t) row], match, error))
        {
            outMatches.push_back (match);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::AreSelectedRowsSearchMatches
//
//  Whether a selection is files a search found inside disk images, which
//  copy from their images.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::AreSelectedRowsSearchMatches() const
{
    bool  any = false;



    for (int row : m_selectedRows)
    {
        any = any || (row >= 0 && (size_t) row < m_rows.size() && !m_rows[(size_t) row].imagePath.empty());
    }

    return any;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::TryPreviewAppleSingle
//
//  A host file that is an AppleSingle container previews as the file it
//  holds. Only a file small enough to be one is read to find out; anything
//  larger shows its bytes as before.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::TryPreviewAppleSingle (const std::wstring & folder, const CatalogRow & row)
{
    static constexpr uint64_t  kLargestContainer = 16 * 1024 * 1024;
    std::string                content;
    AppleSingleFile            file;
    std::string                error;
    std::span<const Byte>      bytes;
    HRESULT                    hr                = S_OK;



    if (row.sizeBytes > kLargestContainer)
    {
        return false;
    }

    hr = m_fs.ReadAllText (JoinPath (folder, row.name), content);

    if (FAILED (hr))
    {
        return false;
    }

    bytes = std::span<const Byte> ((const Byte *) content.data(), content.size());

    if (!AppleSingleCodec::IsAppleSingle (bytes))
    {
        return false;
    }

    hr = AppleSingleCodec::Decode (bytes, file, error);

    if (FAILED (hr))
    {
        return false;
    }

    PreviewDecoder::RenderAppleSingle (file, m_preview);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::UpdateStatus
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::UpdateStatus()
{
    m_status = Status();
    m_status.selection = FormatSelection (0, m_rows.size());

    if (!m_selectedRows.empty())
    {
        uint64_t  bytes   = 0;
        bool      anyFile = false;

        for (int selected : m_selectedRows)
        {
            if (!m_rows[(size_t) selected].isDirectory)
            {
                bytes  += m_rows[(size_t) selected].sizeBytes;
                anyFile = true;
            }
        }

        m_status.selected = FormatSelected (m_selectedRows.size(), bytes, anyFile);
    }

    if (m_selectedRows.size() == 1)
    {
        const CatalogRow &  row = m_rows[m_selectedRows[0]];

        m_status.detail = row.typeText;

        if (!row.isDirectory)
        {
            m_status.detail += L", " + FormatSize (row.sizeBytes);
        }

        if (!row.addressText.empty())
        {
            m_status.detail += L", " + row.addressText;
        }
    }

    //  As Explorer words a drive's: what is free of the whole. For the image
    //  open, or for one image selected in a folder.
    if (m_isImage)
    {
        DescribeVolumeSpace (GetLocation().path, m_listing, m_kind, m_status);
    }
    else if (m_selectedRows.size() == 1)
    {
        Location       image;
        VolumeListing  listing;
        VolumeKind     kind = VolumeKind::Unknown;

        if (TryGetRowLocation (m_selectedRows[0], image) && image.kind == Location::Kind::DiskImage &&
            m_operations.List (TextEncoding::WideToNarrow (image.path), std::string(), listing, kind).Succeeded())
        {
            DescribeVolumeSpace (image.path, listing, kind, m_status);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::DescribeVolumeSpace
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::DescribeVolumeSpace (const std::wstring & imagePath, const VolumeListing & listing, VolumeKind kind, Status & outStatus)
{
    uint64_t  unit = CatalogModel::GetUnitBytes (kind);



    outStatus.freeSpace    = FormatSize ((uint64_t) listing.freeUnits * unit) + L" free of " + FormatSize ((uint64_t) listing.totalUnits * unit);
    outStatus.freeSpaceTip = std::filesystem::path (imagePath).filename().wstring();

    if (kind == VolumeKind::Dos33)
    {
        outStatus.freeSpaceTip += std::format (L", DOS 3.3 volume {}", listing.volumeNumber);
    }
    else if (kind == VolumeKind::ProDos && !listing.volumeName.empty())
    {
        outStatus.freeSpaceTip += L", ProDOS volume /" + TextEncoding::NarrowToWide (listing.volumeName);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetColumns
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Column> CassoExplorerBrowser::GetColumns()
{
    std::vector<DxuiListView::Column>  columns;



    //  File Explorer's order and its default widths, measured at 100%: each a
    //  fixed width, none stretching to fill the pane.
    columns.push_back (DxuiListView::Column { L"Name",          kNameColumnDip,     false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Date modified", kModifiedColumnDip, false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Type",          kTypeColumnDip,     false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Size",          kSizeColumnDip,     false, DxuiTextHAlign::Right });
    columns.push_back (DxuiListView::Column { L"Address",       kAddressColumnDip,  false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Locked",        kLockedColumnDip,   false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Original location", kOriginalColumnDip, false, DxuiTextHAlign::Left });
    columns.push_back (DxuiListView::Column { L"Date deleted",  kDeletedColumnDip,  false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Folder",        kOriginalColumnDip, false, DxuiTextHAlign::Left  });

    return columns;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetCatalogPreviewColumns
//
//  The preview pane is narrower than the list, so a catalog there keeps only
//  what identifies a file, each column as wide as its header or its widest
//  entry.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Column> CassoExplorerBrowser::GetCatalogPreviewColumns()
{
    std::vector<DxuiListView::Column>  columns;



    columns.push_back (DxuiListView::Column { L"Name", 0, false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Type", 0, false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Size", 0, false, DxuiTextHAlign::Right });

    return columns;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::ToCatalogPreviewCells
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Cell> CassoExplorerBrowser::ToCatalogPreviewCells (const CatalogRow & row)
{
    std::vector<DxuiListView::Cell>  cells;
    DxuiListView::Cell               name;



    name.text = CatalogModel::GetDisplayName (row.name, name.dimRanges);

    cells.push_back (name);
    cells.push_back (DxuiListView::Cell { row.typeText, false });
    //  A folder has no size of its own, except in the Recycle Bin, which adds up
    //  what each deleted folder held, as Explorer shows; nor does a shell item
    //  that is not a file, such as a computer on the network.
    cells.push_back (DxuiListView::Cell { (row.isDirectory && row.recycledId.empty()) || (!row.shellId.empty() && row.hostPath.empty() && row.imagePath.empty()) ? std::wstring() : FormatSizeColumn (row.sizeBytes), false });

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::ToCells
//
//  A folder has no size to show, and an entry that records no date leaves the
//  column blank rather than showing 1970.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Cell> CassoExplorerBrowser::ToCells (const CatalogRow & row, const Location & at, IShellIcons * icons)
{
    std::vector<DxuiListView::Cell>  cells;
    DxuiListView::Cell               name;



    name.text = CatalogModel::GetDisplayName (row.name, name.dimRanges);

    cells.push_back (name);
    cells.push_back (DxuiListView::Cell { row.hasModified ? FormatModified (row.modifiedUnix, row.modifiedIsWallClock) : std::wstring(), false });
    cells.push_back (DxuiListView::Cell { row.typeText, false });
    //  A folder has no size of its own, except in the Recycle Bin, which adds up
    //  what each deleted folder held, as Explorer shows; nor does a shell item
    //  that is not a file, such as a computer on the network.
    cells.push_back (DxuiListView::Cell { (row.isDirectory && row.recycledId.empty()) || (!row.shellId.empty() && row.hostPath.empty() && row.imagePath.empty()) ? std::wstring() : FormatSizeColumn (row.sizeBytes), false });
    cells.push_back (DxuiListView::Cell { row.addressText, false });
    cells.push_back (DxuiListView::Cell { row.locked ? L"Yes" : L"", false });
    cells.push_back (DxuiListView::Cell { row.originalFolder, false });
    cells.push_back (DxuiListView::Cell { row.hasDeleted ? FormatModified (row.deletedUnix, false) : std::wstring(), false });
    cells.push_back (DxuiListView::Cell { row.folderPath, false });

    if (icons != nullptr && !cells.empty())
    {
        cells[0].icon = GetRowIcon (row, at, *icons);
    }

    cells[0].iconGhosted  = row.isHidden;

    //  Explorer's Content row: a file's type under its name, and the date and
    //  the size in the column beside them, each saying what it is. A folder
    //  has its name alone.
    if (!row.isDirectory || row.isDrive)
    {
        cells[0].contentLeft = { DxuiListView::Cell { L"Type: " + row.typeText, false } };
    }

    if (row.hasModified)
    {
        cells[0].contentRight.push_back (DxuiListView::Cell { L"Date modified: " + FormatModified (row.modifiedUnix, row.modifiedIsWallClock), false });
    }

    if (!row.isDirectory)
    {
        cells[0].contentRight.push_back (DxuiListView::Cell { L"Size: " + FormatSize (row.sizeBytes), false });
    }

    cells[0].tileNameOnly = row.isDirectory && !row.isDrive;

    //  Explorer's drive tile: a bar as full as the drive, then what is free.
    if (row.isDrive && row.sizeBytes > 0)
    {
        DxuiListView::Cell  meter;
        DxuiListView::Cell  free;

        meter.meter = 1.0f - (float) ((double) row.freeBytes / (double) row.sizeBytes);
        free.text   = std::format (L"{} free of {}", FormatSize (row.freeBytes), FormatSize (row.sizeBytes));

        cells[0].tileLines = { meter, free };
    }
    else if (!row.isDirectory)
    {
        //  Explorer's file tile: its type, then its size.
        cells[0].tileLines = { DxuiListView::Cell { row.typeText, false }, DxuiListView::Cell { FormatSize (row.sizeBytes), false } };
    }

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetNameArgb
//
//  Explorer's colors for a compressed or an encrypted item's name, when its
//  options ask for them. The dark theme's compressed blue is measured from
//  Explorer; its encrypted green, which EFS would be needed to measure, is a
//  green of the same lightness. The light theme's are Explorer's classic
//  pair.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t CassoExplorerBrowser::GetNameArgb (const CatalogRow & row, const FolderOptions & options, bool dark)
{
    if (!options.colorCompressed)
    {
        return 0;
    }

    if (row.isEncrypted)
    {
        return dark ? kEncryptedDarkArgb : kEncryptedLightArgb;
    }

    if (row.isCompressed)
    {
        return dark ? kCompressedDarkArgb : kCompressedLightArgb;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::ShortenPaths
//
//  Each full path in a message cut to its last part, the file or folder's own
//  name: the address bar and the tree already show where it is, and a whole
//  path makes a message too long to read. A path runs from its drive to the
//  colon, quote or line end that follows; a sentence's period after it is not
//  part of it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerBrowser::ShortenPaths (const std::wstring & message)
{
    std::wstring  out;
    size_t        i = 0;



    while (i < message.size())
    {
        bool  isPath = i + 2 < message.size() && iswalpha (message[i]) && message[i + 1] == L':' && message[i + 2] == L'\\'
                       && (i == 0 || !iswalnum (message[i - 1]));

        if (!isPath)
        {
            out += message[i++];
            continue;
        }

        size_t        end  = message.find_first_of (L":\r\n\"'", i + 2);
        std::wstring  path  = message.substr (i, (end == std::wstring::npos) ? std::wstring::npos : end - i);
        std::wstring  tail;
        size_t        slash = 0;

        while (!path.empty() && (path.back() == L' ' || path.back() == L'.' || path.back() == L','))
        {
            tail.insert (tail.begin(), path.back());
            path.pop_back();
        }

        //  A file's extension ends in a letter, so a period only moved to the
        //  tail when it closed the sentence -- unless the name itself ended
        //  there, which the tail cannot tell apart and leaves as it was.
        while (!path.empty() && path.back() == L'\\')
        {
            path.pop_back();
        }

        slash = path.find_last_of (L'\\');
        out  += ((slash == std::wstring::npos) ? path : path.substr (slash + 1)) + tail;
        i     = (end == std::wstring::npos) ? message.size() : end;
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::FormatImageError
//
//  The command line's refusal, "path: reason", as a sentence about the file
//  by its own name: the address bar and the tree already show where it is.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerBrowser::FormatImageError (const std::wstring & imagePath, const std::string & message)
{
    std::wstring  text   = TextEncoding::NarrowToWide (message);
    std::wstring  prefix = imagePath + L": ";
    size_t        slash  = imagePath.find_last_of (L"\\/");



    while (!text.empty() && (text.back() == L'\n' || text.back() == L'\r' || text.back() == L' '))
    {
        text.pop_back();
    }

    if (text.compare (0, prefix.size(), prefix) == 0)
    {
        text = imagePath.substr ((slash == std::wstring::npos) ? 0 : slash + 1) + L" " + text.substr (prefix.size());
    }

    //  The general part leads and the detail follows on a line of its own.
    text = TreeModel::BreakAfterFirstSentence (text);

    if (!text.empty() && text.back() != L'.')
    {
        text += L'.';
    }

    return ShortenPaths (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::FormatSize
//
//  A size as Explorer's status bar and its free space show one: Windows' own
//  byte-size format, three significant digits in the largest unit that fits.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerBrowser::FormatSize (uint64_t bytes)
{
    wchar_t  text[64] = {};



    if (StrFormatByteSizeW ((LONGLONG) bytes, text, (UINT) std::size (text)) == nullptr)
    {
        return std::format (L"{} bytes", bytes);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::FormatSizeColumn
//
//  A size as Explorer's Size column shows one: whole kilobytes rounded up,
//  with the user's thousands separator, so a 187-byte file is "1 KB".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerBrowser::FormatSizeColumn (uint64_t bytes)
{
    wchar_t  text[64] = {};



    if (StrFormatKBSizeW ((LONGLONG) bytes, text, (UINT) std::size (text)) == nullptr)
    {
        return std::format (L"{} KB", (bytes + 1023) / 1024);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::FormatModified
//
//  A host file's instant in local time, as Explorer shows it; a catalog's
//  wall-clock time as it was recorded. Either way in the user's own short
//  date and short time, as Explorer's Date modified column writes them.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerBrowser::FormatModified (int64_t unixSeconds, bool wallClock)
{
    __time64_t  seconds  = (__time64_t) unixSeconds;
    tm          local    = {};
    errno_t     err      = wallClock ? _gmtime64_s (&local, &seconds) : _localtime64_s (&local, &seconds);
    SYSTEMTIME  st       = {};
    wchar_t     date[80] = {};
    wchar_t     time[80] = {};
    int         dateLen  = 0;
    int         timeLen  = 0;



    if (err != 0)
    {
        return std::wstring();
    }

    st.wYear   = (WORD) (local.tm_year + 1900);
    st.wMonth  = (WORD) (local.tm_mon + 1);
    st.wDay    = (WORD) local.tm_mday;
    st.wHour   = (WORD) local.tm_hour;
    st.wMinute = (WORD) local.tm_min;
    st.wSecond = (WORD) local.tm_sec;

    dateLen = GetDateFormatEx (LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, date, (int) std::size (date), nullptr);
    timeLen = GetTimeFormatEx (LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &st, nullptr, time, (int) std::size (time));

    if (dateLen == 0 || timeLen == 0)
    {
        return std::format (L"{:04}-{:02}-{:02} {:02}:{:02}", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    }

    return std::wstring (date) + L" " + time;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::FormatSelected
//
//  Two spaces before the size, as Explorer's field has them.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerBrowser::FormatSelected (size_t selected, uint64_t bytes, bool anyFile)
{
    std::wstring  text = std::format (L"{} {} selected", selected, selected == 1 ? L"item" : L"items");



    if (anyFile)
    {
        text += L"  " + FormatSize (bytes);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::FormatSelection
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerBrowser::FormatSelection (size_t selected, size_t total)
{
    std::wstring  items = std::format (L"{} {}", total, total == 1 ? L"item" : L"items");



    if (selected == 0)
    {
        return items;
    }

    return std::format (L"{}, {} selected", items, selected);
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



    //  A search's match inside a disk image opens the image with the file
    //  selected, where it previews and copies as any of the image's files.
    if (row >= 0 && (size_t) row < m_rows.size() && !m_rows[(size_t) row].imagePath.empty())
    {
        std::wstring  name = m_rows[(size_t) row].name;

        m_model.NavigateTo (Location::MakeDiskImage (m_rows[(size_t) row].imagePath));
        ReloadAfterNavigation();
        SelectRowsByKeys ({ L"0:" + name });
        return true;
    }

    if (!TryGetRowLocation (row, target))
    {
        return false;
    }

    m_model.NavigateTo (target);
    ReloadAfterNavigation();

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



    if (location.kind == Location::Kind::DiskDirectory && !location.innerPath.empty())
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
    else if (!m_rootChildren.empty() && m_rows[row].sourceIndex < m_rootChildren.size())
    {
        outLocation = m_rootChildren[m_rows[row].sourceIndex].location;
    }
    else if ((location.kind == Location::Kind::DiskImage || location.kind == Location::Kind::DiskDirectory) && m_rows[row].isDirectory)
    {
        //  A directory inside an image opens below the one listed.
        std::string  inner = (location.kind == Location::Kind::DiskDirectory) ? location.innerPath : std::string();

        inner       += (inner.empty() ? "" : "/") + TextEncoding::WideToNarrow (m_rows[row].name);
        outLocation  = Location::MakeDiskDirectory (location.path, inner);
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
        else if (item.isDiskImage && CanListImage (item.hostPath))
        {
            outLocation = Location::MakeDiskImage (item.hostPath);
        }
        else
        {
            found = false;
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
    else if (m_rows[row].isDiskImage && CanListImage (JoinPath (location.path, m_rows[row].name)))
    {
        outLocation = Location::MakeDiskImage (JoinPath (location.path, m_rows[row].name));
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

    if (!m_rootChildren.empty() && m_rows[row].sourceIndex < m_rootChildren.size())
    {
        outPath = BrowserModel::FormatAddress (m_rootChildren[m_rows[row].sourceIndex].location);
    }
    else if (location.kind == Location::Kind::HostFolder)
    {
        outPath = JoinPath (location.path, m_rows[row].name);
    }
    else if (location.kind == Location::Kind::ShellFolder && !m_rows[row].hostPath.empty())
    {
        outPath = m_rows[row].hostPath;
    }
    else if (location.kind == Location::Kind::DiskImage || location.kind == Location::Kind::DiskDirectory)
    {
        outPath = JoinPath (BrowserModel::FormatAddress (location), m_rows[row].name);
    }
    else
    {
        found = false;
    }

    return found && !outPath.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::CanGoUp
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::CanGoUp() const
{
    Location  location = GetLocation();



    switch (location.kind)
    {
        case Location::Kind::HostFolder:
            return !GetParentFolder (location.path).empty();

        case Location::Kind::DiskImage:
        case Location::Kind::DiskDirectory:
            return true;

        case Location::Kind::ShellFolder:
            return m_shellParent.kind != Location::Kind::None;

        default:
            return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::TryGetUpTarget
//
//  An image's parent is the folder holding it; a directory inside an image
//  goes to the image.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::TryGetUpTarget (Location & outTarget) const
{
    Location  location = GetLocation();



    if (!CanGoUp())
    {
        return false;
    }

    if (location.kind == Location::Kind::DiskDirectory)
    {
        outTarget = Location::MakeDiskImage (location.path);
    }
    else if (location.kind == Location::Kind::ShellFolder)
    {
        outTarget = m_shellParent;
    }
    else
    {
        outTarget = Location::MakeHostFolder (GetParentFolder (location.path));
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GoUp
//
//  An image's parent is the folder holding it; a directory inside an image
//  goes to the image.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::GoUp()
{
    Location  target;



    if (!TryGetUpTarget (target))
    {
        return false;
    }

    m_model.NavigateTo (target);
    ReloadAfterNavigation();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GoBack
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::GoBack()
{
    if (!m_model.HasTabs() || !m_model.GoBack())
    {
        return false;
    }

    ReloadAfterNavigation();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GoForward
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::GoForward()
{
    if (!m_model.HasTabs() || !m_model.GoForward())
    {
        return false;
    }

    ReloadAfterNavigation();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetSelectedEntries
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::GetSelectedEntries (std::vector<FileEntry> & outEntries) const
{
    outEntries.clear();

    if (!m_isImage)
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
//  CassoExplorerBrowser::AreSelectedRowsImages
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::AreSelectedRowsImages() const
{
    if (m_isImage || m_selectedRows.empty() || GetLocation().kind != Location::Kind::HostFolder)
    {
        return false;
    }

    for (int row : m_selectedRows)
    {
        if (!m_rows[row].isDiskImage)
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
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerBrowser::Reload (bool keepSelection)
{
    HRESULT                    hr = S_OK;
    std::vector<std::wstring>  names;



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

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::TryGetNodePath
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::TryGetNodePath (const std::wstring & id, std::wstring & outPath) const
{
    auto  found = m_nodes.find (id);



    if (found == m_nodes.end() || found->second.location.kind != Location::Kind::HostFolder)
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
    auto  found = m_nodes.find (id);
    bool  has   = found != m_nodes.end() && found->second.location.kind != Location::Kind::None;



    if (has)
    {
        outLocation = found->second.location;
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
    auto  found = m_nodes.find (id);



    return found != m_nodes.end()
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
    auto  found = m_nodes.find (id);



    return found != m_nodes.end() && found->second.kind == TreeNode::Kind::KnownFolder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetLocationLabel
//
//  A folder's own name, a drive's root as written, an image's file name, and
//  the innermost directory of a path inside an image.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerBrowser::GetLocationLabel (const Location & location)
{
    std::wstring  path  = location.path;
    size_t        slash = 0;
    std::string   inner = location.innerPath;



    switch (location.kind)
    {
        case Location::Kind::None:
            return L"Home";

        case Location::Kind::Root:
            return TreeModel::GetRootLabel (location.path);

        case Location::Kind::RecycleBin:
            return TreeModel::GetRootLabel (Location::kRecycleBinId);

        case Location::Kind::ShellFolder:
            return location.label.empty() ? location.path : location.label;

        case Location::Kind::DiskDirectory:
            slash = inner.find_last_of ("/:");
            return TextEncoding::NarrowToWide ((slash == std::string::npos) ? inner : inner.substr (slash + 1));

        default:
            break;
    }

    while (path.size() > 3 && path.back() == L'\\')
    {
        path.pop_back();
    }

    if (path.size() <= 3)
    {
        return path;
    }

    slash = path.rfind (L'\\');

    return (slash == std::wstring::npos) ? path : path.substr (slash + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetTabLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerBrowser::GetTabLabel (size_t index) const
{
    if (index >= m_model.GetTabCount())
    {
        return std::wstring();
    }

    return GetLocationLabel (m_model.GetTab (index).location);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::NewTab
//
////////////////////////////////////////////////////////////////////////////////

size_t CassoExplorerBrowser::NewTab()
{
    //  Home, the way a browser and Explorer open one, rather than a second
    //  copy of the folder the tab it was opened from is showing.
    size_t  index = m_model.OpenTab (Location());



    ReloadAfterNavigation();

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::SwitchTab
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::SwitchTab (size_t index)
{
    std::vector<std::wstring>  names;



    if (!m_model.SwitchTo (index))
    {
        return false;
    }

    names = m_model.GetActiveTab().selection;
    ReloadAfterNavigation();
    SelectRowsByKeys (names);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::CloseTab
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::CloseTab (size_t index)
{
    if (m_model.GetTabCount() <= 1 || !m_model.CloseTab (index))
    {
        return false;
    }

    return SwitchTab (m_model.GetActiveIndex());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::OpenInNewTab
//
////////////////////////////////////////////////////////////////////////////////

size_t CassoExplorerBrowser::OpenInNewTab (const Location & location)
{
    size_t  index = m_model.OpenTab (location);



    ReloadAfterNavigation();

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::DuplicateTab
//
//  A new tab at the same location; its history starts fresh.
//
////////////////////////////////////////////////////////////////////////////////

size_t CassoExplorerBrowser::DuplicateTab (size_t index)
{
    if (index >= m_model.GetTabCount())
    {
        return m_model.GetActiveIndex();
    }

    return OpenInNewTab (m_model.GetTab (index).location);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::CloseOtherTabs
//
//  Closed from the end, so the indices still to close do not move.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::CloseOtherTabs (size_t index)
{
    size_t  i      = m_model.GetTabCount();
    bool    closed = false;



    if (index >= m_model.GetTabCount())
    {
        return false;
    }

    while (i-- > 0)
    {
        if (i != index && m_model.CloseTab (i))
        {
            closed = true;
        }
    }

    if (closed)
    {
        SwitchTab (0);
    }

    return closed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::CloseTabsToRight
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::CloseTabsToRight (size_t index)
{
    size_t  i      = m_model.GetTabCount();
    bool    closed = false;



    while (i-- > index + 1)
    {
        if (m_model.CloseTab (i))
        {
            closed = true;
        }
    }

    if (closed)
    {
        SwitchTab (m_model.GetActiveIndex());
    }

    return closed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::RestoreTabs
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::RestoreTabs (const std::vector<Location> & locations)
{
    bool  switched = false;



    //  Nothing saved -- a first run -- opens one tab at Home, as Explorer
    //  opens one, rather than leaving the strip empty.
    if (locations.empty())
    {
        if (m_model.GetTabCount() == 0)
        {
            m_model.OpenTab (Location());
            switched = SwitchTab (0);
            IGNORE_RETURN_VALUE (switched, true);
        }

        return;
    }

    for (bool closed = true; closed && m_model.GetTabCount() > 0; )
    {
        closed = m_model.CloseTab (0);
    }

    for (const Location & location : locations)
    {
        m_model.OpenTab (location);
    }

    switched = SwitchTab (0);
    IGNORE_RETURN_VALUE (switched, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetRowIcon
//
//  A host folder's rows are real files and folders, so each gets the shell's
//  icon for its path, except a disk image, which has Casso's floppy. A row inside an image has no host path, so it gets the generic
//  folder or file icon.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> CassoExplorerBrowser::GetRowIcon (const CatalogRow & row, const Location & at, IShellIcons & icons)
{
    //  A disk image has Casso's floppy wherever it is listed.
    if (row.isDiskImage)
    {
        return icons.GetForKind (IShellIcons::Kind::DiskImage);
    }

    if (at.kind == Location::Kind::HostFolder)
    {
        return icons.GetForPath (JoinPath (at.path, row.name), row.isDirectory);
    }

    //  A root's rows are its known folders and drives, with their own icons.
    if (at.kind == Location::Kind::Root && !row.hostPath.empty())
    {
        return icons.GetForPath (row.hostPath, true);
    }

    //  A search's match inside a disk image has its Apple type's icon.
    if (!row.imagePath.empty())
    {
        return icons.GetForKind (GetAppleTypeIconKind (row.typeText));
    }

    //  A shell folder's item has the icon the shell draws for it, by its path
    //  when it has one.
    if (at.kind == Location::Kind::ShellFolder && !row.shellId.empty())
    {
        return icons.GetForPath (row.hostPath.empty() ? row.shellId : row.hostPath, row.isDirectory);
    }

    //  A deleted item still has its file, under the bin's own folder on its
    //  drive, so it has the icon it had.
    if (at.kind == Location::Kind::RecycleBin && row.recycledId.size() > 2 && row.recycledId[1] == L':')
    {
        return icons.GetForPath (row.recycledId, row.isDirectory);
    }

    return icons.GetForKind (row.isDirectory ? IShellIcons::Kind::Folder : GetAppleTypeIconKind (row.typeText));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetLocationIcon
//
//  The icon the address bar leads with, as Explorer's shows the folder's own:
//  a host folder's or a shell folder's from the shell, a disk image's by its
//  extension, and a directory inside an image the generic folder.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> CassoExplorerBrowser::GetLocationIcon (const Location & at, IShellIcons & icons)
{
    switch (at.kind)
    {
        case Location::Kind::HostFolder:    return icons.GetForPath (at.path, true);
        case Location::Kind::ShellFolder:   return icons.GetForPath (at.path, true);
        case Location::Kind::DiskImage:     return icons.GetForKind (IShellIcons::Kind::DiskImage);
        case Location::Kind::DiskDirectory: return icons.GetForKind (IShellIcons::Kind::Folder);
        case Location::Kind::RecycleBin:    return icons.GetForKind (IShellIcons::Kind::RecycleBin);
        case Location::Kind::Root:
            return icons.GetForKind ((at.path == TreeModel::kCassoRootId) ? IShellIcons::Kind::Casso : IShellIcons::Kind::ThisPc);
        default:                            return nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetAppleTypeIconKind
//
//  By the type a catalog shows: DOS 3.3's letter or ProDOS's mnemonic. Any
//  other type has the generic file icon.
//
////////////////////////////////////////////////////////////////////////////////

IShellIcons::Kind CassoExplorerBrowser::GetAppleTypeIconKind (const std::wstring & typeText)
{
    static constexpr std::pair<const wchar_t *, IShellIcons::Kind>  kTypes[] =
    {
        { L"T",   IShellIcons::Kind::AppleText        },
        { L"TXT", IShellIcons::Kind::AppleText        },
        { L"A",   IShellIcons::Kind::AppleApplesoft   },
        { L"BAS", IShellIcons::Kind::AppleApplesoft   },
        { L"I",   IShellIcons::Kind::AppleInteger     },
        { L"INT", IShellIcons::Kind::AppleInteger     },
        { L"B",   IShellIcons::Kind::AppleBinary      },
        { L"BIN", IShellIcons::Kind::AppleBinary      },
        { L"SYS", IShellIcons::Kind::AppleSystem      },
        { L"S",   IShellIcons::Kind::AppleSystem      },
        { L"R",   IShellIcons::Kind::AppleRelocatable },
        { L"REL", IShellIcons::Kind::AppleRelocatable },
    };



    for (const auto & type : kTypes)
    {
        if (typeText == type.first)
        {
            return type.second;
        }
    }

    return IShellIcons::Kind::File;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetNodeIcon
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> CassoExplorerBrowser::GetNodeIcon (const TreeNode & node, IShellIcons & icons)
{
    //  A root the desktop holds as an entry of its own, as OneDrive, has that
    //  entry's icon, its cloud, rather than its folder's.
    if (!node.iconId.empty())
    {
        return icons.GetForPath (node.iconId, true);
    }

    switch (node.kind)
    {
        case TreeNode::Kind::CassoRoot:     return icons.GetForKind (IShellIcons::Kind::Casso);
        case TreeNode::Kind::ThisPcRoot:    return icons.GetForKind (IShellIcons::Kind::ThisPc);
        case TreeNode::Kind::RecycleBinRoot: return icons.GetForKind (IShellIcons::Kind::RecycleBin);
        case TreeNode::Kind::DiskDirectory: return icons.GetForKind (IShellIcons::Kind::Folder);
        //  Every remaining node is a drive or a host folder; a disk image node
        //  has Casso's floppy.
        case TreeNode::Kind::DiskImage:     return icons.GetForKind (IShellIcons::Kind::DiskImage);
        default:                            return icons.GetForPath (node.location.path, true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::NavigateToLocation
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::NavigateToLocation (const Location & location)
{
    if (!m_model.HasTabs())
    {
        m_model.OpenTab (location);
    }
    else if (m_model.GetActiveTab().location != location)
    {
        m_model.NavigateTo (location);
    }

    ReloadAfterNavigation();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::NavigateToAddress
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::NavigateToAddress (const std::wstring & text)
{
    Location  location;
    bool      parsed = BrowserModel::ParseAddress (m_fs, RootOnSystemDrive (text), location);



    //  A shell folder typed by the shell's name for it shows the name it has.
    if (parsed && location.kind == Location::Kind::ShellFolder && location.label.empty() && m_shellVerbs != nullptr)
    {
        HRESULT  hr = m_shellVerbs->GetShellItemName (location.path, location.label);

        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (parsed)
    {
        NavigateToLocation (location);

        //  After the navigation, so that a path going nowhere is never offered
        //  back.
        m_typedPaths.Add (RootOnSystemDrive (text));
    }

    return parsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::RootOnSystemDrive
//
//  A path that starts at a root with no drive -- "\", "\Users" -- is on the
//  system drive, the one Windows booted from, as Explorer reads it. A UNC path
//  and anything else are kept.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerBrowser::RootOnSystemDrive (const std::wstring & text)
{
    wchar_t  windows[MAX_PATH] = {};
    UINT     length            = 0;



    if (text.empty() || text[0] != L'\\' || (text.size() > 1 && text[1] == L'\\'))
    {
        return text;
    }

    length = GetWindowsDirectoryW (windows, MAX_PATH);

    return (length >= 2) ? std::wstring (windows, 2) + text : L"C:" + text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GoBackBy
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::GoBackBy (size_t steps)
{
    size_t  moved = 0;



    while (moved < steps && m_model.HasTabs() && m_model.GoBack())
    {
        moved++;
    }

    if (moved > 0)
    {
        ReloadAfterNavigation();
    }

    return moved > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GoForwardBy
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::GoForwardBy (size_t steps)
{
    size_t  moved = 0;



    while (moved < steps && m_model.HasTabs() && m_model.GoForward())
    {
        moved++;
    }

    if (moved > 0)
    {
        ReloadAfterNavigation();
    }

    return moved > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::CanListImage
//
//  Whether an image has a catalog to open, read once and cached; an image
//  with no file system has nothing to open into.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::CanListImage (const std::wstring & imagePath)
{
    VolumeListing           listing;
    VolumeKind              kind   = VolumeKind::Unknown;
    DiskOperations::Result  result;
    bool                    listed = m_model.TryGetCachedCatalog (imagePath, listing, kind);



    if (!listed)
    {
        result = m_operations.List (TextEncoding::WideToNarrow (imagePath), listing, kind);
        listed = result.Succeeded();

        if (listed)
        {
            m_model.CacheCatalog (imagePath, listing, kind);
        }
    }

    return listed;
}
