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

SymbolsPane::SymbolsPane (IDebuggerPaneHost & host) :
    ToolListPane (host, DebuggerLayout::kSymbols, L"Symbols")
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



        if (each != nullptr && !each->isHeading)
        {
            m_host.GoToCode (each->address);
        }
    });

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
    m_rows    = GetRows (*m_symbols);
    m_list->SetRows (GetCells (*m_symbols, m_rows));
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

    if (each != nullptr)
    {
        items.push_back ({ L"Clear table",   [this, table = each->table] { m_host.RunAction (DebuggerActions::GetClearSymbols (SymbolTable::ToUpper (table), m_host.GetMode())); } });
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
//  Each table's heading, then its symbols in the order the table has them.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<SymbolsPane::Row> SymbolsPane::GetRows (const DebuggerViewSnapshot::SymbolsState & symbols)
{
    std::vector<Row>  rows;



    for (const DebuggerViewSnapshot::SymbolTableRow & table : symbols.tables)
    {
        rows.push_back ({ table.name, std::string(), 0, true });

        for (const SymbolInfo & symbol : symbols.symbols)
        {
            if (symbol.table == table.id)
            {
                rows.push_back ({ table.name, symbol.name, symbol.address, false });
            }
        }
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolsPane::GetCells
//
//  A heading has the table's check, its name and its count; a constant's
//  value is marked as one, since it is not an address.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::vector<DxuiListView::Cell>> SymbolsPane::GetCells (const DebuggerViewSnapshot::SymbolsState & symbols, const std::vector<Row> & rows)
{
    std::vector<std::vector<DxuiListView::Cell>>  cells;
    size_t                                        index = 0;



    for (const Row & row : rows)
    {
        if (row.isHeading)
        {
            auto  table = std::ranges::find_if (symbols.tables, [&row] (const DebuggerViewSnapshot::SymbolTableRow & each) { return each.name == row.table; });
            DxuiListView::Cell  name  = { TextEncoding::NarrowToWide (std::format ("{} ({})", row.table, table->count)) };



            name.check = table->enabled;
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
