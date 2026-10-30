#include "Pch.h"

#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/SearchQuery.h"
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
    m_listingKept  = false;
    m_loader.m_writeProtected = false;
    m_loader.m_listError.clear();
    m_selectedRows.clear();
    m_loader.m_shellParent = Location();

    m_loader.RevisitLink (location);

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

    m_rowsUnfiltered = true;
    m_homeRows       = IsHomeLocation() ? m_rows : std::vector<CatalogRow>();
    SortRows();
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
    m_tree.SetShellVerbs (verbs);

    m_tree.SetShellFetch ([this] (const std::wstring & id, std::vector<IShellItemVerbs::ShellFolderItem> & outItems)
    {
        HRESULT  hr = S_OK;

        return m_shellListings.TryTake (s_kTreeKey + id, id, outItems, hr);
    });
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

    //  Home keeps the shell's order: Quick access, then the tab's items.
    if (IsHomeLocation())
    {
        static constexpr int  s_kTabGroups[] = { 3, 2, 4 };

        m_rows.clear();
        m_rowGroups.clear();

        for (const CatalogRow & row : m_homeRows)
        {
            if (row.homeGroup == 1)
            {
                m_rows.push_back (row);
            }
        }

        //  The pinned folders keep their order and the frequent ones follow by
        //  name; Shared is by name; the others are as the shell has them.
        std::stable_sort (m_rows.begin(), m_rows.end(), [] (const CatalogRow & a, const CatalogRow & b)
        {
            return (a.homePinned != b.homePinned) ? a.homePinned : (!a.homePinned && _wcsicmp (a.name.c_str(), b.name.c_str()) < 0);
        });

        m_homeFirstTabRow = (int) m_rows.size();

        for (const CatalogRow & row : m_homeRows)
        {
            if (row.homeGroup == s_kTabGroups[m_homeTab])
            {
                m_rows.push_back (row);
            }
        }

        if (s_kTabGroups[m_homeTab] == 4)
        {
            std::stable_sort (m_rows.begin() + m_homeFirstTabRow, m_rows.end(), [] (const CatalogRow & a, const CatalogRow & b)
            {
                return _wcsicmp (a.name.c_str(), b.name.c_str()) < 0;
            });
        }

        return;
    }

    FilterRows();

    if ((size_t) m_model.GetActiveTab().sortColumn >= CatalogModel::kColumnCount && m_propertyKey)
    {
        std::vector<CatalogModel::PropertyKey>  keys;

        for (size_t row = 0; row < m_rows.size(); row++)
        {
            keys.push_back (m_propertyKey (row, (size_t) m_model.GetActiveTab().sortColumn - CatalogModel::kColumnCount));
        }

        CatalogModel::SortByProperty (m_rows, keys, m_model.GetActiveTab().sortDescending);
    }
    else
    {
        CatalogModel::Sort (m_rows, m_model.GetActiveTab().sortColumn, m_model.GetActiveTab().sortDescending);
    }

    GroupRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetFilterField
//
//  The columns File Explorer filters by its groups: name, date, type and
//  size.
//
////////////////////////////////////////////////////////////////////////////////

RowGrouping::Field CassoExplorerBrowser::GetFilterField (size_t column)
{
    switch ((CatalogModel::Column) column)
    {
        case CatalogModel::Column::Name:     return RowGrouping::Field::Name;
        case CatalogModel::Column::Modified: return RowGrouping::Field::DateModified;
        case CatalogModel::Column::Type:     return RowGrouping::Field::Type;
        case CatalogModel::Column::Size:     return RowGrouping::Field::Size;
        default:                             return RowGrouping::Field::None;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetFilterChoices
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<RowGrouping::Group> & CassoExplorerBrowser::GetFilterChoices (RowGrouping::Field field) const
{
    static const std::vector<RowGrouping::Group>  s_kNone;
    auto                                          found  = m_filterChoices.find (field);



    return (found != m_filterChoices.end()) ? found->second : s_kNone;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::IsFilterChecked
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerBrowser::IsFilterChecked (RowGrouping::Field field, const std::wstring & label) const
{
    auto  found = m_filters.find (field);



    return found != m_filters.end() && found->second.contains (label);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::ToggleFilter
//
//  The last group unchecked ends the column's filter.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::ToggleFilter (RowGrouping::Field field, const std::wstring & label)
{
    std::set<std::wstring> &  checked = m_filters[field];



    if (!checked.erase (label))
    {
        checked.insert (label);
    }

    if (checked.empty())
    {
        m_filters.erase (field);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::ClearFilter
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::ClearFilter (RowGrouping::Field field)
{
    m_filters.erase (field);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::FilterRows
//
//  The choices come from every row the folder has; the rows kept are those
//  in a checked group of every filtered column. Rows are filtered once each
//  load, so a sort afterward sorts the rows the filter left.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::FilterRows()
{
    RowGrouping::Today  today = RowGrouping::GetToday();



    if (!m_rowsUnfiltered)
    {
        return;
    }

    m_rowsUnfiltered = false;

    if (!(GetLocation() == m_filterLocation))
    {
        m_filters.clear();
        m_filterLocation = GetLocation();
    }

    m_filterChoices.clear();

    for (RowGrouping::Field field : { RowGrouping::Field::Name, RowGrouping::Field::DateModified, RowGrouping::Field::Type, RowGrouping::Field::Size })
    {
        std::vector<RowGrouping::Group> &  choices = m_filterChoices[field];

        for (const CatalogRow & row : m_rows)
        {
            RowGrouping::Group  group = RowGrouping::GetGroup (row, field, today);

            if (std::find (choices.begin(), choices.end(), group) == choices.end())
            {
                choices.push_back (group);
            }
        }

        //  File Explorer's filter lists a group of rows with no value, such
        //  as a folder's Unspecified size, after the others.
        std::stable_sort (choices.begin(), choices.end(), [] (const RowGrouping::Group & a, const RowGrouping::Group & b)
        {
            if ((a.rank < 0) != (b.rank < 0))
            {
                return b.rank < 0;
            }

            return RowGrouping::IsBefore (a, b, false);
        });
    }

    if (m_filters.empty())
    {
        return;
    }

    std::erase_if (m_rows, [this, &today] (const CatalogRow & row)
    {
        for (const auto & [field, checked] : m_filters)
        {
            if (!checked.contains (RowGrouping::GetGroup (row, field, today).label))
            {
                return true;
            }
        }

        return false;
    });
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
    m_homeTab = tab;
    m_selectedRows.clear();

    SortRows();
    m_previewer.UpdatePreview();
    m_cells.UpdateStatus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetListGroups
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Group> CassoExplorerBrowser::GetListGroups() const
{
    std::vector<DxuiListView::Group>  out;



    if (IsHomeLocation())
    {
        out.push_back ({ L"Quick access", 0 });
        out.push_back ({ L"Home tabs", m_homeFirstTabRow });

        return out;
    }

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



    if (!m_model.HasTabs() || column < 0 || (column > (int) CatalogModel::Column::DateDeleted && !IsPropertyColumn (column)))
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
        else if (item.isDiskImage && m_loader.CanListImage (item.hostPath))
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
        case Location::Kind::AppleDrive:
            return true;

        case Location::Kind::ShellFolder:
            return SearchQuery::IsId (location.path) || m_loader.m_shellParent.kind != Location::Kind::None;

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
    else if (location.kind == Location::Kind::AppleDrive)
    {
        std::wstring  link;
        int           slot  = 0;
        int           drive = 0;
        size_t        slash = location.innerPath.find_last_of ('/');

        TreeModel::TryParseLinkId (location.path, link, slot, drive);

        //  A directory goes up to its parent, and a drive to its link.
        outTarget = location.innerPath.empty()       ? Location::MakeRoot (TreeModel::MakeLinkId (link))
                  : slash == std::string::npos      ? Location::MakeAppleDrive (location.path, std::string())
                                                    : Location::MakeAppleDrive (location.path, location.innerPath.substr (0, slash));
    }
    else if (location.kind == Location::Kind::ShellFolder && SearchQuery::IsId (location.path))
    {
        std::wstring               scope;
        std::wstring               query;
        std::vector<std::wstring>  folders;

        //  A search's results go up to the folder searched, or to Casso's
        //  root for one that searched all of Casso's folders.
        SearchQuery::TryParseId (location.path, scope, query);
        folders   = SearchQuery::SplitScopes (scope);
        outTarget = (folders.size() == 1) ? Location::MakeHostFolder (folders[0]) : Location::MakeRoot (TreeModel::kCassoRootId);
    }
    else if (location.kind == Location::Kind::ShellFolder)
    {
        outTarget = m_loader.m_shellParent;
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
    m_loader.ReloadAfterNavigation();

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

    m_loader.ReloadAfterNavigation();

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

    m_loader.ReloadAfterNavigation();

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
//  CassoExplorerBrowser::NewTab
//
////////////////////////////////////////////////////////////////////////////////

size_t CassoExplorerBrowser::NewTab()
{
    //  Home, the way a browser and Explorer open one, rather than a second
    //  copy of the folder the tab it was opened from is showing.
    size_t  index = m_model.OpenTab (Location());



    m_loader.ReloadAfterNavigation();

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

    //  Returning to a tab lists what was last read there; Refresh, or going to
    //  the drive again, reads its disks.
    names                 = m_model.GetActiveTab().selection;
    m_loader.m_linkVisit  = GetLocation();
    m_loader.ReloadAfterNavigation();
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
//  CassoExplorerBrowser::InsertTab
//
//  The tab's cached catalog moves with it, so its disk is not read again.
//
////////////////////////////////////////////////////////////////////////////////

size_t CassoExplorerBrowser::InsertTab (size_t index, BrowserModel::Tab tab)
{
    index = m_model.InsertTab (index, std::move (tab));
    SwitchTab (index);

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::OpenInNewTab
//
////////////////////////////////////////////////////////////////////////////////

size_t CassoExplorerBrowser::OpenInNewTab (const Location & location)
{
    size_t  index = m_model.OpenTab (location);



    m_loader.ReloadAfterNavigation();

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
        if (!m_loader.IsOnMissingDrive (location))
        {
            m_model.OpenTab (location);
        }
    }

    if (m_model.GetTabCount() == 0)
    {
        m_model.OpenTab (Location());
    }

    switched = SwitchTab (0);
    IGNORE_RETURN_VALUE (switched, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::RestoreTabs
//
//  Whole tabs, history and view included, with `active` active. A tab left
//  closed for a missing drive moves the active index along with the rest; if
//  the active tab itself is left closed, the first tab is active.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerBrowser::RestoreTabs (std::vector<BrowserModel::Tab> tabs, size_t active)
{
    size_t  kept     = 0;
    size_t  showing  = 0;
    size_t  i        = 0;
    bool    switched = false;



    if (tabs.empty())
    {
        RestoreTabs (std::vector<Location>());
        return;
    }

    for (bool closed = true; closed && m_model.GetTabCount() > 0; )
    {
        closed = m_model.CloseTab (0);
    }

    for (i = 0; i < tabs.size(); i++)
    {
        if (m_loader.IsOnMissingDrive (tabs[i].location))
        {
            continue;
        }

        if (i == active)
        {
            showing = kept;
        }

        m_model.InsertTab (kept++, std::move (tabs[i]));
    }

    if (m_model.GetTabCount() == 0)
    {
        m_model.OpenTab (Location());
    }

    switched = SwitchTab (showing);
    IGNORE_RETURN_VALUE (switched, true);
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

    m_loader.ReloadAfterNavigation();
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
        m_loader.ReloadAfterNavigation();
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
        m_loader.ReloadAfterNavigation();
    }

    return moved > 0;
}
