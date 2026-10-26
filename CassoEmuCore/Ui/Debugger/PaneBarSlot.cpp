#include "Pch.h"

#include "Ui/Debugger/PaneBarSlot.h"

#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PaneBarSlot::PaneBarSlot
//
////////////////////////////////////////////////////////////////////////////////

PaneBarSlot::PaneBarSlot (IDebuggerPaneHost & host, std::wstring pane, std::wstring accessibleName) :
    m_host (host),
    m_pane (std::move (pane)),
    m_slot (std::make_unique<DebuggerPaneFrame> (std::move (accessibleName)))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneBarSlot::~PaneBarSlot
//
////////////////////////////////////////////////////////////////////////////////

PaneBarSlot::~PaneBarSlot() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  PaneBarSlot::Create
//
////////////////////////////////////////////////////////////////////////////////

void PaneBarSlot::Create()
{
    m_bar = m_host.GetControlParent().CreateChild<DxuiToolbar>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneBarSlot::Configure
//
////////////////////////////////////////////////////////////////////////////////

void PaneBarSlot::Configure()
{
    m_bar->SetTextRenderer  (m_host.GetPaneTextRenderer());
    m_bar->SetPopupHost     (m_host.GetPanePopupHost());
    m_bar->SetIconFace      (DxuiToolbar::kMdl2IconFace);
    m_bar->SetCompact       (true);
    m_bar->SetPaneTextInset (true);
    m_bar->EnableSeeMore    (s_kpszMdl2More, L"See more");
    m_bar->SetVisible       (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneBarSlot::GetHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int PaneBarSlot::GetHeightPx (int, const DxuiDpiScaler & scaler)
{
    return scaler.ToPx (DxuiToolbar::kCompactBandDp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneBarSlot::Place
//
//  The bar is visible while its slot is, laid out over the slot in the window
//  the pane is in and clipped to it.
//
////////////////////////////////////////////////////////////////////////////////

void PaneBarSlot::Place()
{
    bool          shown = m_bar != nullptr && m_slot->IsVisible();
    DxuiWindow  * host  = nullptr;
    RECT          slot  = {};



    if (m_bar == nullptr)
    {
        return;
    }

    m_bar->SetVisible (shown);

    if (!shown)
    {
        return;
    }

    host = m_host.GetPaneWindow (m_pane);
    slot = m_slot->GetBounds();

    m_bar->SetTextRenderer   (host->GetTextRenderer());
    m_bar->SetPopupHost      (host->GetPopupHost());
    m_bar->SetHostClientRect (host->GetBounds());
    m_bar->Layout            (m_host.GetBarStrip (m_pane, slot), m_host.GetPaneScaler());

    host->SetChildClip (m_bar, slot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneBarSlot::RouteToolbarMouse
//
//  The strip and whatever menu it has open take the left button, from the
//  window the pane's bar is in.
//
////////////////////////////////////////////////////////////////////////////////

bool PaneBarSlot::RouteToolbarMouse (IDebuggerPaneHost & host, const std::wstring & pane, DxuiToolbar * bar, const DxuiMouseEvent & ev, const PressFn & press)
{
    int   x    = ev.positionDip.x;
    int   y    = ev.positionDip.y;
    bool  open = false;
    bool  over = false;



    if (bar == nullptr || !host.IsBarRouted (pane))
    {
        return false;
    }

    open = bar->IsMenuOpen();
    over = bar->IsVisible() && DxuiDockSite::Contains (bar->GetBounds(), POINT { x, y });

    if (!over && !open)
    {
        bar->OnToolbarMouseLeave();
        return false;
    }

    if ((ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Up) && ev.button != DxuiMouseButton::Left && !open)
    {
        return false;
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        host.UpdateTooltip (ev.positionDip);
        return bar->OnToolbarMouseMove (x, y);

    case DxuiMouseEventKind::Down:
        if (!open && press)
        {
            press();
        }

        return bar->OnToolbarLButtonDown (x, y);

    case DxuiMouseEventKind::Up:
        return bar->OnToolbarLButtonUp (x, y);

    default:
        return open;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneBarSlot::GetToolbarTip
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * PaneBarSlot::GetToolbarTip (IDebuggerPaneHost & host, const std::wstring & pane, const DxuiToolbar * bar, POINT clientPx, RECT & anchor)
{
    if (bar == nullptr || !host.IsBarRouted (pane) || !bar->IsVisible())
    {
        return nullptr;
    }

    return bar->GetTooltipAt (clientPx.x, clientPx.y, anchor);
}
