#include "Pch.h"

#include "Ui/Debugger/Panes/ToolPanes.h"

#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MakeNote
//
//  One muted cell across the row.
//
////////////////////////////////////////////////////////////////////////////////

static std::vector<DxuiListView::Cell> MakeNote (const std::wstring & text)
{
    std::vector<DxuiListView::Cell>  cells (1, DxuiListView::Cell { text, true });



    cells[0].spansRow = true;
    return cells;
}





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
//  SymbolsPane::SymbolsPane
//
////////////////////////////////////////////////////////////////////////////////

SymbolsPane::SymbolsPane (DxuiListView * list, ShowFn show, EnableFn enable) :
    m_list   (list),
    m_show   (std::move (show)),
    m_enable (std::move (enable))
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
            m_show (each->address);
        }
    });

    m_list->SetOnCheckToggled ([this] (int row, size_t, bool checked)
    {
        const Row  * each = GetRow (row);



        if (each != nullptr && each->isHeading)
        {
            m_enable (each->table, checked);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolsPane::Apply
//
////////////////////////////////////////////////////////////////////////////////

void SymbolsPane::Apply (const std::shared_ptr<const DebuggerViewSnapshot::SymbolsState> & symbols)
{
    if (symbols == nullptr || symbols == m_symbols)
    {
        return;
    }

    m_symbols = symbols;
    m_rows    = GetRows (*symbols);
    m_list->SetRows (GetCells (*symbols, m_rows));
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

BookmarksPane::BookmarksPane (DxuiListView * list, ShowFn show) :
    m_list (list),
    m_show (std::move (show))
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
            m_show (bookmark->address);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BookmarksPane::Apply
//
////////////////////////////////////////////////////////////////////////////////

void BookmarksPane::Apply (const std::vector<DebuggerViewSnapshot::WatchLine> & bookmarks, const DebuggerViewSnapshot::SymbolsState * symbols)
{
    m_bookmarks = bookmarks;
    m_list->SetRows (GetCells (bookmarks, symbols));
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
//  ProfilerPane::ProfilerPane
//
////////////////////////////////////////////////////////////////////////////////

ProfilerPane::ProfilerPane (DxuiListView * list, ShowFn show) :
    m_list (list),
    m_show (std::move (show))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::Configure
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::Configure()
{
    m_list->SetActivateOnDoubleClick (true);

    m_list->SetOnActivateRow ([this] (int row)
    {
        //  The first row has the totals; an address row after it, its address.
        if (m_isByAddress && m_byAddress.has_value() && row >= 1 && row <= (int) m_byAddress->addresses.size())
        {
            m_show (m_byAddress->addresses[(size_t) (row - 1)].address);
        }
    });

    SetByAddress (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::SetByAddress
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::SetByAddress (bool isByAddress)
{
    m_isByAddress = isByAddress;

    if (isByAddress)
    {
        m_list->SetColumns ({ { L"Address", 0, false, DxuiTextHAlign::Left },
                              { L"Symbol",  0, false, DxuiTextHAlign::Left },
                              { L"Cycles",  0, true,  DxuiTextHAlign::Right } });
    }
    else
    {
        m_list->SetColumns ({ { L"Opcode", 0, false, DxuiTextHAlign::Left },
                              { L"Mode",   0, false, DxuiTextHAlign::Left },
                              { L"Count",  0, false, DxuiTextHAlign::Right },
                              { L"Cycles", 0, true,  DxuiTextHAlign::Right } });
    }

    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::Apply
//
//  A snapshot of a running machine has no profile; the last one read stays.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::Apply (const std::optional<ProfileData> & byOpcode, const std::optional<ProfileData> & byAddress)
{
    if (!byOpcode.has_value())
    {
        return;
    }

    m_byOpcode  = byOpcode;
    m_byAddress = byAddress;
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::Refresh
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::Refresh()
{
    m_list->SetRows (GetCells (m_isByAddress ? m_byAddress : m_byOpcode, m_isByAddress));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::GetCells
//
//  The totals across the first row, then a row an opcode or an address.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::vector<DxuiListView::Cell>> ProfilerPane::GetCells (const std::optional<ProfileData> & profile, bool isByAddress)
{
    std::vector<std::vector<DxuiListView::Cell>>  cells;



    if (!profile.has_value())
    {
        cells.push_back (MakeNote (L"Pause the machine to read the profile."));
        return cells;
    }

    cells.push_back (MakeNote (std::format (L"Profiling {}: {} instructions, {} cycles, {} page crossings, {} branches taken.",
                                            profile->isOn ? L"on" : L"off", profile->instructions, profile->cycles,
                                            profile->pageCross, profile->branchTaken)));

    if (isByAddress)
    {
        for (const ProfileAddressEntry & entry : profile->addresses)
        {
            cells.push_back ({ { std::format (L"${:04X}", entry.address) }, { TextEncoding::NarrowToWide (entry.symbol) }, { std::to_wstring (entry.cycles) } });
        }
    }
    else
    {
        for (const ProfileEntry & entry : profile->opcodes)
        {
            cells.push_back ({ { TextEncoding::NarrowToWide (entry.mnemonic) }, { TextEncoding::NarrowToWide (entry.mode) },
                               { std::to_wstring (entry.count) }, { std::to_wstring (entry.cycles) } });
        }
    }

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlocksPane::DataBlocksPane
//
////////////////////////////////////////////////////////////////////////////////

DataBlocksPane::DataBlocksPane (DxuiListView * list, ShowFn show) :
    m_list (list),
    m_show (std::move (show))
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
            m_show (block->first);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlocksPane::Apply
//
////////////////////////////////////////////////////////////////////////////////

void DataBlocksPane::Apply (const std::vector<DataBlockEntry> & blocks)
{
    m_blocks = blocks;
    m_list->SetRows (GetCells (blocks));
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
