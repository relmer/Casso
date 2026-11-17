#include "Pch.h"

#include "DxuiPointerTracker.h"
#include "Core/DxuiEvents.h"
#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPointerTracker::Update
//
//  The controls under the pointer found again, and each control on the old
//  path that isn't on the new one sent the pointer away. A control removed
//  from the tree since is skipped, as it may be gone. True when a control
//  was sent the pointer away, so the window repaints.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPointerTracker::Update (POINT clientPx, bool leftWindow)
{
    std::vector<IDxuiControl *>  path;
    bool                         sent = false;



    if (!leftWindow)
    {
        CollectPathAt (m_root, clientPx, path);
    }

    for (IDxuiControl * control : m_path)
    {
        bool  stays = std::find (path.begin(), path.end(), control) != path.end();

        if (!stays && IsInTree (m_root, control))
        {
            SendPointerAway (*control);
            sent = true;
        }
    }

    m_path = std::move (path);

    return sent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPointerTracker::CollectPathAt
//
//  The topmost visible child under the point, then its topmost child there,
//  and so on down. The last-added child is drawn on top, as in OnMouse. A
//  container that clips its children at the point isn't searched further.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPointerTracker::CollectPathAt (const IDxuiControl & control, POINT clientPx, std::vector<IDxuiControl *> & outPath)
{
    const IDxuiControl  * node  = &control;
    bool                  found = true;



    while (found && !node->IsPointClipped (clientPx))
    {
        size_t  count = node->GetChildCount();

        found = false;

        for (size_t i = count; !found && i > 0; i--)
        {
            IDxuiControl  * child = node->GetChild (i - 1);
            RECT            rc    = (child != nullptr) ? child->GetBounds() : RECT {};

            if (child != nullptr && child->IsVisible() && PtInRect (&rc, clientPx))
            {
                outPath.push_back (child);
                node  = child;
                found = true;
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPointerTracker::IsInTree
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPointerTracker::IsInTree (const IDxuiControl & root, const IDxuiControl * control)
{
    size_t  count = root.GetChildCount();
    bool    found = false;



    for (size_t i = 0; !found && i < count; i++)
    {
        const IDxuiControl  * child = root.GetChild (i);

        found = child != nullptr && (child == control || IsInTree (*child, control));
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPointerTracker::SendPointerAway
//
//  A move to a point outside every control, which clears a hover set from
//  the pointer's position, then a DxuiMouseEventKind::Leave event, which
//  clears one set otherwise.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPointerTracker::SendPointerAway (IDxuiControl & control)
{
    DxuiMouseEvent  ev;
    bool            took = false;



    ev.kind        = DxuiMouseEventKind::Move;
    ev.button      = DxuiMouseButton::None;
    ev.positionDip = { kNowherePx, kNowherePx };
    took           = control.OnMouse (ev);
    IGNORE_RETURN_VALUE (took, false);

    ev.kind = DxuiMouseEventKind::Leave;
    took    = control.OnMouse (ev);
    IGNORE_RETURN_VALUE (took, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPointerTracker::ShowScrollbarMenuIn
//
//  The control itself first, then the visible child under the point, down
//  to the control whose bar it is on. A container that clips its children
//  there, as a scrolled page does, is not searched below.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPointerTracker::ShowScrollbarMenuIn (IDxuiControl & control, HWND hwnd, POINT clientPx)
{
    bool    shown = control.ShowScrollbarMenu (hwnd, clientPx);
    size_t  count = control.GetChildCount();



    for (size_t i = 0; !shown && !control.IsPointClipped (clientPx) && i < count; i++)
    {
        IDxuiControl  * child = control.GetChild (i);
        RECT            rc    = (child != nullptr) ? child->GetBounds() : RECT {};

        if (child != nullptr && child->IsVisible() && PtInRect (&rc, clientPx))
        {
            shown = ShowScrollbarMenuIn (*child, hwnd, clientPx);
        }
    }

    return shown;
}
