#include "Pch.h"

#include "CassoExplorer/Model/AppleFileProperties.h"
#include "Core/NumberFormat.h"
#include "Machines/Apple2/Common/ProDosTypeNames.h"
#include "Ui/DiskInspector/Blocks/BlockFilesLevel.h"
#include "Ui/DiskInspector/Blocks/BlockLevels.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesLevel::Init
//
////////////////////////////////////////////////////////////////////////////////

void BlockFilesLevel::Init (const IDxuiTheme * theme, HWND hwnd)
{
    vector<DxuiListView::Column>  columns (kColumnCount);



    m_search = CreateChild<DxuiSearchBox>();
    m_tree   = CreateChild<DxuiTreeView>();
    m_list   = CreateChild<DxuiListView>();

    m_search->SetTheme       (theme);
    m_search->SetHwnd        (hwnd);
    m_search->SetPlaceholder (L"Search the volume");
    m_search->SetOnChange    ([this] (const std::wstring & text) { m_query = text; FillRows(); });

    m_tree->SetShowCheckboxes (false);
    m_tree->SetOnSelect       ([this] (const std::wstring & id)
    {
        int  directory = std::stoi (id);

        m_search->Clear();
        m_query.clear();
        m_directory = directory;
        FillRows();

        if (m_onDirectory)
        {
            m_onDirectory (directory);
        }
    });

    columns[kName]     = { L"Name",      150 };
    columns[kType]     = { L"Type",      60 };
    columns[kAux]      = { L"Aux",       64 };
    columns[kBlocks]   = { L"Blocks",    64, false, DxuiTextHAlign::Right };
    columns[kEof]      = { L"EOF",       84, false, DxuiTextHAlign::Right };
    columns[kStorage]  = { L"Storage",   90 };
    columns[kKey]      = { L"Key block", 76, false, DxuiTextHAlign::Right };
    columns[kModified] = { L"Modified",  140, true };

    m_list->SetTheme              (theme);
    m_list->SetShowHeader         (true);
    m_list->SetActivateOnDoubleClick (true);
    m_list->SetAlwaysShowSelection (true);
    m_list->SetOwnerWindow        (hwnd);
    m_list->SetColumns            (std::move (columns));
    m_list->SetSortIndicator      (m_sortColumn, m_isDescending);
    m_list->SetOnSelectionChanged ([this] (int row) { OnRowSelected (row); });
    m_list->SetOnActivateRow      ([this] (int row) { OnRowActivated (row); });
    m_list->SetOnSortColumn       ([this] (int column)
    {
        m_isDescending = column == m_sortColumn && !m_isDescending;
        m_sortColumn   = column;
        m_list->SetSortIndicator (m_sortColumn, m_isDescending);
        FillRows();
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesLevel::SetVolume
//
////////////////////////////////////////////////////////////////////////////////

void BlockFilesLevel::SetVolume (const BlockMap * map, uint32_t first)
{
    DxuiTreeNode  root;



    m_map       = map;
    m_first     = first;
    m_directory = -1;
    m_query.clear();

    m_children.assign (map != nullptr ? map->entries.size() + 1 : 1, vector<int>());
    m_blockCounts.clear();

    if (map != nullptr)
    {
        for (int i = 0; i < (int) map->entries.size(); i++)
        {
            m_children[(size_t) (map->entries[i].parent + 1)].push_back (i);
        }

        for (const vector<uint32_t> & blocks : map->GetBlocksOfEach())
        {
            m_blockCounts.push_back ((uint32_t) blocks.size());
        }

        root.id       = MakeId (-1);
        root.label    = map->volumeName.empty() ? std::wstring (L"Volume") : map->volumeName;
        root.expanded = true;
        root.children = BuildNodes (-1);
    }

    m_search->Clear();
    m_tree->SetNodes (map != nullptr ? vector<DxuiTreeNode> { root } : vector<DxuiTreeNode>());
    m_tree->HighlightRow (0);

    FillRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesLevel::BuildNodes
//
//  The directories in `directory`, each closed, with the directories in
//  it below it.
//
////////////////////////////////////////////////////////////////////////////////

vector<DxuiTreeNode> BlockFilesLevel::BuildNodes (int directory) const
{
    vector<DxuiTreeNode>  nodes;



    for (int entry : m_children[(size_t) (directory + 1)])
    {
        DxuiTreeNode  node;

        if (!m_map->entries[entry].isDirectory)
        {
            continue;
        }

        node.id       = MakeId (entry);
        node.label    = m_map->entries[entry].name;
        node.expanded = false;
        node.children = BuildNodes (entry);
        nodes.push_back (std::move (node));
    }

    return nodes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesLevel::ShowDirectory
//
//  The directory's ancestors are opened in the tree, so its row is there.
//
////////////////////////////////////////////////////////////////////////////////

void BlockFilesLevel::ShowDirectory (int directory)
{
    vector<int>  chain;
    int          at  = directory;
    int          row = 0;



    if (m_map == nullptr)
    {
        return;
    }

    while (at >= 0 && (size_t) at < m_map->entries.size() && chain.size() <= m_map->entries.size())
    {
        chain.push_back (at);
        at = m_map->entries[at].parent;
    }

    chain.push_back (-1);

    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        row = m_tree->FindRowById (MakeId (*it));

        if (row >= 0 && *it != directory)
        {
            (void) m_tree->SetRowExpanded (row, true);
        }
    }

    row = m_tree->FindRowById (MakeId (directory));

    if (row >= 0)
    {
        m_tree->HighlightRow (row);
    }

    m_search->Clear();
    m_query.clear();
    m_directory = directory;
    FillRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesLevel::SelectEntry
//
////////////////////////////////////////////////////////////////////////////////

void BlockFilesLevel::SelectEntry (int entry)
{
    auto  found = m_rows.end();



    if (m_map == nullptr || entry < 0 || (size_t) entry >= m_map->entries.size())
    {
        return;
    }

    if (m_query.empty() && m_map->entries[entry].parent != m_directory)
    {
        ShowDirectory (m_map->entries[entry].parent);
    }

    found = std::find (m_rows.begin(), m_rows.end(), entry);

    if (found != m_rows.end())
    {
        m_list->SetSelectedRow ((int) (found - m_rows.begin()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesLevel::SetSearch
//
////////////////////////////////////////////////////////////////////////////////

void BlockFilesLevel::SetSearch (const std::wstring & text)
{
    m_search->SetText (text);
    m_query = text;
    FillRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesLevel::FillRows
//
//  The selected directory's entries, or with a search, every entry whose
//  name has the text, matched without case, listed by path.
//
////////////////////////////////////////////////////////////////////////////////

void BlockFilesLevel::FillRows()
{
    vector<vector<DxuiListView::Cell>>  rows;
    std::wstring                        query = m_query;
    std::wstring                        name;



    m_rows.clear();

    if (m_map == nullptr || (size_t) (m_directory + 1) >= m_children.size())
    {
        m_list->SetRows ({});
        return;
    }

    std::transform (query.begin(), query.end(), query.begin(), ::towlower);

    if (query.empty())
    {
        m_rows = m_children[(size_t) (m_directory + 1)];
    }
    else
    {
        for (int i = 0; i < (int) m_map->entries.size(); i++)
        {
            name = m_map->entries[i].name;
            std::transform (name.begin(), name.end(), name.begin(), ::towlower);

            if (name.find (query) != std::wstring::npos)
            {
                m_rows.push_back (i);
            }
        }
    }

    SortRows();

    for (int entry : m_rows)
    {
        vector<DxuiListView::Cell>  cells;

        for (std::wstring & text : BuildCells (*m_map, entry, m_first, !query.empty(), (size_t) entry < m_blockCounts.size() ? m_blockCounts[entry] : 0))
        {
            cells.push_back ({ std::move (text) });
        }

        rows.push_back (std::move (cells));
    }

    m_list->SetRows (std::move (rows));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesLevel::SortRows
//
//  Directories before files, as Casso Explorer lists them, then by the
//  sort column, then by name.
//
////////////////////////////////////////////////////////////////////////////////

void BlockFilesLevel::SortRows()
{
    const vector<BlockFileEntry>  & entries = m_map->entries;
    const vector<uint32_t>        & counts  = m_blockCounts;
    int                             column  = m_sortColumn;
    bool                            isDown  = m_isDescending;



    auto  key = [&entries, &counts, column] (int i) -> uint64_t
    {
        const BlockFileEntry  & e = entries[i];

        switch (column)
        {
            case kType:     return e.type;
            case kAux:      return e.aux;
            case kBlocks:   return (size_t) i < counts.size() ? counts[i] : 0;
            case kEof:      return e.eof;
            case kStorage:  return e.storage;
            case kKey:      return e.keyBlock;
            case kModified: return ((uint64_t) e.date << 16) | e.time;
            default:        return 0;
        }
    };

    std::stable_sort (m_rows.begin(), m_rows.end(), [&] (int a, int b)
    {
        int       byName = _wcsicmp (entries[a].name.c_str(), entries[b].name.c_str());
        uint64_t  keyA   = key (a);
        uint64_t  keyB   = key (b);

        if (entries[a].isDirectory != entries[b].isDirectory)
        {
            return entries[a].isDirectory;
        }

        if (keyA != keyB)
        {
            return isDown ? keyA > keyB : keyA < keyB;
        }

        return isDown && column == kName ? byName > 0 : byName < 0;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesLevel::BuildCells
//
//  A ProDOS entry has its type, aux type, EOF, storage and date; another
//  file system's has its name, blocks and key block.
//
////////////////////////////////////////////////////////////////////////////////

vector<std::wstring> BlockFilesLevel::BuildCells (const BlockMap & map, int entry, uint32_t first, bool isPath, uint32_t blocks)
{
    const BlockFileEntry  & e        = map.entries[entry];
    bool                    isProDos = map.fileSystem == BlockFileSystem::ProDos || map.fileSystem == BlockFileSystem::Sos;
    vector<std::wstring>    cells (kColumnCount);



    cells[kName]   = isPath ? map.files[entry] : e.name;
    cells[kBlocks] = NumberFormat::FormatInteger (blocks);
    cells[kKey]    = e.keyBlock != BlockFileEntry::kNoBlock && e.keyBlock < map.GetBlockCount() ? NumberFormat::FormatInteger (first + e.keyBlock) : std::wstring();

    if (isProDos)
    {
        cells[kType]     = ProDosTypeNames::Format (e.isDirectory ? (Byte) 0x0F : e.type);
        cells[kAux]      = e.isDirectory ? std::wstring() : std::format (L"${:04X}", e.aux);
        cells[kEof]      = e.isDirectory ? std::wstring() : NumberFormat::FormatInteger (e.eof);
        cells[kStorage]  = BlockLevels::GetStorageName (e);
        cells[kModified] = AppleFileProperties::FormatDate (e.date, e.time);
    }
    else if (e.isDirectory)
    {
        cells[kType] = L"Directory";
    }

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesLevel::OnRowSelected
//
////////////////////////////////////////////////////////////////////////////////

void BlockFilesLevel::OnRowSelected (int row)
{
    if (row >= 0 && row < (int) m_rows.size() && m_onSelect)
    {
        m_onSelect (m_rows[row]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesLevel::OnRowActivated
//
//  A directory opens in the tree; a file opens at the File level.
//
////////////////////////////////////////////////////////////////////////////////

void BlockFilesLevel::OnRowActivated (int row)
{
    int  entry = 0;



    if (row < 0 || row >= (int) m_rows.size() || m_map == nullptr)
    {
        return;
    }

    entry = m_rows[row];

    if (m_map->entries[entry].isDirectory)
    {
        ShowDirectory (entry);

        if (m_onDirectory)
        {
            m_onDirectory (entry);
        }

        return;
    }

    if (m_onOpen)
    {
        m_onOpen (entry);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesLevel::Layout
//
//  The search box across the top, then the tree and the list side by side.
//
////////////////////////////////////////////////////////////////////////////////

void BlockFilesLevel::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int  search = scaler.ToPx (kSearchDip);
    int  gap    = scaler.ToPx (kGapDip);
    int  split  = boundsPx.left + (int) ((boundsPx.right - boundsPx.left) * kTreeShare);
    RECT tree   = { boundsPx.left, boundsPx.top + search + gap, split, boundsPx.bottom };
    RECT list   = { split + gap, boundsPx.top + search + gap, boundsPx.right, boundsPx.bottom };



    SetBounds (boundsPx);

    m_search->SetDpi (scaler.GetDpi());
    m_tree->SetDpi   (scaler.GetDpi());
    m_list->SetDpi   (scaler.GetDpi());

    m_search->Layout ({ boundsPx.left, boundsPx.top, boundsPx.right, boundsPx.top + search }, scaler);
    m_tree->Layout   (tree, scaler);
    m_list->Layout   (list, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesLevel::Paint
//
//  The children, and a divider between the tree and the list.
//
////////////////////////////////////////////////////////////////////////////////

void BlockFilesLevel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT  tree = m_tree->GetBounds();



    if (!IsVisible())
    {
        return;
    }

    DxuiPanel::Paint (painter, text, theme);

    painter.FillRect ((float) tree.right + 1.0f, (float) tree.top, 1.0f, (float) (tree.bottom - tree.top), theme.Divider());
}
