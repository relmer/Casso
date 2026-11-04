#include "Pch.h"

#include "Ui/Debugger/Panes/DevicePanelController.h"

#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/PaneBarSlot.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/DiagnosticsPane.h"
#include "Ui/Debugger/Panes/DiskHeadView.h"
#include "Ui/Debugger/Panes/MemoryMapBar.h"
#include "Ui/Debugger/Panes/MeterBar.h"
#include "Ui/Debugger/Panes/SoundBoardView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::DevicePanelController
//
////////////////////////////////////////////////////////////////////////////////

DevicePanelController::DevicePanelController (IDebuggerPaneHost & host, std::string panelId, std::wstring title) :
    IDebuggerPane (host),
    m_panelId     (std::move (panelId)),
    m_title       (std::move (title))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::~DevicePanelController
//
////////////////////////////////////////////////////////////////////////////////

DevicePanelController::~DevicePanelController() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::GetId
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DevicePanelController::GetId() const
{
    return DebuggerLayout::GetDiagnosticsPaneId (m_panelId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::CreateControls
//
//  Every part a panel can have; the model makes visible the parts its
//  device has.
//
////////////////////////////////////////////////////////////////////////////////

void DevicePanelController::CreateControls()
{
    DxuiPanel       & parent = m_host.GetControlParent();
    DxuiListView    * list   = parent.CreateChild<DxuiListView>();
    MemoryMapBar    * map    = parent.CreateChild<MemoryMapBar>();
    DiskHeadView    * head   = parent.CreateChild<DiskHeadView>();
    MeterBar        * meters = parent.CreateChild<MeterBar>();
    SoundBoardView  * board  = parent.CreateChild<SoundBoardView>();



    m_bar = std::make_unique<PaneBarSlot> (m_host, GetId(), m_title + L" toolbar");
    m_bar->Create();

    m_model = std::make_unique<DiagnosticsPane> (m_panelId, m_title, list, map, head, meters, board, m_bar->GetSlot());
    list->SetVisible (false);
    board->SetFocusable (true);

    board->SetOnMute ([this] (int channel, bool isMuted)
    {
        if (IDebuggerWindowHost * host = m_host.GetWindowHost())
        {
            host->SetDebuggerSoundMuted (channel, isMuted);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::Configure
//
////////////////////////////////////////////////////////////////////////////////

void DevicePanelController::Configure()
{
    IDebuggerWindowHost  * host = m_host.GetWindowHost();



    m_model->Configure();
    m_bar->Configure();

    m_model->GetHead()->SetChoice (FromText ((host != nullptr) ? host->GetDebuggerDiskDrives() : std::string()));
    SetDriveBarEntries();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::SetDriveBarEntries
//
//  The Disk II panel's drives: D1, D2 or both, one of them checked.
//
////////////////////////////////////////////////////////////////////////////////

void DevicePanelController::SetDriveBarEntries()
{
    using Choice = DiskHeadView::DriveChoice;

    static constexpr std::tuple<int, const wchar_t *, const wchar_t *, Choice>  s_kChoices[] =
    {
        { 1, L"D1",   L"Show drive 1",          Choice::Drive1 },
        { 2, L"D2",   L"Show drive 2",          Choice::Drive2 },
        { 3, L"Both", L"Show both drives",      Choice::Both   },
    };

    std::vector<DxuiToolbar::Entry>  entries;



    m_commands.clear();

    for (const auto & [id, label, tip, choice] : s_kChoices)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        DxuiToolbar::Entry            entry;
        Choice                        each    = choice;

        command->id        = id;
        command->label     = label;
        command->tip       = tip;
        command->isChecked = [this, each] { return m_model->GetHead()->GetChoice() == each; };
        command->dispatch  = [this, each] { SetDrives (each); };

        entry.command = command;
        entry.kind    = DxuiToolbar::Kind::Toggle;

        m_commands.push_back (command);
        entries.push_back (std::move (entry));
    }

    m_bar->GetBar()->SetEntries (std::move (entries));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::SetDrives
//
////////////////////////////////////////////////////////////////////////////////

void DevicePanelController::SetDrives (DiskHeadView::DriveChoice choice)
{
    IDebuggerWindowHost  * host = m_host.GetWindowHost();



    m_model->GetHead()->SetChoice (choice);

    if (host != nullptr)
    {
        host->SetDebuggerDiskDrives (ToText (choice));
    }

    m_host.RelayoutDock();
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::ToText
//
////////////////////////////////////////////////////////////////////////////////

std::string DevicePanelController::ToText (DiskHeadView::DriveChoice choice)
{
    switch (choice)
    {
    case DiskHeadView::DriveChoice::Drive1:  return "1";
    case DiskHeadView::DriveChoice::Drive2:  return "2";
    default:                                 return "both";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::FromText
//
////////////////////////////////////////////////////////////////////////////////

DiskHeadView::DriveChoice DevicePanelController::FromText (const std::string & text)
{
    if (text == "1")
    {
        return DiskHeadView::DriveChoice::Drive1;
    }

    if (text == "2")
    {
        return DiskHeadView::DriveChoice::Drive2;
    }

    return DiskHeadView::DriveChoice::Both;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::GetBars
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiToolbar *> DevicePanelController::GetBars() const
{
    return { m_bar->GetBar() };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::PlaceBar
//
////////////////////////////////////////////////////////////////////////////////

void DevicePanelController::PlaceBar()
{
    m_bar->Place();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::RouteBarMouse
//
////////////////////////////////////////////////////////////////////////////////

bool DevicePanelController::RouteBarMouse (const DxuiMouseEvent & ev)
{
    return m_bar->RouteMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::GetBarTip
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DevicePanelController::GetBarTip (POINT clientPx, RECT & anchor) const
{
    return m_bar->GetTip (clientPx, anchor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::RequestOpen
//
////////////////////////////////////////////////////////////////////////////////

bool DevicePanelController::RequestOpen()
{
    m_host.RunAction (DebuggerActions::GetPanel (m_panelId, true, m_host.GetMode()));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::RequestClose
//
////////////////////////////////////////////////////////////////////////////////

bool DevicePanelController::RequestClose()
{
    m_host.RunAction (DebuggerActions::GetPanel (m_panelId, false, m_host.GetMode()));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::GetViewMenuRank
//
//  A panel has a row while its device is in the machine, in the order the
//  snapshot lists the machine's panels.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<int> DevicePanelController::GetViewMenuRank (const DebuggerViewSnapshot * snapshot) const
{
    if (snapshot == nullptr)
    {
        return std::nullopt;
    }

    for (size_t i = 0; i < snapshot->panels.size(); i++)
    {
        if (snapshot->panels[i].id == m_panelId)
        {
            return DebuggerLayout::kDevicePanelRank + (int) i;
        }
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::AddToDock
//
////////////////////////////////////////////////////////////////////////////////

void DevicePanelController::AddToDock (DxuiDockSite & dock)
{
    dock.AddPane (GetId(), m_title, m_model->GetFrame());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::GetContent
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * DevicePanelController::GetContent() const
{
    return m_model->GetFrame();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::GetControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> DevicePanelController::GetControls() const
{
    std::vector<IDxuiControl *>  controls = m_model->GetControls();



    controls.insert (controls.begin(), { m_bar->GetSlot(), m_bar->GetBar() });
    return controls;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::GetLists
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView *> DevicePanelController::GetLists() const
{
    return { m_model->GetList() };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::GetColorKey
//
//  An info button in a band of its own, the panel having no toolbar, for
//  the key of the parts its device has.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<PaneColorKeySpec> DevicePanelController::GetColorKey() const
{
    const DiagnosticsPane  * model = m_model.get();



    return PaneColorKeySpec { nullptr, ColorLegend::Pane::MemoryMap, [model] { return model->GetColorKey(); } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::Apply
//
//  The memory map in the theme's colors, which fall back to its own. A
//  panel the window reopens at startup does not change the front tab of its
//  group, which is the tab in front when the window closed.
//
////////////////////////////////////////////////////////////////////////////////

void DevicePanelController::Apply (const DebuggerViewSnapshot & snapshot)
{
    const DiagnosticsSnapshot   * mine    = nullptr;
    bool                          wasOpen = m_isOpen;
    DebuggerTextColors::Set       colors  = m_host.GetTextColors();
    MemoryMapBar::SourceColors    map     = {};
    SoundBoardView::Palette       board;



    for (const DiagnosticsSnapshot & diagnostics : snapshot.diagnostics)
    {
        mine = (diagnostics.id == m_panelId) ? &diagnostics : mine;
    }

    m_isOpen = mine != nullptr;

    if (mine != nullptr)
    {
        map[(size_t) MemorySource::Main]    = colors.mapMain;
        map[(size_t) MemorySource::Aux]     = colors.mapAux;
        map[(size_t) MemorySource::LcBank1] = colors.mapLcBank1;
        map[(size_t) MemorySource::LcBank2] = colors.mapLcBank2;
        map[(size_t) MemorySource::Rom]     = colors.mapRom;
        map[(size_t) MemorySource::SlotRom] = colors.mapSlotRom;
        map[(size_t) MemorySource::Io]      = colors.mapIo;

        m_model->GetMap()->SetSourceColors (map);

        board.channels = { colors.mapMain, colors.mapRom, colors.mapIo };
        board.voice    = (m_host.GetTheme() != nullptr) ? m_host.GetTheme()->Accent() : board.voice;
        m_model->GetBoard()->SetPalette (board);

        if (m_model->Apply (*mine))
        {
            m_model->GetFrame()->Relayout();
        }
    }

    if (m_isOpen && !wasOpen && !m_host.IsRestoringViews())
    {
        m_host.ActivatePaneTab (GetId());
    }

    if (m_isOpen != wasOpen)
    {
        m_host.RelayoutDock();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::Tick
//
////////////////////////////////////////////////////////////////////////////////

bool DevicePanelController::Tick (int64_t nowMs)
{
    m_model->Tick (nowMs);
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::TryGetTip
//
//  Over the memory map's strips, the run of pages there and what it is, at
//  once and following the pointer, as a memory window's byte does; over the
//  disk head, what its colors mean.
//
////////////////////////////////////////////////////////////////////////////////

bool DevicePanelController::TryGetTip (POINT clientPx, DxuiTip & tip)
{
    MemoryMapBar    * map   = m_model->GetMap();
    DiskHeadView    * head  = m_model->GetHead();
    SoundBoardView  * board = m_model->GetBoard();



    if (map != nullptr && m_host.IsRoutable (map) && map->IsVisible() && map->TryGetTipAt (clientPx, tip.text))
    {
        tip.placement = DxuiTip::Placement::FollowPointer;
        return true;
    }

    //  The tip is placed at the pointer where it first opens, and isn't moved
    //  while the pointer is over a part with the same tip. Moving the balloon
    //  with every move of the pointer lagged a frame behind it.
    if (board != nullptr && m_host.IsRoutable (board) && board->IsVisible() && board->TryGetTipAt (clientPx, tip.text))
    {
        if (tip.text != m_boardTipText)
        {
            m_boardTipText   = tip.text;
            m_boardTipAnchor = RECT { clientPx.x, clientPx.y, clientPx.x + 1, clientPx.y + 1 };
        }

        tip.placement = DxuiTip::Placement::AtPointer;
        tip.anchor    = m_boardTipAnchor;
        return true;
    }

    m_boardTipText.clear();

    if (head == nullptr || !m_host.IsRoutable (head))
    {
        return false;
    }

    tip.text = head->GetColorTipAt (clientPx);

    if (tip.text.empty())
    {
        return false;
    }

    tip.placement = DxuiTip::Placement::AtPointer;
    tip.anchor    = RECT { clientPx.x, clientPx.y, clientPx.x + 1, clientPx.y + 1 };
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::RouteContentMouse
//
////////////////////////////////////////////////////////////////////////////////

bool DevicePanelController::RouteContentMouse (const DxuiMouseEvent & ev)
{
    SoundBoardView  * board = m_model->GetBoard();



    return board != nullptr && m_host.IsRoutable (board) && board->IsVisible() && board->OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::AddContentMenuItems
//
////////////////////////////////////////////////////////////////////////////////

void DevicePanelController::AddContentMenuItems (POINT clientPx, PaneMenuItems & items)
{
    (void) clientPx;

    if (m_panelId == "video")
    {
        items.push_back ({ L"Open video log", [this] { m_host.OpenEventLog (DebugLogKind::Video); } });
    }
    else if (m_panelId == "mockingboard")
    {
        items.push_back ({ L"Open sound log", [this] { m_host.OpenEventLog (DebugLogKind::Sound); } });
    }
    else if (m_panelId == "serial-card" || m_panelId.starts_with ("serial-port-"))
    {
        items.push_back ({ L"Open serial log", [this] { m_host.OpenEventLog (DebugLogKind::Serial); } });
    }
    else if (m_panelId == "uthernet")
    {
        items.push_back ({ L"Open network log", [this] { m_host.OpenEventLog (DebugLogKind::Network); } });
    }
}
