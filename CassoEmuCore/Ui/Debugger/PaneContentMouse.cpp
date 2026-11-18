#include "Pch.h"

#include "Ui/Debugger/PaneContentMouse.h"
#include "Ui/Debugger/Panes/DisassemblyGutter.h"
#include "Ui/Debugger/Panes/FindWidgets.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PaneContentMouse::PaneContentMouse
//
////////////////////////////////////////////////////////////////////////////////

PaneContentMouse::PaneContentMouse (IPaneContentMouseHost & host) :
    m_host (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneContentMouse::Route
//
//  A list mid-press or mid-drag takes every event but a press, wherever the
//  pointer is; otherwise the event goes by its kind.
//
////////////////////////////////////////////////////////////////////////////////

bool PaneContentMouse::Route (const DxuiMouseEvent & ev)
{
    for (DxuiListView * list : m_host.GetLists())
    {
        if (m_host.IsRoutable (list) && list->IsInteracting() && ev.kind != DxuiMouseEventKind::Down)
        {
            list->AsControl().OnMouse (ev);

            //  A click on a register's value edits it once the click is
            //  over, so the list is not left mid-press under the box.
            if (ev.kind == DxuiMouseEventKind::Up)
            {
                m_host.NoteListReleased (list, ev.positionDip);
            }

            return true;
        }
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:  return RouteMove  (ev);
    case DxuiMouseEventKind::Down:  return RouteDown  (ev);
    case DxuiMouseEventKind::Up:    return RouteUp    (ev);
    case DxuiMouseEventKind::Wheel: return RouteWheel (ev);
    default:                        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneContentMouse::RouteMove
//
////////////////////////////////////////////////////////////////////////////////

bool PaneContentMouse::RouteMove (const DxuiMouseEvent & ev)
{
    bool  lbDown = DxuiInputState::IsKeyDown (VK_LBUTTON);



    //  A floating window's tips show from its own tooltip.
    m_host.UpdateTooltip (ev.positionDip);
    m_host.GetCodeGutter().Hover (ev.positionDip);
    m_host.GetCodeGutter().Drag  (ev.positionDip);

    //  Each box is given the move: a hover, or a drag begun in the box,
    //  which selects text.
    for (IDxuiControl * box : m_host.GetHoverBoxes())
    {
        if (m_host.IsRoutable (box))
        {
            (void) box->OnMouse (ev);
        }
    }

    //  Each find widget moves with its pane.
    m_host.GetFindWidgets().OnMouseMove (ev, lbDown);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneContentMouse::RouteDown
//
////////////////////////////////////////////////////////////////////////////////

bool PaneContentMouse::RouteDown (const DxuiMouseEvent & ev)
{
    int   x       = ev.positionDip.x;
    int   y       = ev.positionDip.y;
    bool  handled = false;



    if (ev.button != DxuiMouseButton::Left)
    {
        return true;
    }

    if (m_host.GetCodeGutter().Click (ev))
    {
        return true;
    }

    for (IDxuiControl * control : m_host.GetPressTargets())
    {
        OfferPress (control, ev, handled);
    }

    for (DxuiListView * list : m_host.GetLists())
    {
        RECT  bounds = list->GetBounds();

        if (!handled && m_host.IsRoutable (list) && list->IsVisible() && x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom)
        {
            (void) list->AsControl().OnMouse (ev);
            m_host.FocusControl (list);
            handled = true;
            m_host.NoteCodeListPressed (list);
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneContentMouse::RouteUp
//
////////////////////////////////////////////////////////////////////////////////

bool PaneContentMouse::RouteUp (const DxuiMouseEvent & ev)
{
    int  x = ev.positionDip.x;
    int  y = ev.positionDip.y;



    if (ev.button == DxuiMouseButton::Left && m_host.GetCodeGutter().Drop (ev))
    {
        return true;
    }

    //  Only to what is shown: the disassembly views' radios share a rect,
    //  and a press on the one in front must not reach those behind it.
    for (IDxuiControl * control : m_host.GetPressTargets())
    {
        if (m_host.IsRoutable (control) && control->IsVisible())
        {
            control->OnMouse (ev);
        }
    }

    //  A release goes to the list UNDER THE POINTER, the same as a press. A
    //  list mid-drag already had it above, whatever the pointer is over.
    //  Sending it to every list reached the ones hidden behind a tab as
    //  well: the Stack and Call Stack panes share a rect, so a click on the
    //  fifth Stack row released onto the fifth Call Stack row -- which
    //  activated it and moved the code pane to that frame's call site.
    for (DxuiListView * list : m_host.GetLists())
    {
        RECT  bounds = list->GetBounds();

        if (m_host.IsRoutable (list) && list->IsVisible() &&
            x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom)
        {
            list->AsControl().OnMouse (ev);
            m_host.NoteListReleased (list, ev.positionDip);
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneContentMouse::RouteWheel
//
////////////////////////////////////////////////////////////////////////////////

bool PaneContentMouse::RouteWheel (const DxuiMouseEvent & ev)
{
    int  x = ev.positionDip.x;
    int  y = ev.positionDip.y;



    //  The code pane has only the lines in view, so the vertical wheel
    //  scrolls the disassembly itself, through all of memory.
    if (m_host.ScrollCodeViewAt (ev))
    {
        return true;
    }

    for (DxuiListView * list : m_host.GetLists())
    {
        RECT  bounds = list->GetBounds();

        if (m_host.IsRoutable (list) && list->IsVisible() && x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom)
        {
            list->AsControl().OnMouse (ev);
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaneContentMouse::OfferPress
//
////////////////////////////////////////////////////////////////////////////////

void PaneContentMouse::OfferPress (IDxuiControl * control, const DxuiMouseEvent & ev, bool & handled)
{
    if (!handled && control != nullptr && m_host.IsRoutable (control) && control->IsVisible() && control->OnMouse (ev))
    {
        m_host.FocusControl (control);
        handled = true;
    }
}
