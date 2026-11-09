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

    m_rows = GetRows (*m_symbols, m_kind, m_open);
    m_list->SetRows (GetCells (*m_symbols, m_rows, m_open));
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
//  its symbols in the order the table has them.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<SymbolsPane::Row> SymbolsPane::GetRows (const DebuggerViewSnapshot::SymbolsState & symbols, Kind kind, const std::set<std::string> & open)
{
    std::vector<Row>  rows;



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

        for (const SymbolInfo & symbol : symbols.symbols)
        {
            if (symbol.table == table.id && open.contains (table.name))
            {
                rows.push_back ({ table.name, symbol.name, symbol.address, false });
            }
        }
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
    size_t                                        index = 0;



    for (const Row & row : rows)
    {
        if (row.isHeading)
        {
            auto                table = std::ranges::find_if (symbols.tables, [&row] (const DebuggerViewSnapshot::SymbolTableRow & each) { return each.name == row.table; });
            std::string         file  = GetFileOf (symbols, row.table);
            DxuiListView::Cell  name  = { std::format (L"{} {} ({})", open.contains (row.table) ? s_kpszTriangleDown : s_kpszTriangleRight,
                                                       TextEncoding::NarrowToWide (file.empty() ? row.table : file), table->count) };



            name.check = DxuiListView::Cell::GetCheck (table->enabled);
            name.tip   = file.empty()          ? std::wstring()
                       : table->path.empty()   ? std::format (L"In the {} table", TextEncoding::NarrowToWide (row.table))
                       :                         std::format (L"{}\nIn the {} table", table->path, TextEncoding::NarrowToWide (row.table));
            cells.push_back ({ name, { L"" } });
            continue;
        }

        while (index < symbols.symbols.size() && (symbols.symbols[index].name != row.name || symbols.symbols[index].address != row.address))
        {
            index++;
        }

        bool  isConstant = index < symbols.symbols.size() && symbols.symbols[index].isConstant;



        cells.push_back ({ { L"   " + TextEncoding::NarrowToWide (row.name) },
                           { isConstant ? std::format (L"{:04X} (constant)", row.address) : std::format (L"${:04X}", row.address) } });
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

    m_list->SetActivateOnDoubleClick (true);

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
    m_bookmarks = snapshot.bookmarks;
    m_list->SetRows (GetCells (m_bookmarks, snapshot.symbols.get()));
    ShowNewListRow();
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
        items.push_back ({ L"Delete", [this, id = bookmark->id]      { m_host.RunAction (DebuggerActions::GetClearBookmark (id, m_host.GetMode())); } });
    }

    AddCopyItem (items);
    return true;
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
    ToolListPane (host, DebuggerLayout::kDataBlocks, L"Data ranges")
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

    m_list->SetActivateOnDoubleClick (true);

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
    m_blocks = snapshot.dataBlocks;
    m_list->SetRows (GetCells (m_blocks));
    ShowNewListRow();
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
        items.push_back ({ L"Go to",  [this, at = block->first] { m_host.GoToCode (at); } });
        items.push_back ({ L"Delete", [this, from = block->first, to = block->last] { m_host.RunAction (DebuggerActions::GetClearData (from, to, m_host.GetMode())); } });
    }

    AddCopyItem (items);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlocksPane::GetCells
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::vector<DxuiListView::Cell>> DataBlocksPane::GetCells (const std::vector<DataBlockEntry> & blocks)
{
    static constexpr const wchar_t *  kKinds[] = { L"Bytes", L"Words", L"Addresses", L"Text", L"Floats" };
    std::vector<std::vector<DxuiListView::Cell>>  cells;



    for (const DataBlockEntry & block : blocks)
    {
        cells.push_back ({ { TextEncoding::NarrowToWide (block.name) }, { std::format (L"${:04X}-${:04X}", block.first, block.last) },
                           { kKinds[(size_t) block.kind] } });
    }

    if (cells.empty())
    {
        cells.push_back (MakeNote (L"No data ranges. Mark one from a disassembly row's menu."));
    }

    return cells;
}
