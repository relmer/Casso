#include "Pch.h"

#include "Cassque/CassqueBrowser.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::CassqueBrowser
//
////////////////////////////////////////////////////////////////////////////////

CassqueBrowser::CassqueBrowser (IFileSystem & fs, IDiskFileIo & fileIo)
    : m_fs         (fs),
      m_tree       (fs),
      m_operations (fileIo)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::ToTreeNode
//
//  Collapsed, with children left to the provider, so nothing below a root is
//  read until someone expands it.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTreeNode CassqueBrowser::ToTreeNode (const TreeNode & node, IShellIcons * icons)
{
    DxuiTreeNode  out;



    out.id             = node.id;
    out.label          = node.label;

    //  A known folder's label is its whole path, which a narrow tree cannot
    //  hold; the folder's own name is what Explorer shows there too.
    if (node.kind == TreeNode::Kind::KnownFolder)
    {
        std::wstring  trimmed = node.label;
        size_t        slash   = std::wstring::npos;

        while (trimmed.size() > 3 && trimmed.back() == L'\\')
        {
            trimmed.pop_back();
        }

        slash = trimmed.rfind (L'\\');

        if (slash != std::wstring::npos && slash + 1 < trimmed.size())
        {
            out.label = trimmed.substr (slash + 1);
        }
    }

    out.expanded       = false;
    out.childrenLoaded = !node.canExpand;
    out.dimmed         = node.missing || !node.loadError.empty();

    if (icons != nullptr)
    {
        out.icon = GetNodeIcon (node, *icons);
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::GetTreeRoots
//
////////////////////////////////////////////////////////////////////////////////

void CassqueBrowser::GetTreeRoots (std::vector<DxuiTreeNode> & outNodes)
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
//  CassqueBrowser::GetTreeChildren
//
//  A node whose children will not load has none; the reason is on the node
//  itself, and shows in the list when the node is selected.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiTreeNode> CassqueBrowser::GetTreeChildren (const std::wstring & id)
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
//  CassqueBrowser::GetLocation
//
////////////////////////////////////////////////////////////////////////////////

Location CassqueBrowser::GetLocation() const
{
    return m_model.HasTabs() ? m_model.GetActiveTab().location : Location();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::SelectTreeNode
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassqueBrowser::SelectTreeNode (const std::wstring & id)
{
    auto      found    = m_nodes.find (id);
    Location  location;



    if (found != m_nodes.end())
    {
        location = found->second.location;
    }

    //  A root has no location of its own; it lists what it holds.
    m_rootId = (location.kind == Location::Kind::None && found != m_nodes.end()) ? id : std::wstring();

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
//  CassqueBrowser::Refresh
//
//  A directory inside an image shows the image's catalog: the volume reader
//  enumerates the volume as a whole.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassqueBrowser::Refresh()
{
    HRESULT   hr       = S_OK;
    Location  location = GetLocation();



    m_rows.clear();
    m_hostEntries.clear();
    m_rootChildren.clear();
    m_listing      = VolumeListing();
    m_kind         = VolumeKind::Unknown;
    m_isImage      = false;
    m_listError.clear();
    m_selectedRows.clear();

    switch (location.kind)
    {
        case Location::Kind::HostFolder:
            hr = LoadHostFolder (location.path);
            break;

        case Location::Kind::DiskImage:
        case Location::Kind::DiskDirectory:
            hr = LoadImage (location.path);
            break;

        default:
            if (!m_rootId.empty())
            {
                hr = LoadRoot (m_rootId);
            }

            break;
    }

    SortRows();
    UpdatePreview();
    UpdateStatus();

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::ReloadAfterNavigation
//
////////////////////////////////////////////////////////////////////////////////

void CassqueBrowser::ReloadAfterNavigation()
{
    HRESULT  hr = Refresh();



    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::LoadHostFolder
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassqueBrowser::LoadHostFolder (const std::wstring & path)
{
    HRESULT  hr    = S_OK;
    size_t   index = 0;



    hr = m_fs.EnumerateEntries (path, m_hostEntries);

    if (FAILED (hr))
    {
        m_listError = L"This folder could not be read.";
        return hr;
    }

    for (index = 0; index < m_hostEntries.size(); index++)
    {
        const FileSystemEntry &  entry   = m_hostEntries[index];
        bool                     isImage = !entry.isFolder && TreeModel::IsSupportedImage (entry.name);

        m_rows.push_back (CatalogModel::FromHostEntry (entry, isImage, index));
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::LoadRoot
//
//  The Casso root's known folders and This PC's drives, as folder rows.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassqueBrowser::LoadRoot (const std::wstring & id)
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
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::LoadImage
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassqueBrowser::LoadImage (const std::wstring & path)
{
    DiskOperations::Result  result;
    bool                    cached = m_model.TryGetCachedCatalog (path, m_listing, m_kind);



    if (!cached)
    {
        result = m_operations.List (TextEncoding::WideToNarrow (path), m_listing, m_kind);

        if (!result.Succeeded())
        {
            m_listError = TextEncoding::NarrowToWide (result.message);
            return result.hr;
        }

        m_model.CacheCatalog (path, m_listing, m_kind);
    }

    m_isImage = true;
    CatalogModel::FromListing (m_listing, m_kind, m_rows);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::SortRows
//
////////////////////////////////////////////////////////////////////////////////

void CassqueBrowser::SortRows()
{
    if (!m_model.HasTabs())
    {
        return;
    }

    CatalogModel::Sort (m_rows, m_model.GetActiveTab().sortColumn, m_model.GetActiveTab().sortDescending);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::SortByColumn
//
//  The same column again reverses the order; a new column starts ascending.
//  The selection follows its rows through the reorder.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueBrowser::SortByColumn (int column)
{
    CatalogModel::Column  requested       = (CatalogModel::Column) column;
    std::vector<size_t>   selectedSources;
    bool                  descending      = false;



    if (!m_model.HasTabs() || column < 0 || column > (int) CatalogModel::Column::Modified)
    {
        return;
    }

    descending = m_model.GetActiveTab().sortColumn == requested && !m_model.GetActiveTab().sortDescending;

    for (int row : m_selectedRows)
    {
        selectedSources.push_back (m_rows[row].sourceIndex);
    }

    m_model.SetSort (requested, descending);
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
//  CassqueBrowser::SetSelectedRows
//
////////////////////////////////////////////////////////////////////////////////

void CassqueBrowser::SetSelectedRows (const std::vector<int> & rows)
{
    std::vector<std::wstring>  names;



    m_selectedRows.clear();

    for (int row : rows)
    {
        if (row >= 0 && row < (int) m_rows.size())
        {
            m_selectedRows.push_back (row);
            names.push_back (m_rows[row].name);
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
//  CassqueBrowser::SetDisassemble
//
////////////////////////////////////////////////////////////////////////////////

void CassqueBrowser::SetDisassemble (bool disassemble)
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
//  CassqueBrowser::TryGetSelectedEntry
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueBrowser::TryGetSelectedEntry (const FileEntry *& outEntry) const
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
//  CassqueBrowser::UpdatePreview
//
//  One selected file in an image previews its contents; one selected disk
//  image in a host folder previews its catalog. Anything else, including a
//  multiple selection, previews nothing.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueBrowser::UpdatePreview()
{
    const FileEntry         * entry       = nullptr;
    Location                  location    = GetLocation();
    DiskOperations::Result    result;
    FilePayload               payload;
    VolumeListing             listing;
    VolumeKind                kind        = VolumeKind::Unknown;
    HRESULT                   hr          = S_OK;
    bool                      disassemble = m_model.HasTabs() && m_model.GetActiveTab().disassemble;
    std::wstring              imagePath;



    m_preview = PreviewContent();
    m_preview.kind = PreviewContent::Kind::Text;

    if (TryGetSelectedEntry (entry))
    {
        if (entry->isDirectory)
        {
            return;
        }

        result = m_operations.Read (TextEncoding::WideToNarrow (location.path), entry->name, payload);

        if (!result.Succeeded())
        {
            m_preview.kind    = PreviewContent::Kind::Error;
            m_preview.message = TextEncoding::NarrowToWide (result.message);
            return;
        }

        hr = PreviewDecoder::Render (*entry, m_kind, payload, disassemble, m_bus, m_preview);

        if (FAILED (hr) && m_preview.message.empty())
        {
            m_preview.kind    = PreviewContent::Kind::Error;
            m_preview.message = L"This file could not be previewed.";
        }

        return;
    }

    if (m_isImage || location.kind != Location::Kind::HostFolder || m_selectedRows.size() != 1)
    {
        return;
    }

    if (!m_rows[m_selectedRows[0]].isDiskImage)
    {
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

        if (!result.Succeeded())
        {
            m_preview.kind    = PreviewContent::Kind::Error;
            m_preview.message = TextEncoding::NarrowToWide (result.message);
            return;
        }

        m_model.CacheCatalog (imagePath, listing, kind);
    }

    PreviewDecoder::RenderCatalog (listing, kind, m_preview);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::UpdateStatus
//
////////////////////////////////////////////////////////////////////////////////

void CassqueBrowser::UpdateStatus()
{
    m_status = Status();
    m_status.selection = FormatSelection (m_selectedRows.size(), m_rows.size());

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

    if (m_isImage)
    {
        m_status.freeSpace = FormatSize ((uint64_t) m_listing.freeUnits * CatalogModel::GetUnitBytes (m_kind)) + L" free";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::GetColumns
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Column> CassqueBrowser::GetColumns()
{
    std::vector<DxuiListView::Column>  columns;



    columns.push_back (DxuiListView::Column { L"Name",     200, true,  DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Type",     70,  false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Size",     92,  false, DxuiTextHAlign::Right });
    columns.push_back (DxuiListView::Column { L"Address",  76,  false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Locked",   74,  false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Modified", 140, false, DxuiTextHAlign::Left  });

    return columns;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::GetCatalogPreviewColumns
//
//  The preview pane is narrower than the list, so a catalog there keeps only
//  what identifies a file.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Column> CassqueBrowser::GetCatalogPreviewColumns()
{
    std::vector<DxuiListView::Column>  columns;



    columns.push_back (DxuiListView::Column { L"Name", 0,  true,  DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Type", 60, false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Size", 92, false, DxuiTextHAlign::Right });

    return columns;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::ToCatalogPreviewCells
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Cell> CassqueBrowser::ToCatalogPreviewCells (const CatalogRow & row)
{
    std::vector<DxuiListView::Cell>  cells;



    cells.push_back (DxuiListView::Cell { row.name,     false });
    cells.push_back (DxuiListView::Cell { row.typeText, false });
    cells.push_back (DxuiListView::Cell { row.isDirectory ? std::wstring() : FormatSize (row.sizeBytes), false });

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::ToCells
//
//  A folder has no size to show, and an entry that records no date leaves the
//  column blank rather than showing 1970.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Cell> CassqueBrowser::ToCells (const CatalogRow & row, const Location & at, IShellIcons * icons)
{
    std::vector<DxuiListView::Cell>  cells;



    cells.push_back (DxuiListView::Cell { row.name,     false });
    cells.push_back (DxuiListView::Cell { row.typeText, false });
    cells.push_back (DxuiListView::Cell { row.isDirectory ? std::wstring() : FormatSize (row.sizeBytes), false });
    cells.push_back (DxuiListView::Cell { row.addressText, false });
    cells.push_back (DxuiListView::Cell { row.locked ? L"Yes" : L"", false });
    cells.push_back (DxuiListView::Cell { row.hasModified ? FormatModified (row.modifiedUnix, row.modifiedIsWallClock) : std::wstring(), false });

    if (icons != nullptr && !cells.empty())
    {
        cells[0].icon = GetRowIcon (row, at, *icons);
    }

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::FormatSize
//
//  Bytes up to a kilobyte, then kilobytes with one decimal: an Apple file is
//  small enough that the byte count is what someone wants to see.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassqueBrowser::FormatSize (uint64_t bytes)
{
    if (bytes < 1024)
    {
        return std::format (L"{} bytes", bytes);
    }

    return std::format (L"{:.1f} KB", bytes / 1024.0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::FormatModified
//
//  A host file's instant in local time, as Explorer shows it; a catalog's
//  wall-clock time as it was recorded.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassqueBrowser::FormatModified (int64_t unixSeconds, bool wallClock)
{
    __time64_t  seconds = (__time64_t) unixSeconds;
    tm          local   = {};
    errno_t     err     = wallClock ? _gmtime64_s (&local, &seconds) : _localtime64_s (&local, &seconds);



    if (err != 0)
    {
        return std::wstring();
    }

    return std::format (L"{:04}-{:02}-{:02} {:02}:{:02}", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                        local.tm_hour, local.tm_min);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::FormatSelection
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassqueBrowser::FormatSelection (size_t selected, size_t total)
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
//  CassqueBrowser::JoinPath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassqueBrowser::JoinPath (const std::wstring & folder, const std::wstring & name)
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
//  CassqueBrowser::GetParentFolder
//
//  A drive root such as C:\ has no parent; the folder directly under it
//  keeps the trailing separator, since C: alone means the current directory.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassqueBrowser::GetParentFolder (const std::wstring & path)
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
//  CassqueBrowser::OpenRow
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueBrowser::OpenRow (int row)
{
    Location  location = GetLocation();
    Location  target;



    if (row < 0 || row >= (int) m_rows.size())
    {
        return false;
    }

    if (!m_rootChildren.empty() && m_rows[row].sourceIndex < m_rootChildren.size())
    {
        target = m_rootChildren[m_rows[row].sourceIndex].location;
        m_rootId.clear();
    }
    else if (location.kind != Location::Kind::HostFolder)
    {
        return false;
    }
    else if (m_rows[row].isDirectory)
    {
        target = Location::MakeHostFolder (JoinPath (location.path, m_rows[row].name));
    }
    else if (m_rows[row].isDiskImage)
    {
        target = Location::MakeDiskImage (JoinPath (location.path, m_rows[row].name));
    }
    else
    {
        return false;
    }

    m_model.NavigateTo (target);
    ReloadAfterNavigation();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::CanGoUp
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueBrowser::CanGoUp() const
{
    Location  location = GetLocation();



    switch (location.kind)
    {
        case Location::Kind::HostFolder:
            return !GetParentFolder (location.path).empty();

        case Location::Kind::DiskImage:
        case Location::Kind::DiskDirectory:
            return true;

        default:
            return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::GoUp
//
//  An image's parent is the folder holding it; a directory inside an image
//  goes to the image.
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueBrowser::GoUp()
{
    Location  location = GetLocation();
    Location  target;



    if (!CanGoUp())
    {
        return false;
    }

    if (location.kind == Location::Kind::DiskDirectory)
    {
        target = Location::MakeDiskImage (location.path);
    }
    else
    {
        target = Location::MakeHostFolder (GetParentFolder (location.path));
    }

    m_model.NavigateTo (target);
    ReloadAfterNavigation();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::GoBack
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueBrowser::GoBack()
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
//  CassqueBrowser::GoForward
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueBrowser::GoForward()
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
//  CassqueBrowser::GetSelectedEntries
//
////////////////////////////////////////////////////////////////////////////////

void CassqueBrowser::GetSelectedEntries (std::vector<FileEntry> & outEntries) const
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
//  CassqueBrowser::GetSelectedHostPaths
//
////////////////////////////////////////////////////////////////////////////////

void CassqueBrowser::GetSelectedHostPaths (std::vector<std::wstring> & outPaths) const
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
//  CassqueBrowser::AreSelectedRowsImages
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueBrowser::AreSelectedRowsImages() const
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
//  CassqueBrowser::Reload
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassqueBrowser::Reload (bool keepSelection)
{
    HRESULT                    hr = S_OK;
    std::vector<std::wstring>  names;
    std::vector<int>           rows;



    for (int row : m_selectedRows)
    {
        names.push_back (m_rows[row].name);
    }

    m_model.InvalidateAllCatalogs();
    hr = Refresh();

    if (!keepSelection || names.empty())
    {
        return hr;
    }

    for (size_t row = 0; row < m_rows.size(); row++)
    {
        if (std::find (names.begin(), names.end(), m_rows[row].name) != names.end())
        {
            rows.push_back ((int) row);
        }
    }

    SetSelectedRows (rows);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::TryGetNodePath
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueBrowser::TryGetNodePath (const std::wstring & id, std::wstring & outPath) const
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
//  CassqueBrowser::CanAddToCasso
//
//  Any host folder that is not already known, wherever it shows in the tree.
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueBrowser::CanAddToCasso (const std::wstring & id) const
{
    auto  found = m_nodes.find (id);



    return found != m_nodes.end()
        && found->second.location.kind == Location::Kind::HostFolder
        && found->second.kind != TreeNode::Kind::KnownFolder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::CanRemoveFromCasso
//
//  A known folder, whether or not it is still there.
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueBrowser::CanRemoveFromCasso (const std::wstring & id) const
{
    auto  found = m_nodes.find (id);



    return found != m_nodes.end() && found->second.kind == TreeNode::Kind::KnownFolder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::GetLocationLabel
//
//  A folder's own name, a drive's root as written, an image's file name, and
//  the innermost directory of a path inside an image.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassqueBrowser::GetLocationLabel (const Location & location)
{
    std::wstring  path  = location.path;
    size_t        slash = 0;
    std::string   inner = location.innerPath;



    switch (location.kind)
    {
        case Location::Kind::None:
            return L"Home";

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
//  CassqueBrowser::GetTabLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassqueBrowser::GetTabLabel (size_t index) const
{
    if (index >= m_model.GetTabCount())
    {
        return std::wstring();
    }

    return GetLocationLabel (m_model.GetTab (index).location);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::NewTab
//
////////////////////////////////////////////////////////////////////////////////

size_t CassqueBrowser::NewTab()
{
    //  Home, the way a browser and Explorer open one, rather than a second
    //  copy of the folder the tab it was opened from is showing.
    size_t  index = m_model.OpenTab (Location());



    m_rootId.clear();
    ReloadAfterNavigation();

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::SwitchTab
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueBrowser::SwitchTab (size_t index)
{
    std::vector<std::wstring>  names;
    std::vector<int>           rows;



    if (!m_model.SwitchTo (index))
    {
        return false;
    }

    names = m_model.GetActiveTab().selection;
    m_rootId.clear();
    ReloadAfterNavigation();

    for (size_t row = 0; row < m_rows.size(); row++)
    {
        if (std::find (names.begin(), names.end(), m_rows[row].name) != names.end())
        {
            rows.push_back ((int) row);
        }
    }

    SetSelectedRows (rows);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::CloseTab
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueBrowser::CloseTab (size_t index)
{
    if (m_model.GetTabCount() <= 1 || !m_model.CloseTab (index))
    {
        return false;
    }

    return SwitchTab (m_model.GetActiveIndex());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::RestoreTabs
//
////////////////////////////////////////////////////////////////////////////////

void CassqueBrowser::RestoreTabs (const std::vector<Location> & locations)
{
    bool  switched = false;



    if (locations.empty())
    {
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
//  CassqueBrowser::GetRowIcon
//
//  A host folder's rows are real files and folders, so each gets the shell's
//  icon for its path, and a disk image gets the icon registered for its
//  extension. A row inside an image has no host path, so it gets the generic
//  folder or file icon.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> CassqueBrowser::GetRowIcon (const CatalogRow & row, const Location & at, IShellIcons & icons)
{
    if (at.kind == Location::Kind::HostFolder)
    {
        return icons.GetForPath (JoinPath (at.path, row.name));
    }

    return icons.GetForKind (row.isDirectory ? IShellIcons::Kind::Folder : IShellIcons::Kind::File);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::GetNodeIcon
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> CassqueBrowser::GetNodeIcon (const TreeNode & node, IShellIcons & icons)
{
    switch (node.kind)
    {
        case TreeNode::Kind::CassoRoot:     return icons.GetForKind (IShellIcons::Kind::Casso);
        case TreeNode::Kind::ThisPcRoot:    return icons.GetForKind (IShellIcons::Kind::ThisPc);
        case TreeNode::Kind::DiskDirectory: return icons.GetForKind (IShellIcons::Kind::Folder);
        default:                            return icons.GetForPath (node.location.path);
    }
}
