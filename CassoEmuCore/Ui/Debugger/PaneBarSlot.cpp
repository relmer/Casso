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
//  PaneBarSlot::RouteMouse
//
//  The strip and whatever menu it has open take the left button, from the
//  window the pane's bar is in.
//
////////////////////////////////////////////////////////////////////////////////

bool PaneBarSlot::RouteMouse (const DxuiMouseEvent & ev)
{
    int   x    = ev.positionDip.x;
    int   y    = ev.positionDip.y;
    bool  open = false;
    bool  over = false;



    if (m_bar == nullptr || !m_host.IsBarRouted (m_pane))
    {
        return false;
    }

    open = m_bar->IsMenuOpen();
    over = m_bar->IsVisible() && DxuiDockSite::Contains (m_bar->GetBounds(), POINT { x, y });

    if (!over && !open)
    {
        m_bar->OnToolbarMouseLeave();
        return false;
    }

    if ((ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Up) && ev.button != DxuiMouseButton::Left && !open)
    {
        return false;
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        m_host.UpdateTooltip (ev.positionDip);
        return m_bar->OnToolbarMouseMove (x, y);

    case DxuiMouseEventKind::Down:
        if (!open && m_press)
        {
            m_press();
        }

        return m_bar->OnToolbarLButtonDown (x, y);

    case DxuiMouseEventKind::Up:
        return m_bar->OnToolbarLButtonUp (x, y);

    default:
        return open;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneBarSlot::GetTip
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * PaneBarSlot::GetTip (POINT clientPx, RECT & anchor) const
{
    if (m_bar == nullptr || !m_host.IsBarRouted (m_pane) || !m_bar->IsVisible())
    {
        return nullptr;
    }

    return m_bar->GetTooltipAt (clientPx.x, clientPx.y, anchor);
}
