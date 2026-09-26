#include "Pch.h"

#include "Core/AppleSingleCodec.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "Core/TextEncoding.h"





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
    out.dividerAbove   = node.kind == TreeNode::Kind::ThisPcRoot;   // Explorer's line above This PC

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
    //  A known folder that is gone is dimmed. An image that will not open is
    //  not: the preview says why when it is chosen.
    out.dimmed         = node.missing;

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

    switch (location.kind)
    {
        case Location::Kind::HostFolder:
            hr = LoadHostFolder (location.path);
            break;

        case Location::Kind::DiskImage:
        case Location::Kind::DiskDirectory:
            hr = LoadImage (location.path, (location.kind == Location::Kind::DiskDirectory) ? location.innerPath : std::string());
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
            m_listError = TextEncoding::NarrowToWide (result.message);
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
            m_preview.kind    = PreviewContent::Kind::Error;
            m_preview.message = L"A folder. Open it to see what it holds.";
            return;
        }

        result = m_operations.Read (TextEncoding::WideToNarrow (location.path), GetEntryPath (*entry), payload, entry->catalogIndex);

        if (!result.Succeeded() && PreviewDecoder::ParseDetails (result.message, m_preview.details))
        {
            m_preview.kind = PreviewContent::Kind::Details;
            return;
        }

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
            m_preview.message = TextEncoding::NarrowToWide (result.message);
            return;
        }

        m_model.CacheCatalog (imagePath, listing, kind);
    }

    PreviewDecoder::RenderCatalog (listing, kind, m_preview);
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
//  CassoExplorerBrowser::GetColumns
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Column> CassoExplorerBrowser::GetColumns()
{
    std::vector<DxuiListView::Column>  columns;



    columns.push_back (DxuiListView::Column { L"Name",     200, true,  DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Type",     0,   false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Size",     0,   false, DxuiTextHAlign::Right });
    columns.push_back (DxuiListView::Column { L"Address",  0,   false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Locked",   0,   false, DxuiTextHAlign::Left  });
    columns.push_back (DxuiListView::Column { L"Modified", 0,   false, DxuiTextHAlign::Left  });

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
    cells.push_back (DxuiListView::Cell { row.isDirectory ? std::wstring() : FormatSize (row.sizeBytes), false });

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
//  CassoExplorerBrowser::FormatSize
//
//  Bytes up to a kilobyte, then kilobytes with one decimal: an Apple file is
//  small enough that the byte count is what someone wants to see.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerBrowser::FormatSize (uint64_t bytes)
{
    if (bytes < 1024)
    {
        return std::format (L"{} bytes", bytes);
    }

    return std::format (L"{:.1f} KB", bytes / 1024.0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::FormatModified
//
//  A host file's instant in local time, as Explorer shows it; a catalog's
//  wall-clock time as it was recorded.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoExplorerBrowser::FormatModified (int64_t unixSeconds, bool wallClock)
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



    if (!TryGetRowLocation (row, target))
    {
        return false;
    }

    if (!m_rootChildren.empty())
    {
        m_rootId.clear();
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

        default:
            return false;
    }
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



    m_rootId.clear();
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
    m_rootId.clear();
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



    m_rootId.clear();
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
//  icon for its path, and a disk image gets the icon registered for its
//  extension. A row inside an image has no host path, so it gets the generic
//  folder or file icon.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> CassoExplorerBrowser::GetRowIcon (const CatalogRow & row, const Location & at, IShellIcons & icons)
{
    if (at.kind == Location::Kind::HostFolder)
    {
        return icons.GetForPath (JoinPath (at.path, row.name), row.isDirectory);
    }

    return icons.GetForKind (row.isDirectory ? IShellIcons::Kind::Folder : IShellIcons::Kind::File);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowser::GetNodeIcon
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> CassoExplorerBrowser::GetNodeIcon (const TreeNode & node, IShellIcons & icons)
{
    switch (node.kind)
    {
        case TreeNode::Kind::CassoRoot:     return icons.GetForKind (IShellIcons::Kind::Casso);
        case TreeNode::Kind::ThisPcRoot:    return icons.GetForKind (IShellIcons::Kind::ThisPc);
        case TreeNode::Kind::DiskDirectory: return icons.GetForKind (IShellIcons::Kind::Folder);
        //  Every remaining node is a drive or a host folder, and a disk image
        //  node is a file with the icon its extension gives it.
        case TreeNode::Kind::DiskImage:     return icons.GetForPath (node.location.path, false);
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
    m_rootId.clear();

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
    bool      parsed = BrowserModel::ParseAddress (m_fs, text, location);



    if (parsed)
    {
        NavigateToLocation (location);

        //  After the navigation, so that a path going nowhere is never offered
        //  back.
        m_typedPaths.Add (text);
    }

    return parsed;
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
