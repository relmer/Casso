#include "Pch.h"

#include "Ui/Debugger/Panes/ProfilerPane.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Debugger/SymbolTable.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerMenuCommand.h"
#include "Ui/Debugger/DebuggerStatusText.h"
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
//  ProfilerPane::Apply
//
//  A snapshot of a running machine has no profile; the last one read stays.
//
////////////////////////////////////////////////////////////////////////////////

void ProfilerPane::Apply (const DebuggerViewSnapshot & snapshot)
{
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
    bool  wasShown = !m_totals->GetText().empty();



    m_list->SetRows (GetCells ((m_view == View::Addresses) ? m_byAddress : m_byOpcode, m_view, m_isOn));
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
        for (const ProfileRoutineEntry & entry : profile->routines)
        {
            cells.push_back ({ { TextEncoding::NarrowToWide (entry.GetName()) }, { count (entry.self) }, { share (entry.self) },
                               { count (entry.total) }, { share (entry.total) }, { count (entry.calls) },
                               { (entry.calls != 0) ? count (entry.total / entry.calls) : std::wstring (L"-") } });
        }
    }
    else if (view == View::Tree)
    {
        for (const ProfileTreeEntry & entry : profile->tree)
        {
            cells.push_back ({ { std::wstring ((size_t) entry.depth * 2, L' ') + TextEncoding::NarrowToWide (profile->routines[entry.routine].GetName()) },
                               { count (entry.self) }, { count (entry.total) }, { share (entry.total) }, { count (entry.calls) } });
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
