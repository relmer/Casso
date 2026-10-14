#include "Pch.h"

#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"





//  The event log bars' entries.
static constexpr int  s_kDiskEvents        = 1;
static constexpr int  s_kDiskDrive         = 2;
static constexpr int  s_kDiskSeverity      = 3;
static constexpr int  s_kInputGuest        = 6;
static constexpr int  s_kInputHost         = 7;
static constexpr int  s_kInputPair0        = 8;
static constexpr int  s_kInputPair1        = 9;

//  How far a warning or error row's fill is from the list's background
//  toward the theme's warning or error color.
static constexpr float  s_kSeverityTint = 0.25f;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CreateEventLogs
//
//  Each bar before its boxes, so the boxes paint over the strip.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::CreateEventLogs()
{
    m_diskLog.list   = CreateChild<DxuiListView>  ();
    m_diskLog.bar    = CreateChild<DxuiToolbar>   ();
    m_diskTrackBox   = CreateChild<DxuiTextInput> ();
    m_diskSectorBox  = CreateChild<DxuiTextInput> ();
    m_diskLogError   = CreateChild<DxuiLabel>     ();
    m_inputLog.list  = CreateChild<DxuiListView>  ();
    m_inputLog.bar   = CreateChild<DxuiToolbar>   ();

    m_diskLogPane  = std::make_unique<DiskLogPane>  (m_diskLog.list,  [this] (const std::string & request) { if (m_host != nullptr) { m_host->SendDebuggerEventLogRequest (DebugLogKind::Disk,  request); } });
    m_inputLogPane = std::make_unique<InputLogPane> (m_inputLog.list, [this] (const std::string & request) { if (m_host != nullptr) { m_host->SendDebuggerEventLogRequest (DebugLogKind::Input, request); } });

    m_diskTrackEntry  = std::make_unique<ToolbarBoxEntry> (m_diskTrackBox,  L"Track:",  L"Tracks to list, as 17, 3-5 or 17.25 for a quarter-track; empty is every track", kTrackLabelDip,  kEventLogBoxDip);
    m_diskSectorEntry = std::make_unique<ToolbarBoxEntry> (m_diskSectorBox, L"Sector:", L"Sectors to list, as 0 or 3-5; empty is every sector",                               kSectorLabelDip, kEventLogBoxDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureEventLogs
//
//  The panes' filters and columns, and whether each was ever opened, come
//  back from the host's options text.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureEventLogs()
{
    std::string  disk  = (m_host != nullptr) ? m_host->GetDebuggerEventLogOptions (DebugLogKind::Disk)  : std::string();
    std::string  input = (m_host != nullptr) ? m_host->GetDebuggerEventLogOptions (DebugLogKind::Input) : std::string();



    m_diskLogPane->Configure();
    m_inputLogPane->Configure();

    m_diskLogPane->SetOptionsText  (disk);
    m_inputLogPane->SetOptionsText (input);

    m_diskLog.isOpened  = disk.find ("open=1")  != std::string::npos;
    m_inputLog.isOpened = input.find ("open=1") != std::string::npos;

    for (DxuiTextInput * box : { m_diskTrackBox, m_diskSectorBox })
    {
        box->SetHwnd        (GetHwnd());
        box->SetMaxLength   (64);
        box->SetPlaceholder (L"all");
    }

    m_diskTrackBox->SetText  (m_diskLogPane->GetTrackText());
    m_diskSectorBox->SetText (m_diskLogPane->GetSectorText());

    m_diskLogError->SetTextRole  (DxuiTextRole::Error);
    m_diskLogError->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    m_diskLogError->SetText      (m_diskLogPane->GetInvalidText());

    for (DxuiToolbar * bar : { m_diskLog.bar, m_inputLog.bar })
    {
        bar->SetTextRenderer  (GetTextRenderer());
        bar->SetPopupHost     (GetPopupHost());
        bar->SetIconFace      (DxuiToolbar::kMdl2IconFace);
        bar->SetCompact       (true);
        bar->SetPaneTextInset (true);
        bar->EnableSeeMore    (s_kpszMdl2More, L"See more");
        bar->SetVisible       (false);
    }

    SetEventLogBarEntries();
    SetEventLogMenus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetEventLogBarEntries
//
//  The Disk II log's bar: its kinds, its drive, its severities, and its track
//  and sector boxes. The input log's: the guest's and the host's input and each pair's
//  view. The columns are chosen from the heading's menu.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetEventLogBarEntries()
{
    std::vector<DxuiToolbar::Entry>  disk;
    std::vector<DxuiToolbar::Entry>  input;
    auto                             add  = [] (EventLogWidgets & log, std::vector<DxuiToolbar::Entry> & entries, int id, const wchar_t * label,
                                                const wchar_t * tip, DxuiToolbar::Kind kind, int group, std::function<void()> dispatch)
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

        log.commands.push_back (command);
        entries.push_back (std::move (entry));

        return command;
    };



    m_diskLog.commands.clear();
    m_inputLog.commands.clear();

    add (m_diskLog, disk, s_kDiskEvents, L"Events", L"Choose the kinds of event in this log",  DxuiToolbar::Kind::DropDown, 0, [] {});
    add (m_diskLog, disk, s_kDiskDrive,    L"Drive",    L"Choose the drive whose events are in this log",  DxuiToolbar::Kind::DropDown, 0, [] {});
    add (m_diskLog, disk, s_kDiskSeverity, L"Severity", L"Choose the severities of event in this log",     DxuiToolbar::Kind::DropDown, 0, [] {});

    for (ToolbarBoxEntry * entry : { m_diskTrackEntry.get(), m_diskSectorEntry.get() })
    {
        DxuiToolbar::Entry  box;

        box.custom        = entry;
        box.group         = 1;
        box.neverOverflow = true;

        disk.push_back (std::move (box));
    }


    add (m_inputLog, input, s_kInputGuest,   L"Emulator input",      L"Choose the guest's reads in this log",            DxuiToolbar::Kind::DropDown, 0, [] {});
    add (m_inputLog, input, s_kInputHost,    L"Host input",          L"Choose the host's input in this log",             DxuiToolbar::Kind::DropDown, 0, [] {});
    add (m_inputLog, input, s_kInputPair0,   L"PADDL0-PADDL1 as",    L"View paddles 0 and 1 as joystick 0 or as two paddles", DxuiToolbar::Kind::DropDown, 1, [] {});
    add (m_inputLog, input, s_kInputPair1,   L"PADDL2-PADDL3 as",    L"View paddles 2 and 3 as joystick 1 or as two paddles", DxuiToolbar::Kind::DropDown, 1, [] {});

    m_diskLog.bar->SetEntries  (std::move (disk));
    m_inputLog.bar->SetEntries (std::move (input));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetEventLogMenus
//
//  Built again before each drop-down opens, since the rows have the checks
//  they were built with.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetEventLogMenus()
{
    std::vector<DxuiPopupMenuItem>  kinds;
    std::vector<DxuiPopupMenuItem>  drives;
    std::vector<DxuiPopupMenuItem>  severities;
    std::vector<DxuiPopupMenuItem>  guest;
    std::vector<DxuiPopupMenuItem>  host;
    std::vector<DxuiPopupMenuItem>  pair0;
    std::vector<DxuiPopupMenuItem>  pair1;
    size_t                          i     = 0;
    bool                            isAll = true;
    std::shared_ptr<DxuiCommand>    item;



    for (i = 0; i < DiskLogPane::kKindCount; i++)
    {
        isAll = isAll && m_diskLogPane->IsKindShown ((DiskLogPane::Kind) i);
    }

    kinds.push_back (DxuiPopupMenuItem::ForCommand (MakeMenuCommand (L"All", isAll, [this, isAll]
    {
        for (size_t kind = 0; kind < DiskLogPane::kKindCount; kind++)
        {
            m_diskLogPane->SetKindShown ((DiskLogPane::Kind) kind, !isAll);
        }

        SaveEventLogOptions (DebugLogKind::Disk);
    })));

    for (i = 0; i < DiskLogPane::kKindCount; i++)
    {
        DiskLogPane::Kind  kind = (DiskLogPane::Kind) i;

        item      = MakeMenuCommand (DiskLogPane::GetKindLabel (kind), m_diskLogPane->IsKindShown (kind), [this, kind]
        {
            m_diskLogPane->SetKindShown (kind, !m_diskLogPane->IsKindShown (kind));
            SaveEventLogOptions (DebugLogKind::Disk);
        });
        item->tip = DiskLogPane::GetKindTip (kind);

        kinds.push_back (DxuiPopupMenuItem::ForCommand (item));
    }

    for (int drive = 0; drive <= 2; drive++)
    {
        drives.push_back (DxuiPopupMenuItem::ForCommand (MakeMenuCommand ((drive == 0) ? L"All drives" : std::format (L"Drive {}", drive), m_diskLogPane->GetDrive() == drive, [this, drive]
        {
            m_diskLogPane->SetDrive (drive);
            SaveEventLogOptions (DebugLogKind::Disk);
        })));
    }

    for (i = 0; i < DiskLogPane::kSeverityCount; i++)
    {
        DiskLogPane::Severity  severity = (DiskLogPane::Severity) i;

        severities.push_back (DxuiPopupMenuItem::ForCommand (MakeMenuCommand (DiskLogPane::GetSeverityLabel (severity), m_diskLogPane->IsSeverityShown (severity), [this, severity]
        {
            m_diskLogPane->SetSeverityShown (severity, !m_diskLogPane->IsSeverityShown (severity));
            SaveEventLogOptions (DebugLogKind::Disk);
        })));
    }

    for (InputLogPane::Lane lane : { InputLogPane::kGuestKeyboard, InputLogPane::kJoystick, InputLogPane::kPaddle, InputLogPane::kHostKeyboard, InputLogPane::kHostMouse })
    {
        bool  isGuest = lane == InputLogPane::kGuestKeyboard || lane == InputLogPane::kJoystick || lane == InputLogPane::kPaddle;

        item      = MakeMenuCommand (InputLogPane::GetLaneLabel (lane), m_inputLogPane->IsLaneShown (lane), [this, lane]
        {
            m_inputLogPane->SetLaneShown (lane, !m_inputLogPane->IsLaneShown (lane));
            SaveEventLogOptions (DebugLogKind::Input);
        });
        item->tip = InputLogPane::GetLaneTip (lane);

        //  Joystick applies while a pair is viewed as one, Paddle while a pair
        //  is viewed as paddles.
        if (lane == InputLogPane::kJoystick)
        {
            item->isEnabled = [this] { return m_inputLogPane->IsPairJoystick (0) || m_inputLogPane->IsPairJoystick (1); };
        }
        else if (lane == InputLogPane::kPaddle)
        {
            item->isEnabled = [this] { return !m_inputLogPane->IsPairJoystick (0) || !m_inputLogPane->IsPairJoystick (1); };
        }

        (isGuest ? guest : host).push_back (DxuiPopupMenuItem::ForCommand (item));
    }

    for (int pair = 0; pair < 2; pair++)
    {
        std::vector<DxuiPopupMenuItem>  & menu = (pair == 0) ? pair0 : pair1;

        for (bool isJoystick : { true, false })
        {
            std::wstring  label = isJoystick ? std::format (L"Joystick {}", pair) : std::format (L"Paddles {}, {}", pair * 2, pair * 2 + 1);

            menu.push_back (DxuiPopupMenuItem::ForCommand (MakeMenuCommand (label, m_inputLogPane->IsPairJoystick (pair) == isJoystick, [this, pair, isJoystick]
            {
                m_inputLogPane->SetPairJoystick (pair, isJoystick);
                SaveEventLogOptions (DebugLogKind::Input);
            })));
        }
    }

    m_diskLog.bar->SetDropDownItems  (s_kDiskEvents,  std::move (kinds));
    m_diskLog.bar->SetDropDownItems  (s_kDiskDrive,    std::move (drives));
    m_diskLog.bar->SetDropDownItems  (s_kDiskSeverity, std::move (severities));
    m_inputLog.bar->SetDropDownItems (s_kInputGuest,  std::move (guest));
    m_inputLog.bar->SetDropDownItems (s_kInputHost,   std::move (host));
    m_inputLog.bar->SetDropDownItems (s_kInputPair0,  std::move (pair0));
    m_inputLog.bar->SetDropDownItems (s_kInputPair1,  std::move (pair1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceEventLogBars
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceEventLogBars()
{
    PlaceEventLogBar (m_diskLog,  DebuggerLayout::kDiskLog);
    PlaceEventLogBar (m_inputLog, DebuggerLayout::kInputLog);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceEventLogBar
//
//  The bar is laid out in the place at the top of its pane and moves with
//  the pane into a floating window, as the breakpoints bar does; the Disk II
//  log's boxes move with it. When a box is hidden with its pane, the keys go
//  to the command line.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceEventLogBar (
    EventLogWidgets       & log,
    const std::wstring    & pane)
{
    bool          shown  = log.slot != nullptr && log.slot->IsVisible();
    bool          isDisk = &log == &m_diskLog;
    DxuiWindow  * host   = GetPaneHost (pane);
    RECT          slot   = {};



    log.bar->SetVisible (shown);

    if (isDisk)
    {
        for (DxuiTextInput * box : { m_diskTrackBox, m_diskSectorBox })
        {
            box->SetVisible (shown);

            if (!shown && m_focusMgr.GetFocusedControl() == box)
            {
                m_focusMgr.SetFocused (m_commandBox);
            }
        }
    }

    if (!shown)
    {
        return;
    }

    slot = log.slot->GetBounds();

    log.bar->SetTextRenderer   (host->GetTextRenderer());
    log.bar->SetPopupHost      (host->GetPopupHost());
    log.bar->SetHostClientRect (host->GetBounds());
    log.bar->Layout            (GetBarStrip (pane, slot), m_scaler);

    host->SetChildClip (log.bar, slot);

    if (isDisk)
    {
        host->SetChildClip (m_diskTrackBox,  slot);
        host->SetChildClip (m_diskSectorBox, slot);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteEventLogBarMouse
//
//  As the breakpoints bar's: the strip and whatever menu it has open take
//  the left button. The Disk II log's boxes are the window's own controls
//  and take their presses as every box does.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteEventLogBarMouse (const DxuiMouseEvent & ev)
{
    int   x = ev.positionDip.x;
    int   y = ev.positionDip.y;



    for (auto [log, pane] : { std::pair { &m_diskLog, DebuggerLayout::kDiskLog }, std::pair { &m_inputLog, DebuggerLayout::kInputLog } })
    {
        DxuiToolbar  * bar   = log->bar;
        bool           open  = bar->IsMenuOpen();
        bool           inBox = log == &m_diskLog &&
                               ((m_diskTrackBox->IsVisible()  && DxuiDockSite::Contains (m_diskTrackBox->GetBounds(),  POINT { x, y })) ||
                                (m_diskSectorBox->IsVisible() && DxuiDockSite::Contains (m_diskSectorBox->GetBounds(), POINT { x, y })));
        bool           over  = bar->IsVisible() && DxuiDockSite::Contains (bar->GetBounds(), POINT { x, y }) && !inBox;

        if (m_routingPane != GetBarRoutingPane (pane))
        {
            continue;
        }

        if (!over && !open)
        {
            bar->OnToolbarMouseLeave();
            continue;
        }

        if ((ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Up) && ev.button != DxuiMouseButton::Left && !open)
        {
            return false;
        }

        switch (ev.kind)
        {
        case DxuiMouseEventKind::Move:
            UpdateTooltip (ev.positionDip);

            return bar->OnToolbarMouseMove (x, y);

        case DxuiMouseEventKind::Down:
            if (!open)
            {
                SetEventLogMenus();
            }

            return bar->OnToolbarLButtonDown (x, y);

        case DxuiMouseEventKind::Up:
            return bar->OnToolbarLButtonUp (x, y);

        default:
            return open;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyEventLogs
//
//  Each frame: the deltas the machine's thread sent, whether each pane is
//  open, the machine's position while it is behind live, the boxes' text,
//  then each pane's rows, and the request for older rows when a pane's
//  oldest row is in view. A machine with no Disk II panel has no Disk II
//  controller.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyEventLogs()
{
    DebugLogDeltas  deltas;
    uint64_t        before  = UINT64_MAX;
    bool            isTwoC  = false;
    bool            isDisk  = true;
    std::wstring    invalid;



    if (m_host == nullptr || m_diskLogPane == nullptr)
    {
        return;
    }

    m_host->TakeDebuggerEventLogs (deltas);

    for (const DiskLogDelta & delta : deltas.disk)
    {
        m_diskLogPane->Apply (delta);
    }

    for (const InputLogDelta & delta : deltas.input)
    {
        m_inputLogPane->Apply (delta);
    }

    m_diskLogPane->SetShown  (IsVisible() && IsPaneShown (DebuggerLayout::kDiskLog));
    m_inputLogPane->SetShown (IsVisible() && IsPaneShown (DebuggerLayout::kInputLog));

    if (m_snapshot != nullptr)
    {
        before = m_snapshot->history.isBehindLive ? m_snapshot->history.position : UINT64_MAX;
        isTwoC = m_snapshot->machine.find ("2c") != std::string::npos || m_snapshot->machine.find ("//c") != std::string::npos;
        isDisk = std::ranges::any_of (m_snapshot->panels, [] (const DebuggerViewSnapshot::PanelInfo & panel) { return panel.id == "disk"; });
    }

    if (m_theme != nullptr)
    {
        uint32_t  row = m_theme->ContentBackground() | 0xFF000000u;

        m_diskLogPane->SetSeverityFills (DxuiColor::Mix (row, m_theme->WarningAccent()   | 0xFF000000u, s_kSeverityTint),
                                         DxuiColor::Mix (row, m_theme->ErrorForeground() | 0xFF000000u, s_kSeverityTint));
    }

    m_diskLogPane->SetShownBefore        (before);
    m_inputLogPane->SetShownBefore       (before);
    m_inputLogPane->SetMouseButtonAtC063 (isTwoC);

    ReadEventLogBoxes();

    m_diskLogPane->Refresh();
    m_inputLogPane->Refresh();

    m_diskLogPane->FollowScroll();
    m_inputLogPane->FollowScroll();

    //  The line under the bar is that the machine has no Disk II, or the
    //  filter tokens not taken.
    invalid = isDisk ? m_diskLogPane->GetInvalidText() : std::wstring (L"This machine has no Disk II controller");

    if (invalid != m_diskLogError->GetText())
    {
        m_diskLogError->SetTextRole (isDisk ? DxuiTextRole::Error : DxuiTextRole::Muted);
        m_diskLogError->SetText     (invalid);

        if (m_diskLog.frame != nullptr)
        {
            m_diskLog.frame->Relayout();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ReadEventLogBoxes
//
//  Every keystroke in a box filters again.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ReadEventLogBoxes()
{
    bool  isChanged = false;



    if (m_diskTrackBox->GetText() != m_diskLogPane->GetTrackText())
    {
        m_diskLogPane->SetTrackText (m_diskTrackBox->GetText());
        isChanged = true;
    }

    if (m_diskSectorBox->GetText() != m_diskLogPane->GetSectorText())
    {
        m_diskLogPane->SetSectorText (m_diskSectorBox->GetText());
        isChanged = true;
    }

    if (isChanged)
    {
        SaveEventLogOptions (DebugLogKind::Disk);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SaveEventLogOptions
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SaveEventLogOptions (DebugLogKind kind)
{
    bool          isDisk   = kind == DebugLogKind::Disk;
    bool          isOpened = isDisk ? m_diskLog.isOpened : m_inputLog.isOpened;
    std::string   text     = isDisk ? m_diskLogPane->GetOptionsText() : m_inputLogPane->GetOptionsText();



    if (m_host != nullptr)
    {
        m_host->SetDebuggerEventLogOptions (kind, text + (isOpened ? ";open=1" : ""));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CopyEventLog
//
//  An event log's selected rows, or every row in its list, to the clipboard.
//  False when the control is neither log's list.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::CopyEventLog (
    IDxuiControl  * focused,
    bool            isAll)
{
    std::wstring  text;



    if (focused == m_diskLog.list)
    {
        text = m_diskLogPane->GetText (!isAll);
    }
    else if (focused == m_inputLog.list)
    {
        text = m_inputLogPane->GetText (!isAll);
    }
    else
    {
        return false;
    }

    DxuiClipboard::SetText (GetHwnd(), text);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OpenEventLog
//
//  The pane in front, opened for good: it comes back open with the window.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OpenEventLog (DebugLogKind kind)
{
    ShowPane ((kind == DebugLogKind::Disk) ? DebuggerLayout::kDiskLog : DebuggerLayout::kInputLog);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetEventLogOf
//
////////////////////////////////////////////////////////////////////////////////

DebuggerWindow::EventLogWidgets * DebuggerWindow::GetEventLogOf (const std::wstring & pane)
{
    if (pane == DebuggerLayout::kDiskLog)
    {
        return &m_diskLog;
    }

    if (pane == DebuggerLayout::kInputLog)
    {
        return &m_inputLog;
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetEventLogOf
//
////////////////////////////////////////////////////////////////////////////////

const DebuggerWindow::EventLogWidgets * DebuggerWindow::GetEventLogOf (const std::wstring & pane) const
{
    if (pane == DebuggerLayout::kDiskLog)
    {
        return &m_diskLog;
    }

    if (pane == DebuggerLayout::kInputLog)
    {
        return &m_inputLog;
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::AddEventLogMenuItems
//
//  An event log's menu on its rows: Copy for the selected rows, Copy all
//  rows, and on a sector's row Copy sector bytes, as its hex dump.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::AddEventLogMenuItems (
    DxuiListView                                                   * list,
    int                                                              row,
    std::vector<std::pair<std::wstring, std::function<void()>>>    & items)
{
    std::shared_ptr<const std::vector<Byte>>  bytes = (list == m_diskLog.list) ? m_diskLogPane->GetRowData (row) : nullptr;



    items.push_back ({ L"Copy",          [this, list] { CopyEventLog (list, false); } });
    items.push_back ({ L"Copy all rows", [this, list] { CopyEventLog (list, true);  } });

    if (bytes != nullptr)
    {
        items.push_back ({ L"Copy sector bytes", [this, bytes] { DxuiClipboard::SetText (GetHwnd(), DiskLogPane::GetHexDump (*bytes)); } });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::AddEventLogColumnItems
//
//  An event log's menu on its headings: a check for each column. The text
//  column is always in the list, so its check is dimmed.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::AddEventLogColumnItems (
    DxuiListView                                * list,
    std::vector<std::shared_ptr<DxuiCommand>>   & commands)
{
    bool          isDisk = list == m_diskLog.list;
    DebugLogKind  kind   = isDisk ? DebugLogKind::Disk : DebugLogKind::Input;
    size_t        text   = isDisk ? (size_t) DiskLogPane::kDetail : (size_t) InputLogPane::kMeaning;
    size_t        column = 0;



    for (column = 0; column < list->GetColumnCount(); column++)
    {
        size_t  each = column;

        commands.push_back (MakeMenuCommand (list->GetColumnAt (column).title, list->IsColumnVisible (column), [this, list, each, kind]
        {
            list->SetColumnVisible (each, !list->IsColumnVisible (each));
            SaveEventLogOptions (kind);
        }));

        if (column == text)
        {
            commands.back()->isEnabled = [] { return false; };
        }
    }
}