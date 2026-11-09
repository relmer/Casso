#include "Pch.h"

#include "Ui/Debugger/Panes/EventLogControllers.h"

#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerMenuCommand.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/DiskLogPane.h"
#include "Ui/Debugger/Panes/InputLogPane.h"
#include "Ui/Debugger/Panes/IoLogPane.h"
#include "Ui/Debugger/ToolbarBoxEntry.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Core/TextEncoding.h"
#include "Seams/IHostDialogs.h"





//  The event log bars' entries.
static constexpr int  s_kInputPair0        = 8;
static constexpr int  s_kInputPair1        = 9;
static constexpr int  s_kIoLogClear        = 10;
static constexpr int  s_kIoLogSave         = 11;

//  How far a warning or error row's fill is from the list's background
//  toward the theme's warning or error color.
static constexpr float  s_kSeverityTint = 0.25f;





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController::EventLogController
//
////////////////////////////////////////////////////////////////////////////////

EventLogController::EventLogController (IDebuggerPaneHost & host, std::wstring id, std::wstring title, DebugLogKind kind) :
    IDebuggerPane (host),
    m_id          (id),
    m_title       (std::move (title)),
    m_kind        (kind),
    m_bar         (host, std::move (id), L"Event log commands")
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController::~EventLogController
//
////////////////////////////////////////////////////////////////////////////////

EventLogController::~EventLogController() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController::CreateListAndBar
//
////////////////////////////////////////////////////////////////////////////////

void EventLogController::CreateListAndBar()
{
    m_list = m_host.GetControlParent().CreateChild<DxuiListView>();
    m_bar.Create();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController::ConfigureColumnFilters
//
////////////////////////////////////////////////////////////////////////////////

void EventLogController::ConfigureColumnFilters()
{
    m_list->SetOnHeaderFilter ([this] (int column, RECT chevron)
    {
        RECT  bounds = m_list->GetBounds();



        ShowColumnFilter ((size_t) column, RECT { bounds.left + chevron.left, bounds.top + chevron.top, bounds.left + chevron.right, bounds.top + chevron.bottom });
    });

    ShowFilterState();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController::ShowFilterState
//
//  A chevron on each heading that filters, and the funnel on each that does.
//
////////////////////////////////////////////////////////////////////////////////

void EventLogController::ShowFilterState()
{
    IEventLogFilter  * filters = GetColumnFilters();



    for (size_t column = 0; filters != nullptr && column < m_list->GetColumnCount(); column++)
    {
        m_list->SetColumnFilter (column, filters->GetFilterKind (column) != IEventLogFilter::Kind::None, filters->IsFiltered (column));
    }

    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController::ShowColumnFilter
//
//  A column of values lists each value its events show, checked while the
//  list shows it; a check filters at once and leaves the menu open, as File
//  Explorer's does. A column of numbers offers the comparisons. Clear filter
//  ends the column's filter.
//
////////////////////////////////////////////////////////////////////////////////

void EventLogController::ShowColumnFilter (size_t column, RECT chevronPx)
{
    static constexpr std::pair<IEventLogFilter::Op, const wchar_t *>  s_kComparisons[] =
    {
        { IEventLogFilter::Op::Less,    L"Less than..."    },
        { IEventLogFilter::Op::Greater, L"Greater than..." },
        { IEventLogFilter::Op::Equal,   L"Equal to..."     },
        { IEventLogFilter::Op::Between, L"Between..."      },
    };
    IEventLogFilter                 * filters = GetColumnFilters();
    std::vector<DxuiPopupMenuItem>    items;



    if (filters == nullptr || m_host.GetPanePopupHost() == nullptr)
    {
        return;
    }

    m_filterCommands.clear();

    if (filters->GetFilterKind (column) == IEventLogFilter::Kind::Values)
    {
        for (const std::wstring & value : filters->GetFilterValues (column))
        {
            std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();

            command->label         = value;
            command->keepsMenuOpen = true;
            command->checkBox      = true;
            command->isChecked     = [filters, column, value]() { return filters->IsValueChecked (column, value); };
            command->dispatch      = [this, filters, column, value]()
            {
                filters->ToggleValue (column, value);
                ShowFilterState();
                SaveOptions();
            };

            items.push_back (DxuiPopupMenuItem::ForCommand (command));
            m_filterCommands.push_back (std::move (command));
        }
    }
    else if (filters->GetFilterKind (column) == IEventLogFilter::Kind::Compare)
    {
        for (const auto & [op, label] : s_kComparisons)
        {
            m_filterCommands.push_back (DebuggerMenuCommand::Make (label, [this, column, op] { AskComparison (column, op); }));
            items.push_back (DxuiPopupMenuItem::ForCommand (m_filterCommands.back()));
        }
    }

    if (filters->IsFiltered (column))
    {
        items.push_back (DxuiPopupMenuItem::ForSeparator());
        m_filterCommands.push_back (DebuggerMenuCommand::Make (L"Clear filter", [this, filters, column]
        {
            filters->ClearFilter (column);
            ShowFilterState();
            SaveOptions();
        }));
        items.push_back (DxuiPopupMenuItem::ForCommand (m_filterCommands.back()));
    }

    if (!items.empty())
    {
        DxuiContextMenu::ShowUnder (*m_host.GetPanePopupHost(), chevronPx, std::move (items));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController::AskComparison
//
//  The number, or for Between the two, typed as the column shows them.
//
////////////////////////////////////////////////////////////////////////////////

void EventLogController::AskComparison (size_t column, IEventLogFilter::Op op)
{
    IEventLogFilter               * filters    = GetColumnFilters();
    std::wstring                    heading    = (column < m_list->GetColumnCount()) ? m_list->GetColumnAt (column).title : std::wstring();
    bool                            isBetween  = op == IEventLogFilter::Op::Between;
    IEventLogFilter::Comparison     comparison;
    std::optional<std::wstring>     typed;
    std::vector<std::wstring>       parts;
    auto                            split      = [] (const std::wstring & text)
    {
        size_t  at = text.find (L" to ");



        return (at == std::wstring::npos) ? std::vector<std::wstring> { text } : std::vector<std::wstring> { text.substr (0, at), text.substr (at + 4) };
    };



    if (filters == nullptr)
    {
        return;
    }

    typed = m_host.RunTextDialog (std::format (L"Filter {}", heading), isBetween ? L"From and to" : L"Value",
                                  isBetween ? L"100 to 200" : L"as the column shows it",
                                  [filters, column, isBetween, split] (const std::wstring & text, std::wstring & error)
    {
        std::vector<std::wstring>  each = split (text);
        bool                       isOk = each.size() == (isBetween ? 2u : 1u);



        for (const std::wstring & part : each)
        {
            isOk = isOk && filters->ParseFilterNumber (column, part).has_value();
        }

        error = isOk ? std::wstring() : (isBetween ? L"Type two values as the column shows them, as 100 to 200." : L"Type a value as the column shows it.");
        return isOk;
    });

    if (!typed.has_value())
    {
        return;
    }

    parts            = split (*typed);
    comparison.op    = op;
    comparison.first = filters->ParseFilterNumber (column, parts[0]).value_or (0);
    comparison.last  = isBetween ? filters->ParseFilterNumber (column, parts[1]).value_or (0) : comparison.first;

    if (comparison.last < comparison.first)
    {
        std::swap (comparison.first, comparison.last);
    }

    filters->SetComparison (column, comparison);
    ShowFilterState();
    SaveOptions();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController::ConfigureBar
//
////////////////////////////////////////////////////////////////////////////////

void EventLogController::ConfigureBar()
{
    m_bar.Configure();
    m_bar.SetOnPress ([this] { SetMenus(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController::LoadOptions
//
//  The pane's filters and columns, and whether it was ever opened, from the
//  host's options text.
//
////////////////////////////////////////////////////////////////////////////////

std::string EventLogController::LoadOptions()
{
    IDebuggerWindowHost  * windowHost = GetWindowHost();
    std::string            text       = (windowHost != nullptr) ? windowHost->GetDebuggerEventLogOptions (m_kind) : std::string();



    SetOptionsText (text);
    m_isOpened = text.find ("open=1") != std::string::npos;

    if (GetColumnFilters() != nullptr)
    {
        GetColumnFilters()->SetFilterText (text);
        ShowFilterState();
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController::SaveOptions
//
////////////////////////////////////////////////////////////////////////////////

void EventLogController::SaveOptions()
{
    IDebuggerWindowHost  * windowHost = GetWindowHost();



    if (windowHost != nullptr)
    {
        windowHost->SetDebuggerEventLogOptions (m_kind, GetOptionsText() + (GetColumnFilters() != nullptr ? ";" + GetColumnFilters()->GetFilterText() : std::string()) + (m_isOpened ? ";open=1" : ""));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController::AddEntry
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiCommand> EventLogController::AddEntry (
    std::vector<DxuiToolbar::Entry>  & entries,
    int                                id,
    const wchar_t                    * label,
    const wchar_t                    * tip,
    DxuiToolbar::Kind                  kind,
    int                                group)
{
    std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
    DxuiToolbar::Entry            entry;



    command->id       = id;
    command->label    = label;
    command->tip      = tip;
    command->dispatch = [] {};

    entry.command = command;
    entry.kind    = kind;
    entry.group   = group;

    m_commands.push_back (command);
    entries.push_back (std::move (entry));

    return command;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController::Open
//
//  The pane opened for good, with its options: it comes back open with the
//  window.
//
////////////////////////////////////////////////////////////////////////////////

void EventLogController::Open()
{
    if (!m_isOpened)
    {
        m_isOpened = true;
        SaveOptions();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController::GetContent
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * EventLogController::GetContent() const
{
    return m_frame.get();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController::Apply
//
//  What a frame's rows depend on of the machine: the position it has been
//  taken back to, whether it is a //c, and whether it has a Disk II
//  controller, which a machine with no Disk II panel has not.
//
////////////////////////////////////////////////////////////////////////////////

void EventLogController::Apply (const DebuggerViewSnapshot & snapshot)
{
    m_shownBefore = snapshot.history.isBehindLive ? snapshot.history.position : UINT64_MAX;
    m_isTwoC      = snapshot.machine.find ("2c") != std::string::npos || snapshot.machine.find ("//c") != std::string::npos;
    m_hasDisk     = std::ranges::any_of (snapshot.panels, [] (const DebuggerViewSnapshot::PanelInfo & panel) { return panel.id == "disk"; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController::OnProjectSwitched
//
////////////////////////////////////////////////////////////////////////////////

void EventLogController::OnProjectSwitched()
{
    (void) LoadOptions();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController::OnWindowClose
//
//  The log's column widths, which a drag changes without a notice; and the
//  machine's thread stops sending deltas until the window opens.
//
////////////////////////////////////////////////////////////////////////////////

void EventLogController::OnWindowClose()
{
    SaveOptions();
    SetModelShown (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController::CopyRows
//
//  The log's selected rows, or every row in its list, to the clipboard.
//
////////////////////////////////////////////////////////////////////////////////

bool EventLogController::CopyRows (bool isAll)
{
    DxuiClipboard::SetText (m_host.GetOwnerHwnd(), GetModelText (!isAll));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController::RouteEditKey
//
//  Ctrl+C in the log's list copies its selected rows.
//
////////////////////////////////////////////////////////////////////////////////

bool EventLogController::RouteEditKey (const DxuiKeyEvent & ev, IDxuiControl * focused)
{
    return ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && !ev.shift && ev.vk == 'C' && focused == m_list && CopyRows (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController::AddMenuItems
//
//  Copy for the selected rows, and Copy all rows.
//
////////////////////////////////////////////////////////////////////////////////

bool EventLogController::AddMenuItems (DxuiListView * list, int, PaneMenuItems & items)
{
    if (list != m_list)
    {
        return false;
    }

    items.push_back ({ L"Copy",          [this] { (void) CopyRows (false); } });
    items.push_back ({ L"Copy all rows", [this] { (void) CopyRows (true);  } });
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController::AddColumnMenuItems
//
//  A check for each column. The text column is always in the list, so its
//  check is dimmed.
//
////////////////////////////////////////////////////////////////////////////////

bool EventLogController::AddColumnMenuItems (DxuiListView * list, std::vector<std::shared_ptr<DxuiCommand>> & commands)
{
    size_t  column = 0;



    if (list != m_list)
    {
        return false;
    }

    for (column = 0; column < list->GetColumnCount(); column++)
    {
        size_t  each = column;

        commands.push_back (DebuggerMenuCommand::Make (list->GetColumnAt (column).title, list->IsColumnVisible (column), [this, list, each]
        {
            list->SetColumnVisible (each, !list->IsColumnVisible (each));
            SaveOptions();
        }));

        if (column == GetTextColumn())
        {
            commands.back()->isEnabled = [] { return false; };
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogController::DiskLogController
//
////////////////////////////////////////////////////////////////////////////////

DiskLogController::DiskLogController (IDebuggerPaneHost & host) :
    EventLogController (host, DebuggerLayout::kDiskLog, L"Disk II log", DebugLogKind::Disk)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogController::~DiskLogController
//
////////////////////////////////////////////////////////////////////////////////

DiskLogController::~DiskLogController() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogController::CreateControls
//
//  The bar before its boxes, so the boxes paint over the strip.
//
////////////////////////////////////////////////////////////////////////////////

void DiskLogController::CreateControls()
{
    DxuiPanel  & parent = m_host.GetControlParent();



    CreateListAndBar();

    //  A sector's hex dump in a row's tip lines up in columns.
    m_list->SetMonospaceTips (true);

    m_error = parent.CreateChild<DxuiLabel>();


    m_model = std::make_unique<DiskLogPane> (m_list, [this] (const std::string & request)
    {
        if (GetWindowHost() != nullptr)
        {
            GetWindowHost()->SendDebuggerEventLogRequest (DebugLogKind::Disk, request);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogController::Configure
//
////////////////////////////////////////////////////////////////////////////////

void DiskLogController::Configure()
{
    m_model->Configure();
    ConfigureColumnFilters();
    (void) LoadOptions();

    //  The columns' chevrons filter now, so filters an older Casso saved
    //  from the bar show everything.
    for (size_t kind = 0; kind < DiskLogPane::kKindCount; kind++)
    {
        m_model->SetKindShown ((DiskLogPane::Kind) kind, true);
    }

    for (size_t severity = 0; severity < DiskLogPane::kSeverityCount; severity++)
    {
        m_model->SetSeverityShown ((DiskLogPane::Severity) severity, true);
    }

    m_model->SetDrive      (0);
    m_model->SetTrackText  (std::wstring());
    m_model->SetSectorText (std::wstring());

    m_error->SetTextRole  (DxuiTextRole::Muted);
    m_error->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

    ConfigureBar();
    SetBarEntries();
    SetMenus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogController::AddToDock
//
//  The bar over the rows, and between them a line saying the machine has no
//  Disk II controller, only when it hasn't.
//
////////////////////////////////////////////////////////////////////////////////

void DiskLogController::AddToDock (DxuiDockSite & dock)
{
    m_frame = std::make_unique<DebuggerPaneFrame> (m_title);
    m_frame->AddPart (m_bar.GetSlot(), PaneBarSlot::GetHeightPx);
    m_frame->AddPart (m_error, [] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (kErrorDip); },
                      [this] { return !m_error->GetText().empty(); });
    m_frame->SetPartTextAligned (m_error);
    m_frame->AddPart (m_list);

    dock.AddPane (m_id, m_title, m_frame.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogController::GetControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> DiskLogController::GetControls() const
{
    return { m_bar.GetSlot(), m_error, m_list, m_bar.GetBar() };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogController::SetBarEntries
//
//  Find. The columns' chevrons filter the rows, and the columns are chosen
//  from the heading's menu.
//
////////////////////////////////////////////////////////////////////////////////

void DiskLogController::SetBarEntries()
{
    std::vector<DxuiToolbar::Entry>  entries;



    m_commands.clear();
    entries.push_back (m_host.GetFindEntry (m_id));
    m_bar.GetBar()->SetEntries (std::move (entries));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogController::AddMenuItems
//
//  On a sector's row, Copy sector bytes too, as its hex dump.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskLogController::AddMenuItems (DxuiListView * list, int row, PaneMenuItems & items)
{
    std::shared_ptr<const std::vector<Byte>>  bytes;



    if (!EventLogController::AddMenuItems (list, row, items))
    {
        return false;
    }

    bytes = m_model->GetRowData (row);

    if (bytes != nullptr)
    {
        items.push_back ({ L"Copy sector bytes", [this, bytes] { DxuiClipboard::SetText (m_host.GetOwnerHwnd(), DiskLogPane::GetHexDump (*bytes)); } });
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogController::ApplyLogDeltas
//
//  Each frame: the deltas the machine's thread sent, whether the pane is
//  open, the machine's position while it is behind live, then the rows,
//  and the request for older rows when the oldest row is in view. The line
//  under the bar says when the machine has no Disk II controller.
//
////////////////////////////////////////////////////////////////////////////////

void DiskLogController::ApplyLogDeltas (const DebugLogDeltas & deltas, bool isWindowVisible)
{
    const DxuiTheme  * theme   = m_host.GetTheme();
    std::wstring       invalid;



    for (const DiskLogDelta & delta : deltas.disk)
    {
        m_model->Apply (delta);
    }

    m_model->SetShown (isWindowVisible && m_host.IsPaneOpen (m_id));

    if (theme != nullptr)
    {
        uint32_t  row = theme->ContentBackground() | 0xFF000000u;

        m_model->SetSeverityFills (DxuiColor::Mix (row, theme->WarningAccent()   | 0xFF000000u, s_kSeverityTint),
                                   DxuiColor::Mix (row, theme->ErrorForeground() | 0xFF000000u, s_kSeverityTint));
    }

    m_model->SetShownBefore (m_shownBefore);

    m_model->Refresh();
    m_model->FollowScroll();

    invalid = m_hasDisk ? std::wstring() : std::wstring (L"This machine has no Disk II controller");

    if (invalid != m_error->GetText())
    {
        m_error->SetText (invalid);

        if (m_frame != nullptr)
        {
            m_frame->Relayout();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogController model access
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskLogController::GetModelText (bool isSelectedOnly) const  { return m_model->GetText (isSelectedOnly); }
std::string  DiskLogController::GetOptionsText() const                    { return m_model->GetOptionsText(); }
void         DiskLogController::SetOptionsText (const std::string & text) { m_model->SetOptionsText (text); }
void         DiskLogController::SetModelShown (bool isShown)              { m_model->SetShown (isShown); }
size_t       DiskLogController::GetTextColumn() const                     { return DiskLogPane::kDetail; }
IEventLogFilter * DiskLogController::GetColumnFilters() const { return m_model.get(); }





////////////////////////////////////////////////////////////////////////////////
//
//  InputLogController::InputLogController
//
////////////////////////////////////////////////////////////////////////////////

InputLogController::InputLogController (IDebuggerPaneHost & host) :
    EventLogController (host, DebuggerLayout::kInputLog, L"Input log", DebugLogKind::Input)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputLogController::~InputLogController
//
////////////////////////////////////////////////////////////////////////////////

InputLogController::~InputLogController() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  InputLogController::CreateControls
//
////////////////////////////////////////////////////////////////////////////////

void InputLogController::CreateControls()
{
    CreateListAndBar();

    m_model = std::make_unique<InputLogPane> (m_list, [this] (const std::string & request)
    {
        if (GetWindowHost() != nullptr)
        {
            GetWindowHost()->SendDebuggerEventLogRequest (DebugLogKind::Input, request);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputLogController::Configure
//
////////////////////////////////////////////////////////////////////////////////

void InputLogController::Configure()
{
    m_model->Configure();
    ConfigureColumnFilters();
    (void) LoadOptions();

    //  The Source chevron filters now, so sources an older Casso saved as
    //  hidden show.
    for (size_t lane = 0; lane < InputLogPane::kLaneCount; lane++)
    {
        m_model->SetLaneShown ((InputLogPane::Lane) lane, true);
    }

    ConfigureBar();
    SetBarEntries();
    SetMenus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputLogController::AddToDock
//
////////////////////////////////////////////////////////////////////////////////

void InputLogController::AddToDock (DxuiDockSite & dock)
{
    m_frame = std::make_unique<DebuggerPaneFrame> (m_title);
    m_frame->AddPart (m_bar.GetSlot(), PaneBarSlot::GetHeightPx);
    m_frame->AddPart (m_list);

    dock.AddPane (m_id, m_title, m_frame.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputLogController::GetControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> InputLogController::GetControls() const
{
    return { m_bar.GetSlot(), m_list, m_bar.GetBar() };
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputLogController::SetBarEntries
//
//  Each pair's view, each drop-down reading the view chosen. The columns,
//  and the guest's and the host's input, are chosen from the headings' menu.
//
////////////////////////////////////////////////////////////////////////////////

void InputLogController::SetBarEntries()
{
    std::vector<DxuiToolbar::Entry>  entries;



    m_commands.clear();

    AddEntry (entries, s_kInputPair0, GetPairLabel (0).c_str(), L"View paddles 0 and 1 as joystick 0 or as two paddles", DxuiToolbar::Kind::DropDown, 1);
    AddEntry (entries, s_kInputPair1, GetPairLabel (1).c_str(), L"View paddles 2 and 3 as joystick 1 or as two paddles", DxuiToolbar::Kind::DropDown, 1);

    entries.push_back (m_host.GetFindEntry (m_id));

    m_bar.GetBar()->SetEntries (std::move (entries));
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputLogController::SetMenus
//
////////////////////////////////////////////////////////////////////////////////

void InputLogController::SetMenus()
{
    std::vector<DxuiPopupMenuItem>  pair0;
    std::vector<DxuiPopupMenuItem>  pair1;



    for (int pair = 0; pair < 2; pair++)
    {
        std::vector<DxuiPopupMenuItem>  & menu = (pair == 0) ? pair0 : pair1;

        for (bool isJoystick : { true, false })
        {
            std::wstring  label = isJoystick ? std::format (L"Joystick {}", pair) : std::format (L"Paddles {}, {}", pair * 2, pair * 2 + 1);

            menu.push_back (DxuiPopupMenuItem::ForCommand (DebuggerMenuCommand::Make (label, m_model->IsPairJoystick (pair) == isJoystick, [this, pair, isJoystick]
            {
                m_model->SetPairJoystick (pair, isJoystick);
                SaveOptions();
                SetBarEntries();
            })));
        }
    }

    m_bar.GetBar()->SetDropDownItems (s_kInputPair0, std::move (pair0));
    m_bar.GetBar()->SetDropDownItems (s_kInputPair1, std::move (pair1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputLogController::ApplyLogDeltas
//
//  Each frame: the deltas, whether the pane is open, the machine's position
//  while it is behind live, whether $C063 is the //c's mouse button, then
//  the rows.
//
////////////////////////////////////////////////////////////////////////////////

void InputLogController::ApplyLogDeltas (const DebugLogDeltas & deltas, bool isWindowVisible)
{
    for (const InputLogDelta & delta : deltas.input)
    {
        m_model->Apply (delta);
    }

    m_model->SetShown             (isWindowVisible && m_host.IsPaneOpen (m_id));
    m_model->SetShownBefore       (m_shownBefore);
    m_model->SetMouseButtonAtC063 (m_isTwoC);
    m_model->Refresh();
    m_model->FollowScroll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputLogController::GetPairLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InputLogController::GetPairLabel (int pair) const
{
    return m_model->IsPairJoystick (pair) ? std::format (L"Joystick {}", pair) : std::format (L"Paddles {}, {}", pair * 2, pair * 2 + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputLogController model access
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InputLogController::GetModelText (bool isSelectedOnly) const  { return m_model->GetText (isSelectedOnly); }
std::string  InputLogController::GetOptionsText() const                    { return m_model->GetOptionsText(); }
void         InputLogController::SetOptionsText (const std::string & text) { m_model->SetOptionsText (text); }
void         InputLogController::SetModelShown (bool isShown)              { m_model->SetShown (isShown); }
size_t       InputLogController::GetTextColumn() const                     { return InputLogPane::kMeaning; }
IEventLogFilter * InputLogController::GetColumnFilters() const { return m_model.get(); }





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogController::IoLogController
//
////////////////////////////////////////////////////////////////////////////////

IoLogController::IoLogController (IDebuggerPaneHost & host, DebugLogKind kind) :
    EventLogController (host,
                        (kind == DebugLogKind::Video) ? DebuggerLayout::kVideoLog : DebuggerLayout::kSoundLog,
                        (kind == DebugLogKind::Video) ? L"Video log" : L"Sound log",
                        kind)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogController::~IoLogController
//
////////////////////////////////////////////////////////////////////////////////

IoLogController::~IoLogController() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogController::CreateControls
//
////////////////////////////////////////////////////////////////////////////////

void IoLogController::CreateControls()
{
    CreateListAndBar();

    m_model = std::make_unique<IoLogPane> (m_list, [this] (const std::string & request)
    {
        if (GetWindowHost() != nullptr)
        {
            GetWindowHost()->SendDebuggerEventLogRequest (m_kind, request);
        }
    }, IsVideo());
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogController::Configure
//
////////////////////////////////////////////////////////////////////////////////

void IoLogController::Configure()
{
    m_model->Configure();
    ConfigureColumnFilters();
    (void) LoadOptions();

    //  The Source chevron filters now, so sources an older Casso saved as
    //  hidden show.
    for (size_t lane = 0; lane < IoLogPane::kLaneCount; lane++)
    {
        m_model->SetLaneShown ((IoLogPane::Lane) lane, true);
    }

    ConfigureBar();
    SetBarEntries();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogController::AddToDock
//
////////////////////////////////////////////////////////////////////////////////

void IoLogController::AddToDock (DxuiDockSite & dock)
{
    m_frame = std::make_unique<DebuggerPaneFrame> (m_title);
    m_frame->AddPart (m_bar.GetSlot(), PaneBarSlot::GetHeightPx);
    m_frame->AddPart (m_list);

    dock.AddPane (m_id, m_title, m_frame.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogController::GetControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> IoLogController::GetControls() const
{
    return { m_bar.GetSlot(), m_list, m_bar.GetBar() };
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogController::SetBarEntries
//
//  Clear and Save, as glyphs, as the profiler's Reset and Save are.
//
////////////////////////////////////////////////////////////////////////////////

void IoLogController::SetBarEntries()
{
    std::vector<DxuiToolbar::Entry>  entries;
    std::shared_ptr<DxuiCommand>     command;



    m_commands.clear();

    command           = AddEntry (entries, s_kIoLogClear, L"Clear", IsVideo() ? L"Hide the video log's entries up to now"
                                                                              : L"Hide the sound log's entries up to now",
                                  DxuiToolbar::Kind::Command, 0);
    command->glyph    = s_kpszMdl2Broom;
    command->dispatch = [this] { Clear(); };
    entries.back().iconOnly = true;

    command           = AddEntry (entries, s_kIoLogSave, L"Save...", L"Save the log's entries to a text file", DxuiToolbar::Kind::Command, 0);
    command->glyph    = s_kpszMdl2Save;
    command->dispatch = [this] { Save(); };
    entries.back().iconOnly = true;

    entries.push_back (m_host.GetFindEntry (m_id));

    m_bar.GetBar()->SetEntries (std::move (entries));
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogController::Clear
//
//  As VIDEOLOG or SOUNDLOG CLEAR: the entries until now are hidden from the
//  command and the pane alike, and the pane starts again from here.
//
////////////////////////////////////////////////////////////////////////////////

void IoLogController::Clear()
{
    m_host.RunAction (DebuggerActions::GetIoLog (IsVideo(), "CLEAR", m_host.GetMode()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogController::Save
//
//  As VIDEOLOG or SOUNDLOG SAVE, to a file chosen here: the same text the
//  command writes, every entry since the log was cleared.
//
////////////////////////////////////////////////////////////////////////////////

void IoLogController::Save()
{
    HRESULT                hr      = S_OK;
    IHostDialogs         * dialogs = m_host.GetHostDialogs();
    FileDialogSpec         spec;
    std::filesystem::path  chosen;
    bool                   picked  = false;



    BAIL_OUT_IF (dialogs == nullptr, S_OK);

    spec.filters          = { { L"Text files", L"*.txt" }, { L"All files", L"*.*" } };
    spec.defaultExtension = L"txt";
    spec.defaultFileName  = IsVideo() ? L"VideoLog.txt" : L"SoundLog.txt";

    hr = dialogs->PickFileToSave (m_host.GetOwnerHwnd(), spec, chosen, picked);
    CHR (hr);
    BAIL_OUT_IF (!picked, S_OK);

    m_host.RunAction (DebuggerActions::GetIoLog (IsVideo(), std::format ("SAVE \"{}\"", TextEncoding::WideToNarrow (chosen.wstring())), m_host.GetMode()));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogController::ApplyLogDeltas
//
////////////////////////////////////////////////////////////////////////////////

void IoLogController::ApplyLogDeltas (const DebugLogDeltas & deltas, bool isWindowVisible)
{
    for (const IoLogDelta & delta : deltas.GetIo (m_kind))
    {
        m_model->Apply (delta);
    }

    m_model->SetShown       (isWindowVisible && m_host.IsPaneOpen (m_id));
    m_model->SetShownBefore (m_shownBefore);
    m_model->Refresh();
    m_model->FollowScroll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogController model access
//
////////////////////////////////////////////////////////////////////////////////

std::wstring IoLogController::GetModelText (bool isSelectedOnly) const  { return m_model->GetText (isSelectedOnly); }
std::string  IoLogController::GetOptionsText() const                    { return m_model->GetOptionsText(); }
void         IoLogController::SetOptionsText (const std::string & text) { m_model->SetOptionsText (text); }
void         IoLogController::SetModelShown (bool isShown)              { m_model->SetShown (isShown); }
size_t       IoLogController::GetTextColumn() const                     { return IoLogPane::kEvent; }
IEventLogFilter * IoLogController::GetColumnFilters() const { return m_model.get(); }
