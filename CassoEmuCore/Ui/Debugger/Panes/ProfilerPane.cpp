#include "Pch.h"

#include "Ui/Debugger/Panes/ProfilerPane.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Debugger/SymbolTable.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerMenuCommand.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/Panes/AddSymbolDialog.h"
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
//  The list, the timeline, the totals and the flame graph, then the progress
//  bar over them, then the bar, which paints over them all.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::CreateControls()
{
    ToolListPane::CreateControls();

    m_timeline   = m_host.GetControlParent().CreateChild<ProfileTimeline>();
    m_totals     = m_host.GetControlParent().CreateChild<DxuiLabel>();
    m_flameGraph = m_host.GetControlParent().CreateChild<FlameGraphView>();
    m_progress   = m_host.GetControlParent().CreateChild<ProfileProgress>();
    m_bar.Create();

    m_totals->SetTextRole  (DxuiTextRole::Muted);
    m_totals->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_flameGraph->SetOnActivate ([this] (Word address)    { m_host.GoToCode (address); });
    m_flameGraph->SetOnSelect   ([this] (size_t routine)  { m_selectedRoutine = routine; });
    m_flameGraph->SetOnZoom     ([this] (size_t node)
    {
        ProfileZoom::State  state = m_zoom.GetState();



        if (state.flameZoom != node)
        {
            state.flameZoom = node;
            m_zoom.Push (state);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::Configure
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::Configure()
{
    m_list->SetActivateOnDoubleClick (true);
    m_list->SetOnSortColumn          ([this] (int column) { SortBy (column); });

    m_list->SetOnActivateRow ([this] (int row)
    {
        int                  entry   = (row >= 0 && (size_t) row < m_order.size()) ? (int) m_order[(size_t) row] : row;
        std::optional<Word>  address = GetRowAddress ((m_view == View::Addresses) ? m_byAddress : m_byOpcode, m_view, entry);



        if (address.has_value())
        {
            m_host.GoToCode (*address);
        }
    });

    SetView (View::Routines);

    m_bar.Configure();
    m_bar.SetOnPress ([this] { SetViewMenu(); });
    SetBarEntries();
    SetViewMenu();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::AddToDock
//
//  Its bar, in a place the bar is laid out over, over its timeline, over the
//  totals line, over its list or, in the flame graph view, its flame graph.
//  The progress bar is drawn centered over the list or the flame graph while
//  a profile is built, and moves nothing.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::AddToDock (DxuiDockSite & dock)
{
    auto  isFlame = [this] { return m_view == View::Flame; };



    m_frame = std::make_unique<DebuggerPaneFrame> (m_title);
    m_frame->AddPart (m_bar.GetSlot(), PaneBarSlot::GetHeightPx);
    m_frame->AddPart (m_timeline,      [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (ProfileTimeline::kHeightDip); });
    m_frame->AddPart (m_totals,        [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (kTotalsHeightDip); },
                      [this] { return !m_totals->GetText().empty(); });
    m_frame->AddPart (m_list,          nullptr, [isFlame] { return !isFlame(); });
    m_frame->AddPart (m_flameGraph,    nullptr, isFlame);
    m_frame->AddOverlay (m_progress, kProgressWidthDip, [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (ProfileProgress::kHeightDip); },
                         [this] { return m_isBuilding; });
    m_frame->SetPartTextAligned (m_totals);
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
    return { m_bar.GetSlot(), m_timeline, m_totals, m_list, m_flameGraph, m_progress, m_bar.GetBar() };
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
                              { L"Cycles",  0, false, DxuiTextHAlign::Right } });
        break;

    case View::Routines:
        m_list->SetColumns ({ { L"Routine",  0, false, DxuiTextHAlign::Left },
                              { L"Self",     0, false, DxuiTextHAlign::Right },
                              { L"Percent",  0, false, DxuiTextHAlign::Right },
                              { L"Total",    0, false, DxuiTextHAlign::Right },
                              { L"Percent",  0, false, DxuiTextHAlign::Right },
                              { L"Calls",    0, false, DxuiTextHAlign::Right },
                              { L"Per call", 0, false, DxuiTextHAlign::Right } });
        break;

    case View::Tree:
        m_list->SetColumns ({ { L"Routine", 0, false, DxuiTextHAlign::Left },
                              { L"Self",    0, false, DxuiTextHAlign::Right },
                              { L"Total",   0, false, DxuiTextHAlign::Right },
                              { L"Percent", 0, false, DxuiTextHAlign::Right },
                              { L"Calls",   0, false, DxuiTextHAlign::Right } });
        break;

    case View::Flame:
        break;

    default:
        m_list->SetColumns ({ { L"Opcode", 0, false, DxuiTextHAlign::Left },
                              { L"Mode",   0, false, DxuiTextHAlign::Left },
                              { L"Count",  0, false, DxuiTextHAlign::Right },
                              { L"Cycles", 0, false, DxuiTextHAlign::Right } });
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
//  Each row is one entry.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> ProfilerPane::GetRowAddress (const std::optional<ProfileData> & profile, View view, int row)
{
    size_t                       entry   = (size_t) row;
    const ProfileRoutineEntry  * routine = nullptr;



    if (!profile.has_value() || row < 0)
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
//  ProfilerPane::GetDefaultSort
//
////////////////////////////////////////////////////////////////////////////////

ProfilerPane::Sort ProfilerPane::GetDefaultSort (View view)
{
    switch (view)
    {
    case View::Routines:  return { 3, true };
    case View::Tree:      return { 2, true };
    case View::Addresses: return { 2, true };
    case View::Opcodes:   return { 3, true };
    default:              return {};
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::IsTextColumn
//
//  A column of text sorts A first; a column of numbers, most first.
//
////////////////////////////////////////////////////////////////////////////////

bool ProfilerPane::IsTextColumn (View view, int column)
{
    switch (view)
    {
    case View::Addresses: return column == 1;
    case View::Opcodes:   return column <= 1;
    default:              return column == 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::GetSortKey
//
//  A share sorts as the count it is a share of. A routine never called,
//  Top level, has no cycles per call and sorts as zero.
//
////////////////////////////////////////////////////////////////////////////////

ProfilerPane::SortKey ProfilerPane::GetSortKey (
    const ProfileData  & profile,
    View                 view,
    size_t               entry,
    int                  column)
{
    SortKey  key;



    if (view == View::Routines)
    {
        const ProfileRoutineEntry  & routine = profile.routines[entry];

        switch (column)
        {
        case 0:  key.text   = SymbolTable::ToUpper (routine.GetName());                 break;
        case 1:
        case 2:  key.number = routine.self;                                             break;
        case 3:
        case 4:  key.number = routine.total;                                            break;
        case 5:  key.number = routine.calls;                                            break;
        default: key.number = (routine.calls != 0) ? routine.total / routine.calls : 0; break;
        }
    }
    else if (view == View::Tree)
    {
        const ProfileTreeEntry  & node = profile.tree[entry];

        switch (column)
        {
        case 0:  key.text   = SymbolTable::ToUpper (profile.routines[node.routine].GetName()); break;
        case 1:  key.number = node.self;                                                      break;
        case 4:  key.number = node.calls;                                                     break;
        default: key.number = node.total;                                                     break;
        }
    }
    else if (view == View::Addresses)
    {
        const ProfileAddressEntry  & address = profile.addresses[entry];

        key.number = (column == 0) ? address.address : address.cycles;
        key.text   = (column == 1) ? SymbolTable::ToUpper (address.symbol) : std::string();
    }
    else
    {
        const ProfileEntry  & opcode = profile.opcodes[entry];

        key.number = (column == 2) ? opcode.count : opcode.cycles;
        key.text   = (column == 0) ? opcode.mnemonic : (column == 1) ? opcode.mode : std::string();
    }

    return key;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::GetOrder
//
//  The call tree lists each node's callees after it, a level deeper, so a
//  node's caller is the last node before it a level up.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<size_t> ProfilerPane::GetOrder (
    const ProfileData  & profile,
    View                 view,
    int                  column,
    bool                 descending)
{
    std::vector<size_t>               order;
    std::vector<std::vector<size_t>>  children;
    std::vector<size_t>               roots;
    std::vector<size_t>               callers;
    size_t                            count    = 0;
    bool                              isText   = IsTextColumn (view, column);
    std::function<void (size_t)>      emit;
    auto                              isBefore = [&] (size_t a, size_t b)
    {
        SortKey  keyA = GetSortKey (profile, view, a, column);
        SortKey  keyB = GetSortKey (profile, view, b, column);



        if (isText)
        {
            return descending ? keyB.text < keyA.text : keyA.text < keyB.text;
        }

        return descending ? keyB.number < keyA.number : keyA.number < keyB.number;
    };



    switch (view)
    {
    case View::Routines:  count = profile.routines.size();  break;
    case View::Tree:      count = profile.tree.size();      break;
    case View::Addresses: count = profile.addresses.size(); break;
    case View::Opcodes:   count = profile.opcodes.size();   break;
    default:              return order;
    }

    if (view != View::Tree)
    {
        for (size_t i = 0; i < count; i++)
        {
            order.push_back (i);
        }

        std::stable_sort (order.begin(), order.end(), isBefore);
        return order;
    }

    children.resize (count);

    for (size_t i = 0; i < count; i++)
    {
        callers.resize (std::min (callers.size(), (size_t) std::max (profile.tree[i].depth, 0)));

        if (callers.empty())
        {
            roots.push_back (i);
        }
        else
        {
            children[callers.back()].push_back (i);
        }

        callers.push_back (i);
    }

    emit = [&] (size_t node)
    {
        order.push_back (node);
        std::stable_sort (children[node].begin(), children[node].end(), isBefore);

        for (size_t child : children[node])
        {
            emit (child);
        }
    };

    std::stable_sort (roots.begin(), roots.end(), isBefore);

    for (size_t root : roots)
    {
        emit (root);
    }

    return order;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::SortBy
//
//  A click on the sorted column turns its order around; a click on another
//  sorts by it, text A first and numbers most first.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::SortBy (int column)
{
    Sort  & sort = m_sorts[(size_t) m_view];



    sort.descending = (column == sort.column) ? !sort.descending : !IsTextColumn (m_view, column);
    sort.column     = column;

    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::Apply
//
//  A snapshot of a running machine has no profile; the last one read stays.
//  A paused one has the last profile built, which goes where the zoom
//  routes it.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::Apply (const DebuggerViewSnapshot & snapshot)
{
    ProfileZoom::Target  target = ProfileZoom::Target::None;



    //  The progress bar comes and goes with a build; it is laid out again
    //  over the list.
    if (snapshot.profileProgress.has_value() != m_isBuilding)
    {
        m_isBuilding = snapshot.profileProgress.has_value();

        if (m_frame != nullptr)
        {
            m_frame->Relayout();
        }
    }

    m_progress->SetShare (snapshot.profileProgress.value_or (0.0));

    if (snapshot.profile.has_value())
    {
        target = m_zoom.Route (snapshot.profile->historyRange, snapshot.profile->historyBuild);
        Take ({ snapshot.profile, snapshot.profileByAddress }, target);
    }

    if (target != ProfileZoom::Target::None || snapshot.isProfiling != m_isOn)
    {
        m_isOn = snapshot.isProfiling;
        Refresh();
    }

    m_isPaused  = snapshot.isPaused;
    m_stopCycle = snapshot.history.hasHistory ? std::optional<uint64_t> (snapshot.history.cycle) : std::nullopt;
    BuildIfDue();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::BuildIfDue
//
//  The chosen period's profile, built once for each stop while the pane is
//  open on a stopped machine with history, so the profile appears without the
//  period being chosen again.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::BuildIfDue()
{
    bool  isOpen = m_frame != nullptr && m_frame->IsVisible();



    if (!isOpen || !m_isPaused || m_isBuilding || !m_stopCycle.has_value() || m_builtCycle == m_stopCycle)
    {
        return;
    }

    ShowPeriod (m_period);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::Refresh
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::Refresh()
{
    const std::optional<ProfileData>              & profile  = (m_view == View::Addresses) ? m_byAddress : m_byOpcode;
    const Sort                                    & sort     = m_sorts[(size_t) m_view];
    bool                                            wasShown = !m_totals->GetText().empty();
    std::vector<std::vector<DxuiListView::Cell>>    cells    = GetCells (profile, m_view, m_isOn);
    std::vector<std::vector<DxuiListView::Cell>>    sorted;



    m_order.clear();

    if (profile.has_value() && m_view != View::Flame)
    {
        m_order = GetOrder (*profile, m_view, sort.column, sort.descending);
    }

    if (!m_order.empty() && m_order.size() == cells.size())
    {
        for (size_t entry : m_order)
        {
            sorted.push_back (std::move (cells[entry]));
        }

        cells = std::move (sorted);
    }

    m_list->SetSortIndicator (m_order.empty() ? -1 : sort.column, sort.descending);
    m_list->SetRows          (std::move (cells));

    //  The routine selected in the flame graph is selected once in the
    //  routines or the call tree, at its first node there, the next time
    //  the view is one of those; a row chosen in the list after that stays
    //  chosen.
    for (size_t row = 0; m_selectedRoutine.has_value() && profile.has_value() && row < m_order.size(); row++)
    {
        size_t  entry   = m_order[row];
        bool    isMatch = (m_view == View::Routines) ? entry == *m_selectedRoutine
                                                     : m_view == View::Tree && profile->tree[entry].routine == *m_selectedRoutine;

        if (isMatch)
        {
            m_list->SetSelectedRow ((int) row);
            m_selectedRoutine.reset();
            break;
        }
    }

    m_totals->SetText (GetTotalsText (m_byOpcode));

    if (wasShown != !m_totals->GetText().empty() && m_frame != nullptr)
    {
        m_frame->Relayout();
    }
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
//  The period drop-down builds the profile of a period of history, Reset
//  starts the counts over, Save writes them to a text file, and the view
//  drop-down chooses opcodes, addresses, routines, the call tree or the
//  flame graph. Reset and Save are glyphs.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::SetBarEntries()
{
    std::vector<DxuiToolbar::Entry>  entries;
    auto                             add = [this, &entries] (int id, const wchar_t * label, const wchar_t * tip, DxuiToolbar::Kind kind, int group, std::function<void()> dispatch,
                                                             const wchar_t * glyph = nullptr)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        DxuiToolbar::Entry            entry;

        command->id       = id;
        command->label    = label;
        command->tip      = tip;
        command->glyph    = glyph;
        command->dispatch = std::move (dispatch);

        entry.command  = command;
        entry.kind     = kind;
        entry.group    = group;
        entry.iconOnly = glyph != nullptr;

        m_commands.push_back (command);
        entries.push_back (std::move (entry));

        return command;
    };
    std::shared_ptr<DxuiCommand>     command;



    m_commands.clear();

    m_showLabel = std::make_unique<ToolbarLabelEntry> (L"Show:");

    {
        DxuiToolbar::Entry  label;

        label.command       = m_showLabel->GetCommand();
        label.custom        = m_showLabel.get();
        label.neverOverflow = true;
        entries.push_back (std::move (label));
    }

    command            = add (s_kProfilePeriod, L"Last 60 s", L"Build the profile of a period of history", DxuiToolbar::Kind::DropDown, 0, [] {});
    command->labelText = [this] { return std::wstring (GetPeriods()[m_period].label); };

    add (s_kProfileReset, L"Reset",   L"Reset: start the counts over",           DxuiToolbar::Kind::Command, 0, [this] { RunProfile ("RESET"); }, s_kpszMdl2Refresh);
    add (s_kProfileSave,  L"Save...", L"Save...: save the profile to a text file", DxuiToolbar::Kind::Command, 0, [this] { Save(); },            s_kpszMdl2Save);

    command            = add (s_kProfileView, L"Routines", L"Choose the profile's view: routines, the call tree, a flame graph, addresses or opcodes", DxuiToolbar::Kind::DropDown, 1, [] {});
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
    m_period     = std::min (period, GetPeriods().size() - 1);
    m_builtCycle = m_stopCycle;
    m_zoomDueMs.reset();
    m_dragFromX.reset();
    m_zoom.Reset();
    m_timeline->ResetView();
    m_timeline->SetSelection (std::nullopt);
    RunProfile (GetPeriods()[m_period].words);
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::RouteContentMouse
//
//  The timeline takes a drag, which selects frames, and the flame graph a
//  click on a bar, to zoom or to go to its routine.
//
////////////////////////////////////////////////////////////////////////////////

bool ProfilerPane::RouteContentMouse (const DxuiMouseEvent & ev)
{
    bool  isUsed = false;
    bool  isOver = ev.kind != DxuiMouseEventKind::Leave && IsOverTimeline (ev.positionDip);



    //  This is called for every mouse event in the window, so the line at
    //  the pointer is cleared once the pointer is off the timeline. A move is
    //  not consumed, so the tip is drawn too.
    if (m_host.IsRoutable (m_timeline) && m_timeline->SetPointer (isOver ? std::optional<int> (ev.positionDip.x) : std::nullopt))
    {
        m_host.Repaint();
    }

    if (RouteDrag (ev))
    {
        return true;
    }

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
//  ProfilerPane::Take
//
//  A whole new profile is the timeline's and the lists' with no zoom; the
//  rest go to the parts the zoom routed them to. Each range's profile is
//  stored in m_built, so a move back or forward doesn't start a build.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::Take (const Profiles & profiles, ProfileZoom::Target target)
{
    using Target = ProfileZoom::Target;

    std::optional<Range>  range;



    if (target == Target::None || !profiles.byOpcode.has_value())
    {
        return;
    }

    if (profiles.byOpcode->historyRange.has_value())
    {
        range = Range { (int64_t) profiles.byOpcode->historyRange->first, (int64_t) profiles.byOpcode->historyRange->second };
    }

    if (target == Target::Whole)
    {
        m_built.clear();
        m_wholeRange     = range;
        m_isSelectionDue = false;
        m_flameZoomDue.reset();
        m_zoomDueMs.reset();
        m_dragFromX.reset();
        m_timeline->ResetView();
        m_timeline->SetSelection (std::nullopt);
    }

    if (range.has_value())
    {
        m_built[*range] = profiles;
    }

    if (target != Target::Lists)
    {
        SetTimeline (profiles, range);
    }

    if (target != Target::Timeline)
    {
        SetLists (profiles, range);
    }

    //  A selection's profile is requested once the timeline's profile of
    //  the view it is in arrives.
    if (target == Target::Timeline && m_isSelectionDue && m_zoom.GetState().selection.has_value())
    {
        m_isSelectionDue = false;
        Request (*m_zoom.GetState().selection, true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::SetTimeline
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::SetTimeline (const Profiles & profiles, const std::optional<Range> & range)
{
    m_timelineData  = profiles;
    m_timelineRange = range;
    m_timeline->SetData (profiles.byOpcode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::SetLists
//
//  The flame graph zooms to the same routine in the new profile, by its
//  path from Top level, unless a move back or forward sets the state's zoom.
//  Either way the state then has the zoom the flame graph is at.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::SetLists (const Profiles & profiles, const std::optional<Range> & range)
{
    ProfileZoom::State  state;



    m_byOpcode   = profiles.byOpcode;
    m_byAddress  = profiles.byAddress;
    m_listsRange = range;
    m_flameGraph->SetData (m_byOpcode);

    if (m_flameZoomDue.has_value())
    {
        m_flameGraph->SetZoom (*m_flameZoomDue);
        m_flameZoomDue.reset();
    }

    state = m_zoom.GetState();

    if (state.flameZoom != m_flameGraph->GetZoom())
    {
        state.flameZoom = m_flameGraph->GetZoom();
        m_zoom.Replace (state);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::Request
//
//  A range's profile, from those built, or built from history.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::Request (const Range & range, bool isSelection)
{
    ProfileZoom::Request  request { range, isSelection };
    auto                  built   = m_built.find (range);
    Profiles              profiles;



    m_zoom.SetRequest (request);

    if (built != m_built.end())
    {
        profiles = built->second;
        Take (profiles, m_zoom.GetTarget (request));
        return;
    }

    RunProfile (std::format ("HISTORY CYCLES {} {}", range.first, range.second).c_str());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::ApplyState
//
//  Brings the timeline, the lists and the flame graph to the zoom's state:
//  the view at once, then each part's profile, from those built or built
//  from history. The timeline's comes first, and a selection's after it.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::ApplyState (bool isFlameZoomSet)
{
    ProfileZoom::State  state = m_zoom.GetState();
    Range               view;
    Range               lists;



    if (!m_wholeRange.has_value())
    {
        return;
    }

    view  = state.view.value_or (*m_wholeRange);
    lists = state.selection.value_or (view);

    if (state.view.has_value())
    {
        m_timeline->SetView (view.first, view.second);
    }
    else
    {
        m_timeline->ResetView();
    }

    m_timeline->SetSelection (state.selection);

    m_flameZoomDue   = isFlameZoomSet ? std::optional<size_t> (state.flameZoom) : std::nullopt;
    m_isSelectionDue = state.selection.has_value() && m_listsRange != lists;

    if (m_timelineRange != view)
    {
        Request (view, false);
    }
    else if (m_isSelectionDue)
    {
        m_isSelectionDue = false;
        Request (lists, true);
    }
    else if (m_listsRange != lists)
    {
        SetLists (m_timelineData, view);
    }
    else if (m_flameZoomDue.has_value())
    {
        m_flameGraph->SetZoom (*m_flameZoomDue);
        m_flameZoomDue.reset();
    }

    Refresh();
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::PushState
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::PushState (const ProfileZoom::State & state)
{
    m_zoom.Push (state);
    ApplyState (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::GoBack
//
////////////////////////////////////////////////////////////////////////////////

bool ProfilerPane::GoBack()
{
    if (!m_zoom.Back())
    {
        return false;
    }

    ApplyState (true);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::GoForward
//
////////////////////////////////////////////////////////////////////////////////

bool ProfilerPane::GoForward()
{
    if (!m_zoom.Forward())
    {
        return false;
    }

    ApplyState (true);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::Tick
//
//  The profile of a zoomed view is built once the wheel has stopped.
//
////////////////////////////////////////////////////////////////////////////////

bool ProfilerPane::Tick (int64_t nowMs)
{
    if (!m_zoomDueMs.has_value())
    {
        return false;
    }

    if (nowMs < *m_zoomDueMs)
    {
        return true;
    }

    m_zoomDueMs.reset();
    ApplyState (false);
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::ZoomAt
//
//  A notch of the wheel zooms the timeline around the cycle at the pointer,
//  and a horizontal notch, or one with Shift, pans it. One turn of the wheel
//  is one state, and the view's profile is built once the wheel stops.
//
////////////////////////////////////////////////////////////////////////////////

bool ProfilerPane::ZoomAt (const DxuiMouseEvent & ev)
{
    int64_t             now          = (int64_t) GetTickCount64();
    bool                isNewGesture = now - m_lastWheelMs > kWheelGestureMs;
    ProfileZoom::State  state        = m_zoom.GetState();
    Range               view         = m_timeline->GetView();
    double              width        = (double) (view.second - view.first);



    m_lastWheelMs = now;

    if (!m_wholeRange.has_value() || ev.wheelDelta == 0.0f)
    {
        return true;
    }

    if (ev.wheelHorizontal)
    {
        state.view = ProfileZoom::Pan (view, (int64_t) (width * ev.wheelDelta / 10.0), *m_wholeRange);
    }
    else if (ev.shift)
    {
        state.view = ProfileZoom::Pan (view, (int64_t) (-width * ev.wheelDelta / 10.0), *m_wholeRange);
    }
    else
    {
        state.view = ProfileZoom::ZoomAround (view, m_timeline->GetCycleAt (ev.positionDip.x), std::pow (kZoomPerNotch, (double) ev.wheelDelta), *m_wholeRange);
    }

    if (state.view == m_zoom.GetState().view)
    {
        return true;
    }

    if (isNewGesture)
    {
        m_zoom.Push (state);
    }
    else
    {
        m_zoom.Replace (state);
    }

    if (state.view.has_value())
    {
        m_timeline->SetView (state.view->first, state.view->second);
    }
    else
    {
        m_timeline->ResetView();
    }

    m_zoomDueMs = now + kZoomSettleMs;
    m_host.Repaint();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::IsOverTimeline
//
////////////////////////////////////////////////////////////////////////////////

bool ProfilerPane::IsOverTimeline (POINT positionDip) const
{
    return m_host.IsRoutable (m_timeline) && m_timeline->IsVisible() && DxuiDockSite::Contains (m_timeline->GetBounds(), positionDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::RouteEarlyMouse
//
//  The wheel over the timeline zooms it, with Ctrl or without, rather than
//  the panes' text; the mouse's back and forward buttons over the pane move
//  through the zoom's states.
//
////////////////////////////////////////////////////////////////////////////////

bool ProfilerPane::RouteEarlyMouse (const DxuiMouseEvent & ev)
{
    bool  isBackOrForward = ev.kind == DxuiMouseEventKind::Up && (ev.button == DxuiMouseButton::X1 || ev.button == DxuiMouseButton::X2);



    if (ev.kind == DxuiMouseEventKind::Wheel && IsOverTimeline (ev.positionDip))
    {
        return ZoomAt (ev);
    }

    if (!isBackOrForward || m_frame == nullptr || !m_host.IsRoutable (m_timeline) || !m_frame->IsVisible() ||
        !DxuiDockSite::Contains (m_frame->GetBounds(), ev.positionDip))
    {
        return false;
    }

    (void) ((ev.button == DxuiMouseButton::X1) ? GoBack() : GoForward());
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::RouteDrag
//
//  A press on the timeline starts a drag that selects the frames it
//  crosses, and has the mouse until the button comes up. A click with no
//  drag clears the selection.
//
////////////////////////////////////////////////////////////////////////////////

bool ProfilerPane::RouteDrag (const DxuiMouseEvent & ev)
{
    ProfileZoom::State  state = m_zoom.GetState();
    bool                isEnd = ev.kind == DxuiMouseEventKind::Up || (ev.kind == DxuiMouseEventKind::Move && ev.button != DxuiMouseButton::Left);



    if (!m_dragFromX.has_value())
    {
        if (ev.kind != DxuiMouseEventKind::Down || ev.button != DxuiMouseButton::Left || !m_wholeRange.has_value() || !IsOverTimeline (ev.positionDip))
        {
            return false;
        }

        m_dragFromX = ev.positionDip.x;
        m_dragRange.reset();
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Move && !isEnd)
    {
        if (m_dragRange.has_value() || std::abs (ev.positionDip.x - *m_dragFromX) >= kDragDip)
        {
            m_dragRange = ProfileZoom::GetFramesBetween (m_timeline->GetCycleAt (*m_dragFromX), m_timeline->GetCycleAt (ev.positionDip.x), *m_wholeRange);
            m_timeline->SetSelection (m_dragRange);
            m_host.Repaint();
        }

        return true;
    }

    if (!isEnd)
    {
        return true;
    }

    state.selection = m_dragRange;
    m_dragFromX.reset();
    m_dragRange.reset();

    if (state.selection != m_zoom.GetState().selection)
    {
        PushState (state);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::RouteEditKey
//
//  Alt+Left and Alt+Right, while a control of the pane has the focus, move
//  back and forward through the zoom's states.
//
////////////////////////////////////////////////////////////////////////////////

bool ProfilerPane::RouteEditKey (const DxuiKeyEvent & ev, IDxuiControl * focused)
{
    std::vector<IDxuiControl *>  controls = GetControls();
    bool                         isArrow  = ev.vk == VK_LEFT || ev.vk == VK_RIGHT;



    if (ev.kind != DxuiKeyEventKind::Down || !ev.alt || ev.ctrl || ev.shift || !isArrow || focused == nullptr ||
        std::find (controls.begin(), controls.end(), focused) == controls.end())
    {
        return false;
    }

    return (ev.vk == VK_LEFT) ? GoBack() : GoForward();
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

    if (m_host.IsRoutable (m_timeline) && m_timeline->TryGetTipAt (clientPx, tip.text, tip.lineSwatches))
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

bool ProfilerPane::AddMenuItems (DxuiListView * list, int row, PaneMenuItems & items)
{
    int                  entry   = (row >= 0 && (size_t) row < m_order.size()) ? (int) m_order[(size_t) row] : row;
    std::optional<Word>  address;



    if (list != m_list)
    {
        return false;
    }

    address = GetRowAddress ((m_view == View::Addresses) ? m_byAddress : m_byOpcode, m_view, entry);

    if (address.has_value())
    {
        items.push_back ({ L"Add symbol here...", [this, at = *address] { AddSymbolDialog::Run (m_host, at); } });
    }

    items.push_back ({ m_isOn ? L"Stop profiling" : L"Start profiling", [this, on = m_isOn] { RunProfile (on ? "OFF" : "ON"); } });
    items.push_back ({ L"Reset",   [this] { RunProfile ("RESET"); } });
    items.push_back ({ L"Save...", [this] { Save(); } });
    AddCopyItem (items);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::AddContentMenuItems
//
//  Over a flame graph bar, Add symbol here... for its routine; Top level
//  has no address to name.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::AddContentMenuItems (POINT clientPx, PaneMenuItems & items)
{
    std::optional<size_t>  routine;



    if (!m_host.IsRoutable (m_flameGraph) || !m_flameGraph->IsVisible() || !m_byOpcode.has_value())
    {
        return;
    }

    routine = m_flameGraph->FindRoutineAt (clientPx);

    if (routine.has_value() && *routine < m_byOpcode->routines.size() && m_byOpcode->routines[*routine].kind != RoutineKind::Top)
    {
        items.push_back ({ L"Add symbol here...", [this, at = m_byOpcode->routines[*routine].address] { AddSymbolDialog::Run (m_host, at); } });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::GetCells
//
//  A row an opcode, an address, a routine or a node of the call tree; an
//  opcode with no operand has a blank mode. With no profile read yet, a
//  note: how to build one while profiling is off, and that the machine must
//  be paused while it is on.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::vector<DxuiListView::Cell>> ProfilerPane::GetCells (const std::optional<ProfileData> & profile, View view, bool isOn)
{
    std::vector<std::vector<DxuiListView::Cell>>  cells;
    auto                                          share = [&profile] (uint64_t cycles)
    {
        return std::format (L"{:.1f}%", 100.0 * (double) cycles / (double) std::max<uint64_t> (profile->cycles, 1));
    };
    auto                                          count = [] (uint64_t value) { return DebuggerStatusText::FormatCount (value); };



    if (!profile.has_value())
    {
        cells.push_back (MakeNote (isOn ? L"Pause the machine to read the profile."
                                        : L"Choose a period in Show to build a profile from history."));
        return cells;
    }

    if (view == View::Addresses)
    {
        for (const ProfileAddressEntry & entry : profile->addresses)
        {
            cells.push_back ({ { std::format (L"${:04X}", entry.address) }, { TextEncoding::NarrowToWide (entry.symbol) }, { count (entry.cycles) } });
        }
    }
    else if (view == View::Routines)
    {
        //  A routine's swatch is its color in the timeline; the routines it
        //  draws together in gray have none.
        for (size_t i = 0; i < profile->routines.size(); i++)
        {
            const ProfileRoutineEntry  & entry = profile->routines[i];

            cells.push_back ({ { TextEncoding::NarrowToWide (entry.GetName()) }, { count (entry.self) }, { share (entry.self) },
                               { count (entry.total) }, { share (entry.total) }, { count (entry.calls) },
                               { (entry.calls != 0) ? count (entry.total / entry.calls) : std::wstring (L"-") } });
            cells.back()[0].swatch = ProfileTimeline::GetShareColor (i, 0);
        }
    }
    else if (view == View::Tree)
    {
        for (const ProfileTreeEntry & entry : profile->tree)
        {
            cells.push_back ({ { std::wstring ((size_t) entry.depth * 2, L' ') + TextEncoding::NarrowToWide (profile->routines[entry.routine].GetName()) },
                               { count (entry.self) }, { count (entry.total) }, { share (entry.total) }, { count (entry.calls) } });
            cells.back()[0].swatch = ProfileTimeline::GetShareColor (entry.routine, 0);
        }
    }
    else if (view == View::Opcodes)
    {
        for (const ProfileEntry & entry : profile->opcodes)
        {
            cells.push_back ({ { TextEncoding::NarrowToWide (entry.mnemonic) }, { (entry.mode == "[No Operand]") ? std::wstring() : TextEncoding::NarrowToWide (entry.mode) },
                               { count (entry.count) }, { count (entry.cycles) } });
        }
    }

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::GetTotalsText
//
//  The span is the cycles at the machine's clock, about 1.02 MHz, so a
//  profile of a longer period than history covers reads as the shorter one
//  it is.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ProfilerPane::GetTotalsText (const std::optional<ProfileData> & profile)
{
    constexpr double  kCyclesPerSecond = 1020484.0;



    if (!profile.has_value())
    {
        return std::wstring();
    }

    return std::format (L"{} s of running: {} instructions, {} cycles, {} page crossings, {} branches taken",
                        FormatSeconds ((double) profile->cycles / kCyclesPerSecond), DebuggerStatusText::FormatCount (profile->instructions),
                        DebuggerStatusText::FormatCount (profile->cycles), DebuggerStatusText::FormatCount (profile->pageCross),
                        DebuggerStatusText::FormatCount (profile->branchTaken));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::FormatSeconds
//
//  Tenths of a second, the whole seconds grouped as a count is.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ProfilerPane::FormatSeconds (double seconds)
{
    uint64_t  tenths = (uint64_t) std::llround (std::max (seconds, 0.0) * 10.0);



    return std::format (L"{}.{}", DebuggerStatusText::FormatCount (tenths / 10), tenths % 10);
}
