#include "Pch.h"

#include "Ui/Debugger/Panes/ToolPanes.h"

#include "Core/TextEncoding.h"
#include "Debugger/SymbolTable.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerMenuCommand.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/FlameGraphView.h"
#include "Ui/Debugger/Panes/ProfileProgress.h"
#include "Ui/Debugger/Panes/ProfileTimeline.h"
#include "Ui/Debugger/ToolbarLabelEntry.h"





//  The profiler bar's entries.
static constexpr int  s_kProfileReset  = 2;
static constexpr int  s_kProfileSave   = 3;
static constexpr int  s_kProfileView   = 4;
static constexpr int  s_kProfilePeriod = 5;





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
//  ProfilerPane::ProfilerPane
//
////////////////////////////////////////////////////////////////////////////////

ProfilerPane::ProfilerPane (IDebuggerPaneHost & host) :
    ToolListPane (host, DebuggerLayout::kProfiler, L"Profiler"),
    m_bar        (host, DebuggerLayout::kProfiler, L"Profiler commands")
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::~ProfilerPane
//
////////////////////////////////////////////////////////////////////////////////

ProfilerPane::~ProfilerPane() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::CreateControls
//
//  The list, the timeline and the flame graph, then the bar, which paints over
//  them.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::CreateControls()
{
    ToolListPane::CreateControls();

    m_timeline   = m_host.GetControlParent().CreateChild<ProfileTimeline>();
    m_progress   = m_host.GetControlParent().CreateChild<ProfileProgress>();
    m_flameGraph = m_host.GetControlParent().CreateChild<FlameGraphView>();
    m_bar.Create();

    m_flameGraph->SetOnActivate ([this] (Word address) { m_host.GoToCode (address); });
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
        std::optional<Word>  address = GetRowAddress ((m_view == View::Addresses) ? m_byAddress : m_byOpcode, m_view, row);



        if (address.has_value())
        {
            m_host.GoToCode (*address);
        }
    });

    SetView (View::Opcodes);

    m_bar.Configure();
    m_bar.SetOnPress ([this] { SetViewMenu(); });
    SetBarEntries();
    SetViewMenu();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::AddToDock
//
//  Its bar, in a place the bar is laid out over, over its timeline, over its
//  list or, in the flame graph view, its flame graph.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::AddToDock (DxuiDockSite & dock)
{
    auto  isFlame = [this] { return m_view == View::Flame; };



    m_frame = std::make_unique<DebuggerPaneFrame> (m_title);
    m_frame->AddPart (m_bar.GetSlot(), PaneBarSlot::GetHeightPx);
    m_frame->AddPart (m_progress,      [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (ProfileProgress::kHeightDip); },
                      [this] { return m_isBuilding; });
    m_frame->AddPart (m_timeline,      [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (ProfileTimeline::kHeightDip); });
    m_frame->SetPartInsetDip (m_progress, 4);
    m_frame->AddPart (m_list,          nullptr, [isFlame] { return !isFlame(); });
    m_frame->AddPart (m_flameGraph,    nullptr, isFlame);
    m_frame->SetPartInsetDip (m_timeline,   4);
    m_frame->SetPartInsetDip (m_flameGraph, 4);

    dock.AddPane (m_id, m_title, m_frame.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::GetControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> ProfilerPane::GetControls() const
{
    return { m_bar.GetSlot(), m_progress, m_timeline, m_list, m_flameGraph, m_bar.GetBar() };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::SetView
//
//  The flame graph view has no list, so the columns are left as they are.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::SetView (View view)
{
    m_view = view;

    //  The flame graph and the list take turns in the pane, so it is laid
    //  out again.
    if (m_frame != nullptr)
    {
        m_frame->Relayout();
        m_host.Repaint();
    }

    switch (view)
    {
    case View::Addresses:
        m_list->SetColumns ({ { L"Address", 0, false, DxuiTextHAlign::Left },
                              { L"Symbol",  0, false, DxuiTextHAlign::Left },
                              { L"Cycles",  0, true,  DxuiTextHAlign::Right } });
        break;

    case View::Routines:
        m_list->SetColumns ({ { L"Routine",  0, false, DxuiTextHAlign::Left },
                              { L"Self",     0, false, DxuiTextHAlign::Right },
                              { L"Percent",  0, false, DxuiTextHAlign::Right },
                              { L"Total",    0, false, DxuiTextHAlign::Right },
                              { L"Percent",  0, false, DxuiTextHAlign::Right },
                              { L"Calls",    0, false, DxuiTextHAlign::Right },
                              { L"Per call", 0, true,  DxuiTextHAlign::Right } });
        break;

    case View::Tree:
        m_list->SetColumns ({ { L"Routine", 0, false, DxuiTextHAlign::Left },
                              { L"Self",    0, false, DxuiTextHAlign::Right },
                              { L"Total",   0, false, DxuiTextHAlign::Right },
                              { L"Percent", 0, false, DxuiTextHAlign::Right },
                              { L"Calls",   0, true,  DxuiTextHAlign::Right } });
        break;

    case View::Flame:
        break;

    default:
        m_list->SetColumns ({ { L"Opcode", 0, false, DxuiTextHAlign::Left },
                              { L"Mode",   0, false, DxuiTextHAlign::Left },
                              { L"Count",  0, false, DxuiTextHAlign::Right },
                              { L"Cycles", 0, true,  DxuiTextHAlign::Right } });
        break;
    }

    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::GetViewLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ProfilerPane::GetViewLabel (View view)
{
    switch (view)
    {
    case View::Addresses: return L"Addresses";
    case View::Routines:  return L"Routines";
    case View::Tree:      return L"Call tree";
    case View::Flame:     return L"Flame graph";
    default:              return L"Opcodes";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::GetRowAddress
//
//  The first row has the totals; each row after it is one entry.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> ProfilerPane::GetRowAddress (const std::optional<ProfileData> & profile, View view, int row)
{
    size_t                       entry   = (size_t) (row - 1);
    const ProfileRoutineEntry  * routine = nullptr;



    if (!profile.has_value() || row < 1)
    {
        return std::nullopt;
    }

    if (view == View::Addresses)
    {
        return (entry < profile->addresses.size()) ? std::optional<Word> (profile->addresses[entry].address) : std::nullopt;
    }

    if (view == View::Routines && entry < profile->routines.size())
    {
        routine = &profile->routines[entry];
    }
    else if (view == View::Tree && entry < profile->tree.size())
    {
        routine = &profile->routines[profile->tree[entry].routine];
    }

    if (routine == nullptr || routine->kind == RoutineKind::Top)
    {
        return std::nullopt;
    }

    return routine->address;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::Apply
//
//  A snapshot of a running machine has no profile; the last one read stays.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::Apply (const DebuggerViewSnapshot & snapshot)
{
    //  The progress bar comes and goes with a build, so the pane is laid out
    //  again.
    if (snapshot.profileProgress.has_value() != m_isBuilding)
    {
        m_isBuilding = snapshot.profileProgress.has_value();

        if (m_frame != nullptr)
        {
            m_frame->Relayout();
        }
    }

    m_progress->SetShare (snapshot.profileProgress.value_or (0.0));

    if (snapshot.profile.has_value() || snapshot.isProfiling != m_isOn)
    {
        m_isOn = snapshot.isProfiling;

        if (snapshot.profile.has_value())
        {
            m_byOpcode  = snapshot.profile;
            m_byAddress = snapshot.profileByAddress;
        }

        Refresh();
    }

    m_timeline->SetData   (m_byOpcode);
    m_flameGraph->SetData (m_byOpcode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::Refresh
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::Refresh()
{
    m_list->SetRows (GetCells ((m_view == View::Addresses) ? m_byAddress : m_byOpcode, m_view, m_isOn));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::RunProfile
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::RunProfile (const char * words)
{
    m_host.RunAction (DebuggerActions::GetProfile (words, m_host.GetMode()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::Save
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::Save()
{
    std::wstring  path = PickSaveFile (L"Text files", L"*.txt", L"txt", L"Profile.txt");



    if (!path.empty())
    {
        RunProfile (std::format ("SAVE \"{}\"", TextEncoding::WideToNarrow (path)).c_str());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::SetBarEntries
//
//  Profile turns profiling on and off, Reset starts the counts over, Save...
//  writes them to a text file, and the view drop-down chooses opcodes,
//  addresses, routines, the call tree or the flame graph.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::SetBarEntries()
{
    std::vector<DxuiToolbar::Entry>  entries;
    auto                             add = [this, &entries] (int id, const wchar_t * label, const wchar_t * tip, DxuiToolbar::Kind kind, int group, std::function<void()> dispatch)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        DxuiToolbar::Entry            entry;

        command->id       = id;
        command->label    = label;
        command->tip      = tip;
        command->dispatch = std::move (dispatch);

        entry.command = command;
        entry.kind    = kind;
        entry.group   = group;

        m_commands.push_back (command);
        entries.push_back (std::move (entry));

        return command;
    };
    std::shared_ptr<DxuiCommand>     command;



    m_commands.clear();

    m_showLabel = std::make_unique<ToolbarLabelEntry> (L"Show profile:");

    {
        DxuiToolbar::Entry  label;

        label.command       = m_showLabel->GetCommand();
        label.custom        = m_showLabel.get();
        label.neverOverflow = true;
        entries.push_back (std::move (label));
    }

    command            = add (s_kProfilePeriod, L"Last 60 s", L"Build the profile of a period of history", DxuiToolbar::Kind::DropDown, 0, [] {});
    command->labelText = [this] { return std::wstring (GetPeriods()[m_period].label); };

    add (s_kProfileReset, L"Reset",   L"Start the counts over",            DxuiToolbar::Kind::Command, 0, [this] { RunProfile ("RESET"); });
    add (s_kProfileSave,  L"Save...", L"Save the profile to a text file", DxuiToolbar::Kind::Command, 0, [this] { Save(); });

    command            = add (s_kProfileView, L"Opcodes", L"Choose the profile's view: opcodes, addresses, routines, the call tree or a flame graph", DxuiToolbar::Kind::DropDown, 1, [] {});
    command->labelText = [this] { return std::wstring (GetViewLabel (m_view)); };

    m_bar.GetBar()->SetEntries (std::move (entries));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::SetViewMenu
//
//  Built again before the drop-down opens, since the rows have the checks
//  they were built with.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::SetViewMenu()
{
    std::vector<DxuiPopupMenuItem>  views;



    for (int view = 0; view < kViewCount; view++)
    {
        views.push_back (DxuiPopupMenuItem::ForCommand (DebuggerMenuCommand::Make (GetViewLabel ((View) view), (int) m_view == view,
                                                                                  [this, view] { SetView ((View) view); })));
    }

    m_bar.GetBar()->SetDropDownItems (s_kProfileView, std::move (views));

    views.clear();

    for (size_t period = 0; period < GetPeriods().size(); period++)
    {
        views.push_back (DxuiPopupMenuItem::ForCommand (DebuggerMenuCommand::Make (GetPeriods()[period].label, m_period == period,
                                                                                  [this, period] { ShowPeriod (period); })));
    }

    m_bar.GetBar()->SetDropDownItems (s_kProfilePeriod, std::move (views));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::GetPeriods
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<ProfilerPane::Period> & ProfilerPane::GetPeriods()
{
    static const std::vector<Period>  periods =
    {
        { "HISTORY 10",  L"Last 10 s"           },
        { "HISTORY 60",  L"Last 60 s"           },
        { "HISTORY 300", L"Last 5 min"          },
        { "HISTORY ALL", L"Since history start" },
    };



    return periods;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::ShowPeriod
//
//  The profile of the period, built from history in the background; the
//  progress bar is up until it is in.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::ShowPeriod (size_t period)
{
    m_period = std::min (period, GetPeriods().size() - 1);
    RunProfile (GetPeriods()[m_period].words);
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::RouteContentMouse
//
//  The flame graph takes a click on a bar, to zoom or to go to its routine.
//
////////////////////////////////////////////////////////////////////////////////

bool ProfilerPane::RouteContentMouse (const DxuiMouseEvent & ev)
{
    bool  isUsed = false;



    if (!m_host.IsRoutable (m_flameGraph) || !m_flameGraph->IsVisible() || !DxuiDockSite::Contains (m_flameGraph->GetBounds(), ev.positionDip))
    {
        return false;
    }

    isUsed = m_flameGraph->OnMouse (ev);

    if (isUsed)
    {
        m_host.Repaint();
    }

    return isUsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::TryGetTip
//
//  Over the timeline, the frame there; over the flame graph, the routine;
//  each at once and following the pointer.
//
////////////////////////////////////////////////////////////////////////////////

bool ProfilerPane::TryGetTip (POINT clientPx, PaneTip & tip)
{
    tip.kind = PaneTip::Kind::Pointer;

    if (m_host.IsRoutable (m_timeline) && m_timeline->TryGetTipAt (clientPx, tip.text))
    {
        return true;
    }

    return m_host.IsRoutable (m_flameGraph) && m_flameGraph->TryGetTipAt (clientPx, tip.text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::AddMenuItems
//
////////////////////////////////////////////////////////////////////////////////

bool ProfilerPane::AddMenuItems (DxuiListView * list, int, PaneMenuItems & items)
{
    if (list != m_list)
    {
        return false;
    }

    items.push_back ({ m_isOn ? L"Stop profiling" : L"Start profiling", [this, on = m_isOn] { RunProfile (on ? "OFF" : "ON"); } });
    items.push_back ({ L"Reset",   [this] { RunProfile ("RESET"); } });
    items.push_back ({ L"Save...", [this] { Save(); } });
    AddCopyItem (items);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::GetCells
//
//  The totals across the first row, then a row an opcode, an address, a
//  routine or a node of the call tree.
//  With no profile read yet, a note: how to start profiling while it is
//  off, and that the machine must be paused while it is on.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::vector<DxuiListView::Cell>> ProfilerPane::GetCells (const std::optional<ProfileData> & profile, View view, bool isOn)
{
    std::vector<std::vector<DxuiListView::Cell>>  cells;
    auto                                          share = [&profile] (uint64_t cycles)
    {
        return std::format (L"{:.1f}%", 100.0 * (double) cycles / (double) std::max<uint64_t> (profile->cycles, 1));
    };



    if (!profile.has_value())
    {
        cells.push_back (MakeNote (isOn ? L"Pause the machine to read the profile."
                                        : L"Choose a period in Show profile to build a profile from history."));
        return cells;
    }

    cells.push_back (MakeNote (std::format (L"{} instructions, {} cycles, {} page crossings, {} branches taken.",
                                            profile->instructions, profile->cycles, profile->pageCross, profile->branchTaken)));

    if (view == View::Addresses)
    {
        for (const ProfileAddressEntry & entry : profile->addresses)
        {
            cells.push_back ({ { std::format (L"${:04X}", entry.address) }, { TextEncoding::NarrowToWide (entry.symbol) }, { std::to_wstring (entry.cycles) } });
        }
    }
    else if (view == View::Routines)
    {
        for (const ProfileRoutineEntry & entry : profile->routines)
        {
            cells.push_back ({ { TextEncoding::NarrowToWide (entry.GetName()) }, { std::to_wstring (entry.self) }, { share (entry.self) },
                               { std::to_wstring (entry.total) }, { share (entry.total) }, { std::to_wstring (entry.calls) },
                               { (entry.calls != 0) ? std::to_wstring (entry.total / entry.calls) : std::wstring (L"-") } });
        }
    }
    else if (view == View::Tree)
    {
        for (const ProfileTreeEntry & entry : profile->tree)
        {
            cells.push_back ({ { std::wstring ((size_t) entry.depth * 2, L' ') + TextEncoding::NarrowToWide (profile->routines[entry.routine].GetName()) },
                               { std::to_wstring (entry.self) }, { std::to_wstring (entry.total) }, { share (entry.total) }, { std::to_wstring (entry.calls) } });
        }
    }
    else if (view == View::Opcodes)
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
