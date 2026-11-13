#include "Pch.h"

#include "Ui/Debugger/Panes/ToolPanes.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Core/UnicodeSymbols.h"
#include "Debugger/SymbolTable.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerMenuCommand.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FindSymbolName
//
//  The first symbol at an address, or empty.
//
////////////////////////////////////////////////////////////////////////////////

static std::string FindSymbolName (const DebuggerViewSnapshot::SymbolsState * symbols, Word address)
{
    if (symbols == nullptr)
    {
        return {};
    }

    for (const SymbolInfo & symbol : symbols->symbols)
    {
        if (symbol.address == address && !symbol.isConstant)
        {
            return symbol.name;
        }
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolListPane::ToolListPane
//
////////////////////////////////////////////////////////////////////////////////

ToolListPane::ToolListPane (IDebuggerPaneHost & host, std::wstring id, std::wstring title) :
    IDebuggerPane (host),
    m_id          (std::move (id)),
    m_title       (std::move (title))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolListPane::~ToolListPane
//
////////////////////////////////////////////////////////////////////////////////

ToolListPane::~ToolListPane() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  ToolListPane::CreateControls
//
////////////////////////////////////////////////////////////////////////////////

void ToolListPane::CreateControls()
{
    m_list = m_host.GetControlParent().CreateChild<DxuiListView>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolListPane::AddToDock
//
//  The pane is its list, with its commands on its rows' menus.
//
////////////////////////////////////////////////////////////////////////////////

void ToolListPane::AddToDock (DxuiDockSite & dock)
{
    m_frame = std::make_unique<DebuggerPaneFrame> (m_title);
    m_frame->AddPart (m_list);

    dock.AddPane (m_id, m_title, m_frame.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolListPane::GetContent
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * ToolListPane::GetContent() const
{
    return m_frame.get();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolListPane::MakeNote
//
//  One muted cell across the row.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Cell> ToolListPane::MakeNote (const std::wstring & text)
{
    std::vector<DxuiListView::Cell>  cells (1, DxuiListView::Cell { text, true });



    cells[0].spansRow = true;
    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolListPane::AddCopyItem
//
////////////////////////////////////////////////////////////////////////////////

void ToolListPane::AddCopyItem (PaneMenuItems & items)
{
    items.push_back ({ L"Copy", [this] { DxuiClipboard::SetText (m_host.GetOwnerHwnd(), m_list->GetSelectionText()); } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolListPane::PickSaveFile
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ToolListPane::PickSaveFile (const wchar_t * filterName, const wchar_t * filter, const wchar_t * extension, const std::wstring & fileName)
{
    HRESULT                hr      = S_OK;
    IHostDialogs         * dialogs = m_host.GetHostDialogs();
    FileDialogSpec         spec;
    std::filesystem::path  chosen;
    bool                   picked  = false;



    BAIL_OUT_IF (dialogs == nullptr, S_OK);

    spec.filters          = { { filterName, filter }, { L"All files", L"*.*" } };
    spec.defaultExtension = extension;
    spec.defaultFileName  = fileName;

    hr = dialogs->PickFileToSave (m_host.GetOwnerHwnd(), spec, chosen, picked);
    CHR (hr);

Error:
    return picked ? chosen.wstring() : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolsPane::SymbolsPane
//
////////////////////////////////////////////////////////////////////////////////

SymbolsPane::SymbolsPane (IDebuggerPaneHost & host, Kind kind) :
    ToolListPane (host, (kind == Kind::User) ? DebuggerLayout::kSymbols : DebuggerLayout::kMachineSymbols,
                        (kind == Kind::User) ? L"User symbols" : L"Machine symbols"),
    m_kind       (kind)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolsPane::Configure
//
////////////////////////////////////////////////////////////////////////////////

void SymbolsPane::Configure()
{
    m_list->SetColumns ({ { L"Symbol",  0, false, DxuiTextHAlign::Left },
                          { L"Address", 0, true,  DxuiTextHAlign::Left } });

    m_list->SetActivateOnDoubleClick (true);
    m_list->SetSortIndicator         ((int) m_sort, m_isReverse);
    m_list->SetOnSortColumn          ([this] (int column) { SortBy (column); });

    m_list->SetOnActivateRow ([this] (int row)
    {
        const Row  * each = GetRow (row);

        //  A double-click's press has opened or closed the heading already.
        bool  isMouse = DxuiInputState::IsKeyDown (VK_LBUTTON);



        if (each != nullptr && !each->isHeading)
        {
            m_host.GoToCode (each->address);
        }
        else if (each != nullptr && !isMouse)
        {
            ToggleTable (each->table);
        }
    });

    m_list->SetSelectOnCheckPress (false);
    m_list->SetOnCheckToggled ([this] (int row, size_t, bool checked)
    {
        const Row  * each = GetRow (row);



        if (each != nullptr && each->isHeading)
        {
            m_host.RunAction (DebuggerActions::GetEnableSymbols (SymbolTable::ToUpper (each->table), checked, m_host.GetMode()));
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolListPane::ShowNewListRow
//
//  A row's key is the text of its first two cells.
//
////////////////////////////////////////////////////////////////////////////////

void ToolListPane::ShowNewListRow()
{
    std::vector<std::wstring>  keys;



    for (int row = 0; row < m_list->GetRowCount(); row++)
    {
        const std::vector<DxuiListView::Cell>  & cells = m_list->GetCellsOfRow (row);

        keys.push_back (std::format (L"{}\t{}", cells.empty() ? L"" : cells[0].text, cells.size() < 2 ? L"" : cells[1].text));
    }

    m_list->ShowNewRow (keys);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolListPane::GetActedRows
//
////////////////////////////////////////////////////////////////////////////////

std::vector<int> ToolListPane::GetActedRows (int row) const
{
    std::vector<int>  selected = m_list->GetSelectedRows();



    if (row < 0 || std::ranges::find (selected, row) != selected.end())
    {
        return selected;
    }

    return { row };
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolsPane::Apply
//
////////////////////////////////////////////////////////////////////////////////

void SymbolsPane::Apply (const DebuggerViewSnapshot & snapshot)
{
    if (snapshot.symbols == nullptr || snapshot.symbols == m_symbols)
    {
        return;
    }

    m_symbols = snapshot.symbols;
    RebuildRows();
    ShowNewListRow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolsPane::RebuildRows
//
////////////////////////////////////////////////////////////////////////////////

void SymbolsPane::RebuildRows()
{
    if (m_symbols == nullptr)
    {
        return;
    }

    m_rows = GetRows (*m_symbols, m_kind, m_open, m_sort, m_isReverse);
    m_list->SetRows (GetCells (*m_symbols, m_rows, m_open));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolsPane::SortBy
//
//  A click on a heading sorts by that column, and a second click on the
//  same heading reverses the order.
//
////////////////////////////////////////////////////////////////////////////////

void SymbolsPane::SortBy (int column)
{
    Sort  sort = (column == 1) ? Sort::Address : Sort::Name;



    m_isReverse = (sort == m_sort) && !m_isReverse;
    m_sort      = sort;
    m_list->SetSortIndicator ((int) m_sort, m_isReverse);
    RebuildRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolsPane::ToggleTable
//
////////////////////////////////////////////////////////////////////////////////

void SymbolsPane::ToggleTable (const std::string & table)
{
    if (!m_open.erase (table))
    {
        m_open.insert (table);
    }

    RebuildRows();
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolsPane::RouteContentMouse
//
//  A press on a heading opens or closes it at once, except on its check,
//  which turns the table on or off.
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolsPane::RouteContentMouse (const DxuiMouseEvent & ev)
{
    RECT          bounds = m_list->GetBounds();
    RECT          check  = {};
    POINT         local  = { ev.positionDip.x - bounds.left, ev.positionDip.y - bounds.top };
    int           row    = -1;
    const Row   * each   = nullptr;



    if (ev.kind != DxuiMouseEventKind::Down || ev.button != DxuiMouseButton::Left ||
        !m_list->IsVisible() || !m_host.IsRoutable (m_list) || !DxuiDockSite::Contains (bounds, ev.positionDip))
    {
        return false;
    }

    row  = m_list->HitTestRow (local.x, local.y);
    each = GetRow (row);

    if (each == nullptr || !each->isHeading || (m_list->GetCheckRectPx (row, 0, check) && PtInRect (&check, local)))
    {
        return false;
    }

    m_list->SetSelectedRow (row);
    ToggleTable (each->table);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolsPane::GetRow
//
////////////////////////////////////////////////////////////////////////////////

const SymbolsPane::Row * SymbolsPane::GetRow (int row) const
{
    return (row >= 0 && row < (int) m_rows.size()) ? &m_rows[(size_t) row] : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolsPane::AddMenuItems
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolsPane::AddMenuItems (DxuiListView * list, int row, PaneMenuItems & items)
{
    const Row  * each = GetRow (row);



    if (list != m_list)
    {
        return false;
    }

    if (each != nullptr && !each->isHeading)
    {
        items.push_back ({ L"Go to",  [this, at = each->address] { m_host.GoToCode (at); } });
        items.push_back ({ L"Remove", [this, table = each->table, name = each->name]
        {
            m_host.RunAction (DebuggerActions::GetRemoveSymbol (SymbolTable::ToUpper (table), name, m_host.GetMode()));
        } });
    }

    //  A table loaded from a file unloads it; any other is cleared.
    if (each != nullptr)
    {
        std::string  file  = (m_symbols != nullptr) ? GetFileOf (*m_symbols, each->table) : std::string();
        std::wstring label = file.empty() ? std::wstring (L"Clear table") : L"Unload " + TextEncoding::NarrowToWide (file);

        items.push_back ({ label,            [this, table = each->table] { m_host.RunAction (DebuggerActions::GetClearSymbols (SymbolTable::ToUpper (table), m_host.GetMode())); } });
        items.push_back ({ L"Save table...", [this, table = each->table] { SaveTable (table); } });
    }

    AddCopyItem (items);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolsPane::SaveTable
//
////////////////////////////////////////////////////////////////////////////////

void SymbolsPane::SaveTable (const std::string & table)
{
    std::wstring  path = PickSaveFile (L"Symbol files", L"*.sym", L"sym", TextEncoding::NarrowToWide (table) + L".sym");



    if (!path.empty())
    {
        m_host.RunAction (DebuggerActions::GetSaveSymbols (SymbolTable::ToUpper (table), TextEncoding::WideToNarrow (path), m_host.GetMode()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolsPane::GetRows
//
//  The heading of each table the pane lists, then, under an open heading,
//  its symbols in the order chosen.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<SymbolsPane::Row> SymbolsPane::GetRows (const DebuggerViewSnapshot::SymbolsState & symbols, Kind kind, const std::set<std::string> & open,
                                                    Sort sort, bool isReverse)
{
    std::vector<Row>  rows;
    size_t            first = 0;



    for (const DebuggerViewSnapshot::SymbolTableRow & table : symbols.tables)
    {
        if (IsUserTable (table.id) != (kind == Kind::User))
        {
            continue;
        }

        //  USER2, SRC and SRC2 are AppleWin's spare tables, listed only once a
        //  SYMUSER2, SYMSRC or SYMSRC2 command has put something in one.
        if (table.id != SymbolTableId::User && IsUserTable (table.id) && table.count == 0 && table.origins.empty())
        {
            continue;
        }

        rows.push_back ({ table.name, std::string(), 0, true });
        first = rows.size();

        for (const SymbolInfo & symbol : symbols.symbols)
        {
            if (symbol.table == table.id && open.contains (table.name))
            {
                rows.push_back ({ table.name, symbol.name, symbol.address, false, symbol.isConstant });
            }
        }

        std::stable_sort (rows.begin() + (std::ptrdiff_t) first, rows.end(), [sort, isReverse] (const Row & left, const Row & right)
        {
            int  byName    = _stricmp (left.name.c_str(), right.name.c_str());
            int  byAddress = (left.address == right.address) ? 0 : (left.address < right.address) ? -1 : 1;
            int  order     = (sort == Sort::Name) ? (byName != 0 ? byName : byAddress) : (byAddress != 0 ? byAddress : byName);



            return isReverse ? order > 0 : order < 0;
        });
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolsPane::IsUserTable
//
//  The tables SYM LOAD and SYM add to: USER, USER2, SRC and SRC2. A BASIC
//  program in memory doesn't have symbols in any table; the BASIC table is
//  Applesoft's, so it is the machine's.
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolsPane::IsUserTable (SymbolTableId table)
{
    return table == SymbolTableId::User || table == SymbolTableId::User2 || table == SymbolTableId::Src || table == SymbolTableId::Src2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolsPane::GetFileOf
//
//  A load replaces its table, so the table's last origin is the file whose
//  symbols it has.
//
////////////////////////////////////////////////////////////////////////////////

std::string SymbolsPane::GetFileOf (const DebuggerViewSnapshot::SymbolsState & symbols, const std::string & table)
{
    auto  found = std::ranges::find_if (symbols.tables, [&table] (const DebuggerViewSnapshot::SymbolTableRow & each) { return each.name == table; });



    return (found == symbols.tables.end() || found->origins.empty()) ? std::string() : found->origins.back();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolsPane::GetCells
//
//  A heading has the table's check, a triangle that points down while it is
//  open, the name of the file the table was loaded from, or the table's
//  name for one filled any other way, and its count. A constant's value is
//  marked as one, since it is not an address.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::vector<DxuiListView::Cell>> SymbolsPane::GetCells (const DebuggerViewSnapshot::SymbolsState & symbols, const std::vector<Row> & rows, const std::set<std::string> & open)
{
    std::vector<std::vector<DxuiListView::Cell>>  cells;



    for (const Row & row : rows)
    {
        if (row.isHeading)
        {
            auto                table = std::ranges::find_if (symbols.tables, [&row] (const DebuggerViewSnapshot::SymbolTableRow & each) { return each.name == row.table; });
            std::string         file  = GetFileOf (symbols, row.table);
            DxuiListView::Cell  name  = { std::format (L"{} {} ({})", open.contains (row.table) ? s_kpszSmallTriangleDown : s_kpszSmallTriangleRight,
                                                       TextEncoding::NarrowToWide (file.empty() ? row.table : file), table->count) };



            name.check = DxuiListView::Cell::GetCheck (table->enabled);
            name.tip   = file.empty()          ? std::wstring()
                       : table->path.empty()   ? std::format (L"In the {} table", TextEncoding::NarrowToWide (row.table))
                       :                         std::format (L"{}\nIn the {} table", table->path, TextEncoding::NarrowToWide (row.table));
            cells.push_back ({ name, { L"" } });
            continue;
        }

        cells.push_back ({ { L"   " + TextEncoding::NarrowToWide (row.name) },
                           { row.isConstant ? std::format (L"{:04X} (constant)", row.address) : std::format (L"${:04X}", row.address) } });
    }

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BookmarksPane::BookmarksPane
//
////////////////////////////////////////////////////////////////////////////////

BookmarksPane::BookmarksPane (IDebuggerPaneHost & host) :
    ToolListPane (host, DebuggerLayout::kBookmarks, L"Bookmarks")
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BookmarksPane::Configure
//
////////////////////////////////////////////////////////////////////////////////

void BookmarksPane::Configure()
{
    m_list->SetColumns ({ { L"Bookmark", 0, false, DxuiTextHAlign::Left },
                          { L"Address",  0, false, DxuiTextHAlign::Left },
                          { L"Symbol",   0, true,  DxuiTextHAlign::Left } });

    m_list->SetMultiSelect           (true);
    m_list->SetActivateOnDoubleClick (true);
    m_list->SetOnSortColumn          ([this] (int column) { SortBy (column); });

    m_list->SetOnActivateRow ([this] (int row)
    {
        std::optional<DebuggerViewSnapshot::WatchLine>  bookmark = GetBookmark (row);



        if (bookmark.has_value())
        {
            m_host.GoToCode (bookmark->address);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BookmarksPane::Apply
//
////////////////////////////////////////////////////////////////////////////////

void BookmarksPane::Apply (const DebuggerViewSnapshot & snapshot)
{
    m_bookmarks = GetSorted (snapshot.bookmarks, snapshot.symbols.get(), m_sort, m_isReverse);
    m_list->SetRows (GetCells (m_bookmarks, snapshot.symbols.get()));
    ShowNewListRow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BookmarksPane::SortBy
//
//  A click on a heading sorts by that column, and a second click on the
//  same heading reverses the order. The selected bookmark is selected in
//  its new row.
//
////////////////////////////////////////////////////////////////////////////////

void BookmarksPane::SortBy (int column)
{
    const DebuggerViewSnapshot                      * snapshot = m_host.GetLatestSnapshot();
    std::optional<DebuggerViewSnapshot::WatchLine>    selected = GetBookmark (m_list->GetSelectedRow());



    m_isReverse = (column == m_sort) && !m_isReverse;
    m_sort      = column;
    m_list->SetSortIndicator (m_sort, m_isReverse);

    if (snapshot == nullptr)
    {
        return;
    }

    Apply (*snapshot);

    for (size_t row = 0; selected.has_value() && row < m_bookmarks.size(); row++)
    {
        if (m_bookmarks[row].id == selected->id)
        {
            m_list->SetSelectedRow ((int) row);
        }
    }

    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BookmarksPane::GetSorted
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DebuggerViewSnapshot::WatchLine> BookmarksPane::GetSorted (std::vector<DebuggerViewSnapshot::WatchLine>   bookmarks,
                                                                      const DebuggerViewSnapshot::SymbolsState     * symbols, int column, bool isReverse)
{
    if (column < 0 || column > 2)
    {
        return bookmarks;
    }

    std::ranges::stable_sort (bookmarks, [symbols, column, isReverse] (const DebuggerViewSnapshot::WatchLine & left, const DebuggerViewSnapshot::WatchLine & right)
    {
        int  order = (column == 0) ? left.id - right.id
                   : (column == 1) ? (int) left.address - (int) right.address
                   :                 _stricmp (FindSymbolName (symbols, left.address).c_str(), FindSymbolName (symbols, right.address).c_str());



        return isReverse ? order > 0 : order < 0;
    });

    return bookmarks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BookmarksPane::GetBookmark
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DebuggerViewSnapshot::WatchLine> BookmarksPane::GetBookmark (int row) const
{
    if (row < 0 || row >= (int) m_bookmarks.size())
    {
        return std::nullopt;
    }

    return m_bookmarks[(size_t) row];
}





////////////////////////////////////////////////////////////////////////////////
//
//  BookmarksPane::AddMenuItems
//
////////////////////////////////////////////////////////////////////////////////

bool BookmarksPane::AddMenuItems (DxuiListView * list, int row, PaneMenuItems & items)
{
    std::optional<DebuggerViewSnapshot::WatchLine>  bookmark = GetBookmark (row);



    if (list != m_list)
    {
        return false;
    }

    if (bookmark.has_value())
    {
        items.push_back ({ L"Go to",  [this, at = bookmark->address] { m_host.GoToCode (at); } });
        items.push_back ({ L"Delete", [this, rows = GetActedRows (row)] { DeleteRows (rows); } });
    }

    AddCopyItem (items);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BookmarksPane::RouteEditKey
//
////////////////////////////////////////////////////////////////////////////////

bool BookmarksPane::RouteEditKey (const DxuiKeyEvent & ev, IDxuiControl * focused)
{
    if (focused != m_list || ev.kind != DxuiKeyEventKind::Down || ev.ctrl || ev.alt || ev.vk != VK_DELETE)
    {
        return false;
    }

    DeleteRows (GetActedRows (-1));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BookmarksPane::DeleteRows
//
////////////////////////////////////////////////////////////////////////////////

void BookmarksPane::DeleteRows (const std::vector<int> & rows)
{
    std::vector<int>  ids;



    for (int row : rows)
    {
        std::optional<DebuggerViewSnapshot::WatchLine>  bookmark = GetBookmark (row);

        if (bookmark.has_value())
        {
            ids.push_back (bookmark->id);
        }
    }

    std::ranges::sort (ids, std::greater<>());

    for (int id : ids)
    {
        m_host.RunAction (DebuggerActions::GetClearBookmark (id, m_host.GetMode()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BookmarksPane::GetCells
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::vector<DxuiListView::Cell>> BookmarksPane::GetCells (const std::vector<DebuggerViewSnapshot::WatchLine> & bookmarks,
                                                                     const DebuggerViewSnapshot::SymbolsState      * symbols)
{
    std::vector<std::vector<DxuiListView::Cell>>  cells;



    for (const DebuggerViewSnapshot::WatchLine & bookmark : bookmarks)
    {
        cells.push_back ({ { std::format (L"#{}", bookmark.id) }, { std::format (L"${:04X}", bookmark.address) },
                           { TextEncoding::NarrowToWide (FindSymbolName (symbols, bookmark.address)) } });
    }

    if (cells.empty())
    {
        cells.push_back (MakeNote (L"No bookmarks. Add one from a disassembly row's menu, or with Ctrl+K, K."));
    }

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BookmarksPane::GetNext
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> BookmarksPane::GetNext (const std::vector<DebuggerViewSnapshot::WatchLine> & bookmarks, Word from, bool isBackward)
{
    std::vector<Word>  addresses;



    for (const DebuggerViewSnapshot::WatchLine & bookmark : bookmarks)
    {
        addresses.push_back (bookmark.address);
    }

    if (addresses.empty())
    {
        return std::nullopt;
    }

    std::ranges::sort (addresses);

    if (isBackward)
    {
        auto  before = std::ranges::find_if (addresses.rbegin(), addresses.rend(), [from] (Word each) { return each < from; });



        return (before != addresses.rend()) ? *before : addresses.back();
    }

    auto  after = std::ranges::find_if (addresses, [from] (Word each) { return each > from; });



    return (after != addresses.end()) ? *after : addresses.front();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlocksPane::DataBlocksPane
//
////////////////////////////////////////////////////////////////////////////////

DataBlocksPane::DataBlocksPane (IDebuggerPaneHost & host) :
    ToolListPane (host, DebuggerLayout::kDataBlocks, L"Data ranges"),
    m_filters    (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlocksPane::Configure
//
////////////////////////////////////////////////////////////////////////////////

void DataBlocksPane::Configure()
{
    m_list->SetColumns ({ { L"Name",  0, false, DxuiTextHAlign::Left },
                          { L"Range", 0, false, DxuiTextHAlign::Left },
                          { L"Kind",  0, true,  DxuiTextHAlign::Left } });

    m_list->SetMultiSelect           (true);
    m_list->SetActivateOnDoubleClick (true);
    m_list->SetOnSortColumn          ([this] (int column) { SortBy (column); });

    m_filters.Attach (m_list, { kKindColumn }, [this]
    {
        if (m_host.GetLatestSnapshot() != nullptr)
        {
            Apply (*m_host.GetLatestSnapshot());
        }
    });

    m_list->SetOnActivateRow ([this] (int row)
    {
        std::optional<DataBlockEntry>  block = GetBlock (row);



        if (block.has_value())
        {
            m_host.GoToCode (block->first);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlocksPane::Apply
//
////////////////////////////////////////////////////////////////////////////////

void DataBlocksPane::Apply (const DebuggerViewSnapshot & snapshot)
{
    std::vector<std::wstring>  kinds;



    for (const DataBlockEntry & block : snapshot.dataBlocks)
    {
        kinds.push_back (GetKindText (block.kind));
    }

    m_filters.SetValues (kKindColumn, kinds);

    m_blocks = GetSorted (snapshot.dataBlocks, m_sort, m_isReverse);
    std::erase_if (m_blocks, [this] (const DataBlockEntry & block) { return !m_filters.IsShown (kKindColumn, GetKindText (block.kind)); });

    if (m_blocks.empty() && !snapshot.dataBlocks.empty())
    {
        m_list->SetRows ({ MakeNote (L"No data ranges match the filter.") });
    }
    else
    {
        m_list->SetRows (GetCells (m_blocks));
    }

    ShowNewListRow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlocksPane::SortBy
//
//  A click on a heading sorts by that column, and a second click on the
//  same heading reverses the order. The selected block is selected in its
//  new row.
//
////////////////////////////////////////////////////////////////////////////////

void DataBlocksPane::SortBy (int column)
{
    const DebuggerViewSnapshot     * snapshot = m_host.GetLatestSnapshot();
    std::optional<DataBlockEntry>    selected = GetBlock (m_list->GetSelectedRow());



    m_isReverse = (column == m_sort) && !m_isReverse;
    m_sort      = column;
    m_list->SetSortIndicator (m_sort, m_isReverse);

    if (snapshot == nullptr)
    {
        return;
    }

    Apply (*snapshot);

    for (size_t row = 0; selected.has_value() && row < m_blocks.size(); row++)
    {
        if (m_blocks[row].first == selected->first)
        {
            m_list->SetSelectedRow ((int) row);
        }
    }

    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlocksPane::GetSorted
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DataBlockEntry> DataBlocksPane::GetSorted (std::vector<DataBlockEntry> blocks, int column, bool isReverse)
{
    if (column < 0 || column > 2)
    {
        return blocks;
    }

    std::ranges::stable_sort (blocks, [column, isReverse] (const DataBlockEntry & left, const DataBlockEntry & right)
    {
        int  order = (column == 0) ? _stricmp (left.name.c_str(), right.name.c_str())
                   : (column == 1) ? (int) left.first - (int) right.first
                   :                 _wcsicmp (GetKindText (left.kind), GetKindText (right.kind));



        return isReverse ? order > 0 : order < 0;
    });

    return blocks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlocksPane::GetKindText
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DataBlocksPane::GetKindText (DataBlockKind kind)
{
    static constexpr const wchar_t *  s_kKinds[] = { L"Bytes", L"Words", L"Addresses", L"Text", L"Floats" };



    return s_kKinds[(size_t) kind];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlocksPane::GetBlock
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DataBlockEntry> DataBlocksPane::GetBlock (int row) const
{
    if (row < 0 || row >= (int) m_blocks.size())
    {
        return std::nullopt;
    }

    return m_blocks[(size_t) row];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlocksPane::AddMenuItems
//
////////////////////////////////////////////////////////////////////////////////

bool DataBlocksPane::AddMenuItems (DxuiListView * list, int row, PaneMenuItems & items)
{
    std::optional<DataBlockEntry>  block = GetBlock (row);



    if (list != m_list)
    {
        return false;
    }

    if (block.has_value())
    {
        items.push_back ({ L"Go to",     [this, at = block->first] { m_host.GoToCode (at); } });
        items.push_back ({ L"Rename...", [this, renamed = *block] { Rename (renamed); } });
        items.push_back ({ L"Delete",    [this, rows = GetActedRows (row)] { DeleteRows (rows); } });
    }

    AddCopyItem (items);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlocksPane::RouteEditKey
//
//  Delete removes the selected ranges, and F2 renames the one selected.
//
////////////////////////////////////////////////////////////////////////////////

bool DataBlocksPane::RouteEditKey (const DxuiKeyEvent & ev, IDxuiControl * focused)
{
    std::optional<DataBlockEntry>  block;



    if (focused != m_list || ev.kind != DxuiKeyEventKind::Down || ev.ctrl || ev.alt || (ev.vk != VK_DELETE && ev.vk != VK_F2))
    {
        return false;
    }

    if (ev.vk == VK_DELETE)
    {
        DeleteRows (GetActedRows (-1));
        return true;
    }

    block = GetBlock (m_list->GetSelectedRow());

    if (block.has_value())
    {
        Rename (*block);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlocksPane::Rename
//
//  The range is marked again, as the kind it is, under the name typed.
//
////////////////////////////////////////////////////////////////////////////////

void DataBlocksPane::Rename (const DataBlockEntry & block)
{
    std::optional<std::wstring>  typed = m_host.RunTextDialog (L"Rename data range", L"Name", TextEncoding::NarrowToWide (block.name), [] (const std::wstring & text, std::wstring & error)
    {
        error = L"Type a name of letters, digits, _ and ., starting with a letter.";
        return !text.empty() && iswalpha (text[0]) && std::ranges::all_of (text, [] (wchar_t ch) { return iswalnum (ch) || ch == L'_' || ch == L'.'; });
    });



    if (typed.has_value() && TextEncoding::WideToNarrow (*typed) != block.name)
    {
        m_host.RunAction (DebuggerActions::GetMarkData (GetDirective (block), block.first, block.last, m_host.GetMode(), TextEncoding::WideToNarrow (*typed)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlocksPane::GetDirective
//
//  The directive that marks a range of the block's kind, a digit after DB
//  or DW for how many to a line.
//
////////////////////////////////////////////////////////////////////////////////

std::string DataBlocksPane::GetDirective (const DataBlockEntry & block)
{
    constexpr int  kMaxPerLineDigit = 9;
    bool           hasDigit         = block.perLine >= 1 && block.perLine <= kMaxPerLineDigit;



    switch (block.kind)
    {
    case DataBlockKind::Text:    return "ASC";
    case DataBlockKind::Address: return "DA";
    case DataBlockKind::Float:   return "DF";
    case DataBlockKind::Words:   return hasDigit ? std::format ("DW{}", block.perLine) : std::string ("DW");
    default:                     return hasDigit ? std::format ("DB{}", block.perLine) : std::string ("DB");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlocksPane::DeleteRows
//
////////////////////////////////////////////////////////////////////////////////

void DataBlocksPane::DeleteRows (const std::vector<int> & rows)
{
    std::vector<DataBlockEntry>  blocks;



    for (int row : rows)
    {
        std::optional<DataBlockEntry>  block = GetBlock (row);

        if (block.has_value())
        {
            blocks.push_back (*block);
        }
    }

    for (const DataBlockEntry & block : blocks)
    {
        m_host.RunAction (DebuggerActions::GetClearData (block.first, block.last, m_host.GetMode()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlocksPane::GetCells
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::vector<DxuiListView::Cell>> DataBlocksPane::GetCells (const std::vector<DataBlockEntry> & blocks)
{
    std::vector<std::vector<DxuiListView::Cell>>  cells;



    for (const DataBlockEntry & block : blocks)
    {
        cells.push_back ({ { TextEncoding::NarrowToWide (block.name) }, { std::format (L"${:04X}-${:04X}", block.first, block.last) },
                           { GetKindText (block.kind) } });
    }

    if (cells.empty())
    {
        cells.push_back (MakeNote (L"No data ranges. Mark one from a disassembly row's menu."));
    }

    return cells;
}
