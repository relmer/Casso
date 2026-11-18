#include "Pch.h"

#include "Ui/Debugger/Panes/ProfilerPane.h"
#include "Ui/Debugger/SymbolComments.h"
#include "Ui/Debugger/Panes/PaneMenuRows.h"
#include "Ui/Debugger/SessionActions.h"

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

ProfilerPane::ProfilerPane (const DebuggerPaneParts & parts) :
    ToolListPane     (parts, DebuggerLayout::kProfiler, L"Profiler"),
    m_windowHost     (parts.windowHost),
    m_paneWindow     (parts.window),
    m_paneDialogs    (parts.dialogs),
    m_paneLayout     (parts.layout),
    m_paneRouting    (parts.routing),
    m_paneNavigator  (parts.navigator),
    m_paneCommands   (parts.commands),
    m_symbolComments (parts.symbolComments),
    m_latest         (parts.latest),
    m_bar            (parts, DebuggerLayout::kProfiler, L"Profiler commands"),
    m_navigator      (parts, *this)
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

    m_timeline   = m_paneWindow.CreateChild<ProfileTimeline>();
    m_totals     = m_paneWindow.CreateChild<DxuiLabel>();
    m_flameGraph = m_paneWindow.CreateChild<FlameGraphView>();
    m_timeline  ->SetTipFn ([this] (POINT clientPx, DxuiTip & tip) { return TryGetTipAt (clientPx, tip); });
    m_flameGraph->SetTipFn ([this] (POINT clientPx, DxuiTip & tip) { return TryGetTipAt (clientPx, tip); });
    m_flameGraph->SetTipHead ([this] (const ProfileRoutineEntry & routine) { return GetTipHeadWithComment (routine); });
    m_progress   = m_paneWindow.CreateChild<ProfileProgress>();
    m_bar.Create();

    m_totals->SetTextRole  (DxuiTextRole::Muted);
    m_totals->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    m_flameGraph->SetOnActivate ([this] (Word address)    { m_paneNavigator.GoToCode (address); });
    m_flameGraph->SetOnSelect   ([this] (size_t routine)  { m_selectedRoutine = routine; });
    m_flameGraph->SetOnZoom     ([this] (size_t node)   { m_navigator.OnFlameZoom (node); });

    m_navigator.SetControls (m_timeline, m_flameGraph);
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
        std::optional<Word>  address = ProfileRows::GetRowAddress ((m_view == ProfileRows::View::Addresses) ? m_navigator.GetLists().byAddress : m_navigator.GetLists().byOpcode, m_view, entry);



        if (address.has_value())
        {
            m_paneNavigator.GoToCode (*address);
        }
    });

    SetView (ProfileRows::View::Routines);

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
    auto  isFlame = [this] { return m_view == ProfileRows::View::Flame; };



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

void ProfilerPane::SetView (ProfileRows::View view)
{
    m_view = view;

    //  The flame graph and the list take turns in the pane, so it is laid
    //  out again.
    if (m_frame != nullptr)
    {
        m_frame->Relayout();
        m_paneLayout.Repaint();
    }

    switch (view)
    {
    case ProfileRows::View::Addresses:
        m_list->SetColumns ({ { L"Address", 0, false, DxuiTextHAlign::Left,  true, L"The address of an instruction that ran" },
                              { L"Symbol",  0, false, DxuiTextHAlign::Left,  true, L"The symbol at the address, or the nearest one before it" },
                              { L"Cycles",  0, false, DxuiTextHAlign::Right, true, L"The cycles the instructions at the address took during the period" } });
        break;

    case ProfileRows::View::Routines:
        m_list->SetColumns ({ { L"Routine",  0, false, DxuiTextHAlign::Left,  true, ProfileRows::GetRoutineTip() },
                              { L"Self",     0, false, DxuiTextHAlign::Right, true, ProfileRows::GetSelfTip() },
                              { L"Self %",   0, false, DxuiTextHAlign::Right, true, L"Self as a share of the profile's cycles" },
                              { L"Total",    0, false, DxuiTextHAlign::Right, true, ProfileRows::GetTotalTip() },
                              { L"Total %",  0, false, DxuiTextHAlign::Right, true, L"Total as a share of the profile's cycles" },
                              { L"Calls",    0, false, DxuiTextHAlign::Right, true, ProfileRows::GetCallsTip() },
                              { L"Per call", 0, false, DxuiTextHAlign::Right, true, L"Total divided by calls: the cycles one call takes on average" } });
        break;

    case ProfileRows::View::Tree:
        m_list->SetColumns ({ { L"Routine", 0, false, DxuiTextHAlign::Left,  true, L"Each routine under the routine that called it" },
                              { L"Self",    0, false, DxuiTextHAlign::Right, true, ProfileRows::GetSelfTip() },
                              { L"Total",   0, false, DxuiTextHAlign::Right, true, ProfileRows::GetTotalTip() },
                              { L"Total %", 0, false, DxuiTextHAlign::Right, true, L"Total as a share of the profile's cycles" },
                              { L"Calls",   0, false, DxuiTextHAlign::Right, true, ProfileRows::GetCallsTip() } });
        break;

    case ProfileRows::View::Flame:
        break;

    default:
        m_list->SetColumns ({ { L"Opcode", 0, false, DxuiTextHAlign::Left,  true, L"The instruction's mnemonic" },
                              { L"Mode",   0, false, DxuiTextHAlign::Left,  true, L"The instruction's addressing mode" },
                              { L"Count",  0, false, DxuiTextHAlign::Right, true, L"The times the instruction ran during the period" },
                              { L"Cycles", 0, false, DxuiTextHAlign::Right, true, L"The cycles the instruction took during the period" } });
        break;
    }

    Refresh();
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
    ProfileRows::Sort  & sort = m_sorts[(size_t) m_view];



    sort.descending = (column == sort.column) ? !sort.descending : !ProfileRows::IsTextColumn (m_view, column);
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

    target = m_navigator.TakeSnapshotProfile (snapshot.profile, snapshot.profileByAddress);

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
//  The chosen period's profile, built at a stop while the pane is open on a
//  stopped machine with history, so the profile appears without the period
//  being chosen again. A stop less than kRebuildCycles from the last build,
//  as a step is, doesn't rebuild the profile.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::BuildIfDue()
{
    bool      isOpen   = m_frame != nullptr && m_frame->IsVisible();
    uint64_t  distance = 0;



    if (!isOpen || !m_isPaused || m_isBuilding || !m_stopCycle.has_value())
    {
        return;
    }

    if (m_builtCycle.has_value())
    {
        distance = (*m_stopCycle > *m_builtCycle) ? *m_stopCycle - *m_builtCycle : *m_builtCycle - *m_stopCycle;

        if (distance < kRebuildCycles)
        {
            return;
        }
    }

    if (!m_isPeriodRead)
    {
        IDebuggerWindowHost  * host = m_windowHost;



        m_period       = GetPeriodIndex ((host != nullptr) ? host->GetDebuggerProfilerPeriod() : std::string());
        m_isPeriodRead = true;
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
    const std::optional<ProfileData>              & profile  = (m_view == ProfileRows::View::Addresses) ? m_navigator.GetLists().byAddress : m_navigator.GetLists().byOpcode;
    const ProfileRows::Sort                       & sort     = m_sorts[(size_t) m_view];
    bool                                            wasShown = !m_totals->GetText().empty();
    std::vector<std::vector<DxuiListView::Cell>>    cells    = ProfileRows::GetCells (profile, m_view, m_isOn, [this] (const ProfileRoutineEntry & routine) { return GetTipHeadWithComment (routine); });
    std::vector<std::vector<DxuiListView::Cell>>    sorted;



    m_order.clear();

    if (profile.has_value() && m_view != ProfileRows::View::Flame)
    {
        m_order = ProfileRows::GetOrder (*profile, m_view, sort.column, sort.descending);
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
        bool    isMatch = (m_view == ProfileRows::View::Routines) ? entry == *m_selectedRoutine
                                                     : m_view == ProfileRows::View::Tree && profile->tree[entry].routine == *m_selectedRoutine;

        if (isMatch)
        {
            m_list->SetSelectedRow ((int) row);
            m_selectedRoutine.reset();
            break;
        }
    }

    m_totals->SetText (ProfileRows::GetTotalsText (m_navigator.GetLists().byOpcode));

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
    m_paneCommands.RunAction (SessionActions::GetProfile (words, m_latest.GetMode()));
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
        label.pinned        = true;
        entries.push_back (std::move (label));
    }

    command            = add (s_kProfilePeriod, L"Last 5 s", L"Build the profile of a period of history", DxuiToolbar::Kind::DropDown, 0, [] {});
    command->labelText = [this] { return std::wstring (GetPeriods()[m_period].label); };

    add (s_kProfileReset, L"Reset",   L"Start the counts over",           DxuiToolbar::Kind::Command, 0, [this] { RunProfile ("RESET"); }, s_kpszMdl2Refresh);
    add (s_kProfileSave,  L"Save...", L"Save the profile to a text file", DxuiToolbar::Kind::Command, 0, [this] { Save(); },            s_kpszMdl2Save);

    command            = add (s_kProfileView, L"Routines", L"Choose the profile's view: routines, the call tree, a flame graph, addresses or opcodes", DxuiToolbar::Kind::DropDown, 1, [] {});
    command->labelText = [this] { return std::wstring (ProfileRows::GetViewLabel (m_view)); };

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



    for (int view = 0; view < ProfileRows::kViewCount; view++)
    {
        views.push_back (DxuiPopupMenuItem::ForCommand (DebuggerMenuCommand::Make (ProfileRows::GetViewLabel ((ProfileRows::View) view), (int) m_view == view,
                                                                                  [this, view] { SetView ((ProfileRows::View) view); })));
    }

    m_bar.GetBar()->SetDropDownItems (s_kProfileView, std::move (views));

    views.clear();

    for (size_t period = 0; period < GetPeriods().size(); period++)
    {
        views.push_back (DxuiPopupMenuItem::ForCommand (DebuggerMenuCommand::Make (GetPeriods()[period].label, m_period == period,
                                                                                  [this, period] { ChoosePeriod (period); })));
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
        { "HISTORY 5",   L"Last 5 s"            },
        { "HISTORY 10",  L"Last 10 s"           },
        { "HISTORY 60",  L"Last 60 s"           },
        { "HISTORY 300", L"Last 5 min"          },
        { "HISTORY ALL", L"Since history start" },
    };



    return periods;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::GetPeriodIndex
//
//  The period whose HISTORY argument is `argument`, or the first, the last
//  5 seconds, for an empty or unknown one.
//
////////////////////////////////////////////////////////////////////////////////

size_t ProfilerPane::GetPeriodIndex (const std::string & argument)
{
    const std::vector<Period>  & periods = GetPeriods();
    std::string                  words   = "HISTORY " + argument;



    for (size_t period = 0; period < periods.size(); period++)
    {
        if (words == periods[period].words)
        {
            return period;
        }
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::ChoosePeriod
//
//  The user's choice from the period menu, saved for the next session.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::ChoosePeriod (size_t period)
{
    IDebuggerWindowHost  * host = m_windowHost;



    ShowPeriod (period);

    if (host != nullptr)
    {
        host->SetDebuggerProfilerPeriod (std::string (GetPeriods()[m_period].words).substr (std::size ("HISTORY ") - 1));
    }
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
    m_navigator.Reset();
    RunProfile (GetPeriods()[m_period].words);
    m_paneLayout.Repaint();
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
    bool  isOver = ev.kind != DxuiMouseEventKind::Leave && m_navigator.IsOverTimeline (ev.positionDip);



    //  This is called for every mouse event in the window, so the line at
    //  the pointer is cleared once the pointer is off the timeline. A move is
    //  not consumed, so the tip is drawn too.
    if (m_paneRouting.IsRoutable (m_timeline) && m_timeline->SetPointer (isOver ? std::optional<int> (ev.positionDip.x) : std::nullopt))
    {
        m_paneLayout.Repaint();
    }

    if (m_navigator.RouteDrag (ev))
    {
        return true;
    }

    if (!m_paneRouting.IsRoutable (m_flameGraph) || !m_flameGraph->IsVisible() || !DxuiDockSite::Contains (m_flameGraph->GetBounds(), ev.positionDip))
    {
        return false;
    }

    isUsed = m_flameGraph->OnMouse (ev);

    if (isUsed)
    {
        m_paneLayout.Repaint();
    }

    return isUsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::Tick
//
//  The profile of a zoomed view is built once the wheel has stopped, and the
//  flame graph's clicked bar marches.
//
////////////////////////////////////////////////////////////////////////////////

bool ProfilerPane::Tick (int64_t nowMs)
{
    bool  isWaiting  = m_navigator.Tick (nowMs);
    bool  isMarching = m_flameGraph != nullptr && m_paneRouting.IsRoutable (m_flameGraph) && m_flameGraph->Animate (nowMs);



    return isWaiting || isMarching;
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
    bool  isBackOrForward = ev.kind == DxuiMouseEventKind::Up && (ev.button == DxuiMouseButton::Back || ev.button == DxuiMouseButton::Forward);



    if (ev.kind == DxuiMouseEventKind::Wheel && m_navigator.IsOverTimeline (ev.positionDip))
    {
        return m_navigator.ZoomAt (ev);
    }

    if (!isBackOrForward || m_frame == nullptr || !m_paneRouting.IsRoutable (m_timeline) || !m_frame->IsVisible() ||
        !DxuiDockSite::Contains (m_frame->GetBounds(), ev.positionDip))
    {
        return false;
    }

    (void) ((ev.button == DxuiMouseButton::Back) ? m_navigator.GoBack() : m_navigator.GoForward());
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

    return (ev.vk == VK_LEFT) ? m_navigator.GoBack() : m_navigator.GoForward();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::TryGetTipAt
//
//  Over the timeline, the frame there; over the flame graph, the routine;
//  each at once and following the pointer.
//
////////////////////////////////////////////////////////////////////////////////

bool ProfilerPane::TryGetTipAt (POINT clientPx, DxuiTip & tip)
{
    tip.placement = DxuiTip::Placement::FollowPointer;

    if (m_paneRouting.IsRoutable (m_timeline) && m_timeline->TryGetTipAt (clientPx, tip.text, tip.lineSwatches))
    {
        return true;
    }

    return m_paneRouting.IsRoutable (m_flameGraph) && m_flameGraph->TryGetTipAt (clientPx, tip.text);
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

    address = ProfileRows::GetRowAddress ((m_view == ProfileRows::View::Addresses) ? m_navigator.GetLists().byAddress : m_navigator.GetLists().byOpcode, m_view, entry);

    if (address.has_value())
    {
        items.push_back ({ L"Add symbol here...", [this, at = *address] { AddSymbolDialog::Run (m_paneDialogs, m_paneCommands, m_latest.GetMode(), at); } });
        PaneMenuRows::AddBookmark (m_latest, m_paneCommands, *address, items);
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



    if (!m_paneRouting.IsRoutable (m_flameGraph) || !m_flameGraph->IsVisible() || !m_navigator.GetLists().byOpcode.has_value())
    {
        return;
    }

    routine = m_flameGraph->FindRoutineAt (clientPx);

    if (routine.has_value() && *routine < m_navigator.GetLists().byOpcode->routines.size() && m_navigator.GetLists().byOpcode->routines[*routine].kind != RoutineKind::Top)
    {
        items.push_back ({ L"Add symbol here...", [this, at = m_navigator.GetLists().byOpcode->routines[*routine].address] { AddSymbolDialog::Run (m_paneDialogs, m_paneCommands, m_latest.GetMode(), at); } });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfilerPane::GetTipHeadWithComment
//
////////////////////////////////////////////////////////////////////////////////

std::string ProfilerPane::GetTipHeadWithComment (const ProfileRoutineEntry & routine) const
{
    std::string  head    = routine.GetTipHead();
    std::string  comment = (routine.kind != RoutineKind::Top && !routine.symbol.empty()) ? m_symbolComments.GetComment (m_latest.Get(), m_windowHost, routine.address, routine.symbol) : std::string();



    return comment.empty() ? head : head + "\n" + comment;
}
