#include "Pch.h"

#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetHeatMapOptions
//
//  A choice made on the heat map's bar: shown at once, kept, and sent on to
//  the machine. The fade drop-down's rows are built again, since each holds
//  the check it was built with.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetHeatMapOptions (const HeatMapOptions & options)
{
    m_heatMapView->SetOptions (options);

    if (m_host != nullptr)
    {
        m_host->SetDebuggerHeatMapOptions (options.ToText());
    }

    SetHeatMapBarMenus();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetHeatMapBarMenus
//
//  The fade times, the one in force checked.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetHeatMapBarMenus()
{
    std::vector<DxuiPopupMenuItem>  items;
    int                             current = m_heatMapView->GetOptions().fadeSeconds;



    m_heatMapFadeCommands.clear();

    for (int seconds : HeatMapOptions::kFadeChoices)
    {
        std::shared_ptr<DxuiCommand>  command = MakeMenuCommand (HeatMapBarCommands::GetFadeChoiceLabel (seconds), seconds == current, [this, seconds]
        {
            HeatMapOptions  options = m_heatMapView->GetOptions();



            options.fadeSeconds = seconds;
            SetHeatMapOptions (options);
        });

        m_heatMapFadeCommands.push_back (command);
        items.push_back (DxuiPopupMenuItem::ForCommand (command));
    }

    m_heatMapBar->SetDropDownItems (HeatMapBarCommands::kFade, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceHeatMapBar
//
//  The bar sits in the place held at the top of the heat map pane and is one
//  of the pane's controls, so it goes with the pane into a floating window
//  and draws, measures and opens its menu there.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceHeatMapBar()
{
    bool          shown = m_heatMapBarSlot != nullptr && m_heatMapBarSlot->IsVisible();
    DxuiWindow  * host  = GetPaneHost (DebuggerLayout::kHeatMap);
    RECT          slot  = {};



    m_heatMapBar->SetVisible (shown);

    if (!shown)
    {
        return;
    }

    slot = m_heatMapBarSlot->GetBounds();

    m_heatMapBar->SetTextRenderer   (host->GetTextRenderer());
    m_heatMapBar->SetPopupHost      (host->GetPopupHost());
    m_heatMapBar->SetHostClientRect (host->GetBounds());
    m_heatMapBar->Layout            (slot, m_scaler);

    host->SetChildClip (m_heatMapBar, slot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteHeatMapBarMouse
//
//  As the breakpoints pane's: the strip and whatever menu it has open take
//  the left button.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteHeatMapBarMouse (const DxuiMouseEvent & ev)
{
    int   x     = ev.positionDip.x;
    int   y     = ev.positionDip.y;
    RECT  strip = m_heatMapBar->GetBounds();
    bool  open  = m_heatMapBar->IsMenuOpen();
    bool  over  = m_heatMapBar->IsVisible() && DxuiDockSite::Contains (strip, POINT { x, y });



    if (!over && !open)
    {
        m_heatMapBar->OnToolbarMouseLeave();
        return false;
    }

    if ((ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Up) && ev.button != DxuiMouseButton::Left && !open)
    {
        return false;
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        UpdateTooltip (ev.positionDip);

        return m_heatMapBar->OnToolbarMouseMove (x, y);

    case DxuiMouseEventKind::Down:
        if (!open)
        {
            SetHeatMapBarMenus();
        }

        return m_heatMapBar->OnToolbarLButtonDown (x, y);

    case DxuiMouseEventKind::Up:
        return m_heatMapBar->OnToolbarLButtonUp (x, y);

    default:
        return open;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsHeatMapBarEnabled
//
//  The fade time counts only while fading, and Reset counts only while
//  cumulative.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsHeatMapBarEnabled (int id) const
{
    bool  isCumulative = m_heatMapView->GetOptions().cumulative;



    switch (id)
    {
    case HeatMapBarCommands::kFade:        return !isCumulative;
    case HeatMapBarCommands::kResetCounts: return isCumulative;
    default:                               return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsHeatMapBarChecked
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsHeatMapBarChecked (int id) const
{
    bool  isCumulative = m_heatMapView->GetOptions().cumulative;



    switch (id)
    {
    case HeatMapBarCommands::kFading:     return !isCumulative;
    case HeatMapBarCommands::kCumulative: return isCumulative;
    default:                              return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetHeatMapBarLabel
//
//  The fade drop-down reads the time in force; the rest keep their labels.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetHeatMapBarLabel (int id) const
{
    if (id == HeatMapBarCommands::kFade)
    {
        return HeatMapBarCommands::GetFadeLabel (m_heatMapView->GetOptions().fadeSeconds);
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RunHeatMapBarEntry
//
//  The fade time is chosen from its drop-down's rows, not here.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RunHeatMapBarEntry (int id)
{
    HeatMapOptions  options = m_heatMapView->GetOptions();



    switch (id)
    {
    case HeatMapBarCommands::kFading:
    case HeatMapBarCommands::kCumulative:
        options.cumulative = (id == HeatMapBarCommands::kCumulative);
        SetHeatMapOptions (options);
        break;

    case HeatMapBarCommands::kResetCounts:
        if (m_host != nullptr)
        {
            m_host->ResetDebuggerHeatMap();
        }

        break;

    case HeatMapBarCommands::kZoomIn:     m_heatMapView->ZoomIn();    break;
    case HeatMapBarCommands::kZoomOut:    m_heatMapView->ZoomOut();   break;
    case HeatMapBarCommands::kResetZoom:  m_heatMapView->ResetZoom(); break;

    default:
        break;
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::TryGetHeatMapTip
//
//  Over the heat map's map, the tip the view gives for the cell the mouse
//  picks.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::TryGetHeatMapTip (POINT clientPx, RECT & anchor, std::wstring & text) const
{
    return IsRoutable (m_heatMapView) && m_heatMapView->IsVisible() && DxuiDockSite::Contains (m_heatMapView->GetBounds(), clientPx) &&
           m_heatMapView->TryGetTipAt (clientPx, anchor, text);
}
