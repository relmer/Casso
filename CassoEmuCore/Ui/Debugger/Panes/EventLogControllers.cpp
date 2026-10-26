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
static constexpr int  s_kDiskEvents        = 1;
static constexpr int  s_kDiskDrive         = 2;
static constexpr int  s_kDiskSeverity      = 3;
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
        windowHost->SetDebuggerEventLogOptions (m_kind, GetOptionsText() + (m_isOpened ? ";open=1" : ""));
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

    m_trackBox  = parent.CreateChild<DxuiTextInput>();
    m_sectorBox = parent.CreateChild<DxuiTextInput>();
    m_error     = parent.CreateChild<DxuiLabel>();

    m_model = std::make_unique<DiskLogPane> (m_list, [this] (const std::string & request)
    {
        if (GetWindowHost() != nullptr)
        {
            GetWindowHost()->SendDebuggerEventLogRequest (DebugLogKind::Disk, request);
        }
    });

    m_trackEntry  = std::make_unique<ToolbarBoxEntry> (m_trackBox,  L"Track:",  L"Tracks to list, as 17, 3-5 or 17.25 for a quarter-track; empty is every track", kTrackLabelDip,  kBoxDip);
    m_sectorEntry = std::make_unique<ToolbarBoxEntry> (m_sectorBox, L"Sector:", L"Sectors to list, as 0 or 3-5; empty is every sector",                               kSectorLabelDip, kBoxDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogController::Configure
//
////////////////////////////////////////////////////////////////////////////////

void DiskLogController::Configure()
{
    m_model->Configure();
    (void) LoadOptions();

    for (DxuiTextInput * box : { m_trackBox, m_sectorBox })
    {
        box->SetHwnd        (m_host.GetOwnerHwnd());
        box->SetMaxLength   (64);
        box->SetPlaceholder (L"all");
    }

    m_trackBox->SetText  (m_model->GetTrackText());
    m_sectorBox->SetText (m_model->GetSectorText());

    m_error->SetTextRole  (DxuiTextRole::Error);
    m_error->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    m_error->SetText      (m_model->GetInvalidText());

    ConfigureBar();
    SetBarEntries();
    SetMenus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogController::AddToDock
//
//  The bar over the rows, and between them a line listing the filter tokens
//  not taken, only when there are any.
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
    return { m_bar.GetSlot(), m_error, m_list, m_bar.GetBar(), m_trackBox, m_sectorBox };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogController::SetBarEntries
//
//  Its kinds, its drive, its severities, and its track and sector boxes. The
//  columns are chosen from the heading's menu.
//
////////////////////////////////////////////////////////////////////////////////

void DiskLogController::SetBarEntries()
{
    std::vector<DxuiToolbar::Entry>  entries;



    m_commands.clear();

    AddEntry (entries, s_kDiskEvents,   L"Events",   L"Choose the kinds of event in this log",         DxuiToolbar::Kind::DropDown, 0);
    AddEntry (entries, s_kDiskDrive,    L"Drive",    L"Choose the drive whose events are in this log", DxuiToolbar::Kind::DropDown, 0);
    AddEntry (entries, s_kDiskSeverity, L"Severity", L"Choose the severities of event in this log",    DxuiToolbar::Kind::DropDown, 0);

    for (ToolbarBoxEntry * entry : { m_trackEntry.get(), m_sectorEntry.get() })
    {
        DxuiToolbar::Entry  box;

        box.custom        = entry;
        box.group         = 1;
        box.neverOverflow = true;

        entries.push_back (std::move (box));
    }

    m_bar.GetBar()->SetEntries (std::move (entries));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogController::SetMenus
//
////////////////////////////////////////////////////////////////////////////////

void DiskLogController::SetMenus()
{
    std::vector<DxuiPopupMenuItem>  kinds;
    std::vector<DxuiPopupMenuItem>  drives;
    std::vector<DxuiPopupMenuItem>  severities;
    size_t                          i     = 0;
    bool                            isAll = true;
    std::shared_ptr<DxuiCommand>    item;



    for (i = 0; i < DiskLogPane::kKindCount; i++)
    {
        isAll = isAll && m_model->IsKindShown ((DiskLogPane::Kind) i);
    }

    kinds.push_back (DxuiPopupMenuItem::ForCommand (DebuggerMenuCommand::Make (L"All", isAll, [this, isAll]
    {
        for (size_t kind = 0; kind < DiskLogPane::kKindCount; kind++)
        {
            m_model->SetKindShown ((DiskLogPane::Kind) kind, !isAll);
        }

        SaveOptions();
    })));

    for (i = 0; i < DiskLogPane::kKindCount; i++)
    {
        DiskLogPane::Kind  kind = (DiskLogPane::Kind) i;

        item      = DebuggerMenuCommand::Make (DiskLogPane::GetKindLabel (kind), m_model->IsKindShown (kind), [this, kind]
        {
            m_model->SetKindShown (kind, !m_model->IsKindShown (kind));
            SaveOptions();
        });
        item->tip = DiskLogPane::GetKindTip (kind);

        kinds.push_back (DxuiPopupMenuItem::ForCommand (item));
    }

    for (int drive = 0; drive <= 2; drive++)
    {
        drives.push_back (DxuiPopupMenuItem::ForCommand (DebuggerMenuCommand::Make ((drive == 0) ? L"All drives" : std::format (L"Drive {}", drive), m_model->GetDrive() == drive, [this, drive]
        {
            m_model->SetDrive (drive);
            SaveOptions();
        })));
    }

    for (i = 0; i < DiskLogPane::kSeverityCount; i++)
    {
        DiskLogPane::Severity  severity = (DiskLogPane::Severity) i;

        severities.push_back (DxuiPopupMenuItem::ForCommand (DebuggerMenuCommand::Make (DiskLogPane::GetSeverityLabel (severity), m_model->IsSeverityShown (severity), [this, severity]
        {
            m_model->SetSeverityShown (severity, !m_model->IsSeverityShown (severity));
            SaveOptions();
        })));
    }

    m_bar.GetBar()->SetDropDownItems (s_kDiskEvents,   std::move (kinds));
    m_bar.GetBar()->SetDropDownItems (s_kDiskDrive,    std::move (drives));
    m_bar.GetBar()->SetDropDownItems (s_kDiskSeverity, std::move (severities));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogController::PlaceBar
//
//  The bar is laid out in the place at the top of its pane and moves with
//  the pane into a floating window; its boxes move with it. When a box is
//  hidden with its pane, the keys go to the command line.
//
////////////////////////////////////////////////////////////////////////////////

void DiskLogController::PlaceBar()
{
    bool          shown = m_bar.GetSlot()->IsVisible();
    DxuiWindow  * host  = nullptr;
    RECT          slot  = {};



    for (DxuiTextInput * box : { m_trackBox, m_sectorBox })
    {
        box->SetVisible (shown);

        if (!shown && m_host.GetFocusedControl() == box)
        {
            m_host.FocusDefault();
        }
    }

    m_bar.Place();

    if (!shown)
    {
        return;
    }

    host = m_host.GetPaneWindow (m_id);
    slot = m_bar.GetSlot()->GetBounds();

    host->SetChildClip (m_trackBox,  slot);
    host->SetChildClip (m_sectorBox, slot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogController::IsInBox
//
////////////////////////////////////////////////////////////////////////////////

bool DiskLogController::IsInBox (POINT pointDip) const
{
    return (m_trackBox->IsVisible()  && DxuiDockSite::Contains (m_trackBox->GetBounds(),  pointDip)) ||
           (m_sectorBox->IsVisible() && DxuiDockSite::Contains (m_sectorBox->GetBounds(), pointDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogController::RouteBarMouse
//
//  The boxes are controls of their own and take their presses as every box
//  does, so the strip under them is not the bar's.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskLogController::RouteBarMouse (const DxuiMouseEvent & ev)
{
    DxuiToolbar  * bar = m_bar.GetBar();



    if (!m_host.IsBarRouted (m_id))
    {
        return false;
    }

    if (IsInBox (ev.positionDip) && !bar->IsMenuOpen())
    {
        bar->OnToolbarMouseLeave();
        return false;
    }

    return m_bar.RouteMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogController::IsMonospaceTipAt
//
//  A sector's hex dump in the log lines up in columns.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskLogController::IsMonospaceTipAt (POINT clientPx) const
{
    return m_list->IsVisible() && DxuiDockSite::Contains (m_list->GetBounds(), clientPx);
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
//  open, the machine's position while it is behind live, the boxes' text,
//  then the rows, and the request for older rows when the oldest row is in
//  view. The line under the bar lists the filter tokens not taken, or that
//  the machine has no Disk II controller.
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

    ReadBoxes();

    m_model->Refresh();
    m_model->FollowScroll();

    invalid = m_hasDisk ? m_model->GetInvalidText() : std::wstring (L"This machine has no Disk II controller");

    if (invalid != m_error->GetText())
    {
        m_error->SetTextRole (m_hasDisk ? DxuiTextRole::Error : DxuiTextRole::Muted);
        m_error->SetText     (invalid);

        if (m_frame != nullptr)
        {
            m_frame->Relayout();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogController::ReadBoxes
//
//  Every keystroke in a box filters again.
//
////////////////////////////////////////////////////////////////////////////////

void DiskLogController::ReadBoxes()
{
    bool  isChanged = false;



    if (m_trackBox->GetText() != m_model->GetTrackText())
    {
        m_model->SetTrackText (m_trackBox->GetText());
        isChanged = true;
    }

    if (m_sectorBox->GetText() != m_model->GetSectorText())
    {
        m_model->SetSectorText (m_sectorBox->GetText());
        isChanged = true;
    }

    if (isChanged)
    {
        SaveOptions();
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
    (void) LoadOptions();

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

    ShowSourceFilter();

    m_model->SetShown             (isWindowVisible && m_host.IsPaneOpen (m_id));
    m_model->SetShownBefore       (m_shownBefore);
    m_model->SetMouseButtonAtC063 (m_isTwoC);
    m_model->Refresh();
    m_model->FollowScroll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputLogController::GetColumnFilterItems
//
//  Joystick applies while a pair is viewed as one, Paddle while a pair is
//  viewed as paddles.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> InputLogController::GetColumnFilterItems (DxuiListView * list, size_t column, std::vector<std::shared_ptr<DxuiCommand>> & commands)
{
    std::vector<DxuiPopupMenuItem>  items;
    std::shared_ptr<DxuiCommand>    item;



    if (list != m_list || column != InputLogPane::kSource)
    {
        return items;
    }

    for (InputLogPane::Lane lane : { InputLogPane::kGuestKeyboard, InputLogPane::kJoystick, InputLogPane::kPaddle, InputLogPane::kHostKeyboard, InputLogPane::kHostMouse })
    {
        bool          isHost = lane == InputLogPane::kHostKeyboard || lane == InputLogPane::kHostMouse;
        std::wstring  name   = InputLogPane::GetLaneLabel (lane);

        name[0] = towlower (name[0]);
        item    = DebuggerMenuCommand::Make (std::format (L"{} {}", isHost ? L"Host" : L"Guest", name), m_model->IsLaneShown (lane), [this, lane]
        {
            m_model->SetLaneShown (lane, !m_model->IsLaneShown (lane));
            SaveOptions();
            ShowSourceFilter();
        });
        item->tip = InputLogPane::GetLaneTip (lane);

        if (lane == InputLogPane::kJoystick)
        {
            item->isEnabled = [this] { return m_model->IsPairJoystick (0) || m_model->IsPairJoystick (1); };
        }
        else if (lane == InputLogPane::kPaddle)
        {
            item->isEnabled = [this] { return !m_model->IsPairJoystick (0) || !m_model->IsPairJoystick (1); };
        }

        if (lane == InputLogPane::kHostKeyboard)
        {
            items.push_back (DxuiPopupMenuItem::ForSeparator());
        }

        commands.push_back (item);
        items.push_back (DxuiPopupMenuItem::ForCommand (item));
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InputLogController::ShowSourceFilter
//
//  The Source column's heading has the filter funnel while any input is left
//  out of the log.
//
////////////////////////////////////////////////////////////////////////////////

void InputLogController::ShowSourceFilter()
{
    bool  isFiltered = false;



    for (size_t lane = 0; lane < InputLogPane::kLaneCount; lane++)
    {
        isFiltered = isFiltered || !m_model->IsLaneShown ((InputLogPane::Lane) lane);
    }

    m_list->SetColumnFiltered (InputLogPane::kSource, isFiltered);
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
    (void) LoadOptions();

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
//  Clear and Save, as glyphs, as the profiler's Reset and Save are. The
//  sound log's sources are on the Source column's Filter cascade.
//
////////////////////////////////////////////////////////////////////////////////

void IoLogController::SetBarEntries()
{
    std::vector<DxuiToolbar::Entry>  entries;
    std::shared_ptr<DxuiCommand>     command;



    m_commands.clear();

    command           = AddEntry (entries, s_kIoLogClear, L"Clear", IsVideo() ? L"Clear: hide the video log's entries until now"
                                                                              : L"Clear: hide the sound log's entries until now",
                                  DxuiToolbar::Kind::Command, 0);
    command->glyph    = s_kpszMdl2Broom;
    command->dispatch = [this] { Clear(); };
    entries.back().iconOnly = true;

    command           = AddEntry (entries, s_kIoLogSave, L"Save...", L"Save...: save the log's entries to a text file", DxuiToolbar::Kind::Command, 0);
    command->glyph    = s_kpszMdl2Save;
    command->dispatch = [this] { Save(); };
    entries.back().iconOnly = true;

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

    ShowSourceFilter();

    m_model->SetShown       (isWindowVisible && m_host.IsPaneOpen (m_id));
    m_model->SetShownBefore (m_shownBefore);
    m_model->Refresh();
    m_model->FollowScroll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogController::GetColumnFilterItems
//
//  The sound log's sources, on its Source column; the video log has none.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> IoLogController::GetColumnFilterItems (DxuiListView * list, size_t column, std::vector<std::shared_ptr<DxuiCommand>> & commands)
{
    std::vector<DxuiPopupMenuItem>  items;
    std::shared_ptr<DxuiCommand>    item;



    if (list != m_list || column != IoLogPane::kSource || IsVideo())
    {
        return items;
    }

    for (IoLogPane::Lane lane : { IoLogPane::kSpeaker, IoLogPane::kVia, IoLogPane::kAy })
    {
        item = DebuggerMenuCommand::Make (IoLogPane::GetLaneLabel (lane), m_model->IsLaneShown (lane), [this, lane]
        {
            m_model->SetLaneShown (lane, !m_model->IsLaneShown (lane));
            SaveOptions();
            ShowSourceFilter();
        });

        commands.push_back (item);
        items.push_back (DxuiPopupMenuItem::ForCommand (item));
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogController::ShowSourceFilter
//
////////////////////////////////////////////////////////////////////////////////

void IoLogController::ShowSourceFilter()
{
    bool  isFiltered = false;



    for (size_t lane = 0; lane < IoLogPane::kLaneCount; lane++)
    {
        isFiltered = isFiltered || !m_model->IsLaneShown ((IoLogPane::Lane) lane);
    }

    m_list->SetColumnFiltered (IoLogPane::kSource, isFiltered && !IsVideo());
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
