#include "Pch.h"

#include "Ui/Debugger/Panes/DevicePanelController.h"

#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/DiagnosticsPane.h"
#include "Ui/Debugger/Panes/DiskHeadView.h"
#include "Ui/Debugger/Panes/MemoryMapBar.h"
#include "Ui/Debugger/Panes/MeterBar.h"





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
    DxuiPanel     & parent = m_host.GetControlParent();
    DxuiListView  * list   = parent.CreateChild<DxuiListView>();
    MemoryMapBar  * map    = parent.CreateChild<MemoryMapBar>();
    DiskHeadView  * head   = parent.CreateChild<DiskHeadView>();
    MeterBar      * meters = parent.CreateChild<MeterBar>();



    m_model = std::make_unique<DiagnosticsPane> (m_panelId, m_title, list, map, head, meters);
    list->SetVisible (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController::Configure
//
////////////////////////////////////////////////////////////////////////////////

void DevicePanelController::Configure()
{
    m_model->Configure();
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
    return m_model->GetControls();
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

bool DevicePanelController::TryGetTip (POINT clientPx, PaneTip & tip)
{
    MemoryMapBar  * map  = m_model->GetMap();
    DiskHeadView  * head = m_model->GetHead();



    if (map != nullptr && m_host.IsRoutable (map) && map->IsVisible() && map->TryGetTipAt (clientPx, tip.text))
    {
        tip.kind = PaneTip::Kind::Pointer;
        return true;
    }

    if (head == nullptr || !m_host.IsRoutable (head))
    {
        return false;
    }

    tip.text = head->GetColorTipAt (clientPx);

    if (tip.text.empty())
    {
        return false;
    }

    tip.kind   = PaneTip::Kind::Control;
    tip.anchor = RECT { clientPx.x, clientPx.y, clientPx.x + 1, clientPx.y + 1 };
    return true;
}
