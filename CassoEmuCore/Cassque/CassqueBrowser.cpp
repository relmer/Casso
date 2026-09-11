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

DxuiTreeNode CassqueBrowser::ToTreeNode (const TreeNode & node)
{
    DxuiTreeNode  out;



    out.id             = node.id;
    out.label          = node.label;
    out.expanded       = false;
    out.childrenLoaded = !node.canExpand;
    out.dimmed         = node.missing || !node.loadError.empty();

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
        outNodes.push_back (ToTreeNode (node));
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
        out.push_back (ToTreeNode (node));
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
            break;
    }

    SortRows();
    UpdatePreview();
    UpdateStatus();

    return hr;
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
    columns.push_back (DxuiListView::Column { L"Type",     90,  false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Size",     80,  false, DxuiTextHAlign::Right });
    columns.push_back (DxuiListView::Column { L"Address",  70,  false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Locked",   60,  false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Modified", 130, false, DxuiTextHAlign::Left  });

    return columns;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowser::ToCells
//
//  A folder has no size to show, and an entry that records no date leaves the
//  column blank rather than showing 1970.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Cell> CassqueBrowser::ToCells (const CatalogRow & row)
{
    std::vector<DxuiListView::Cell>  cells;



    cells.push_back (DxuiListView::Cell { row.name,     false });
    cells.push_back (DxuiListView::Cell { row.typeText, false });
    cells.push_back (DxuiListView::Cell { row.isDirectory ? std::wstring() : FormatSize (row.sizeBytes), false });
    cells.push_back (DxuiListView::Cell { row.addressText, false });
    cells.push_back (DxuiListView::Cell { row.locked ? L"Yes" : L"", false });
    cells.push_back (DxuiListView::Cell { row.hasModified ? FormatModified (row.modifiedUnix) : std::wstring(), false });

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
//  In local time, as Explorer shows it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassqueBrowser::FormatModified (int64_t unixSeconds)
{
    __time64_t  seconds = (__time64_t) unixSeconds;
    tm          local   = {};
    errno_t     err     = _localtime64_s (&local, &seconds);



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
