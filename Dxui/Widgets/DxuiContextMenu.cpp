#include "Pch.h"

#include "DxuiContextMenu.h"
#include "Window/DxuiHwndSource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiContextMenu::Show
//
//  A right-click can arrive before the window has a text renderer, which is
//  the state until its backend is up; there is nothing to measure with, so
//  the click is dropped rather than a null pointer dereferenced, exactly as
//  the callers this replaces did.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiContextMenu::Show (DxuiHwndSource & host, int x, int y, std::vector<DxuiPopupMenuItem> items)
{
    HRESULT              hr        = S_OK;
    HWND                 hwnd      = host.GetHwnd();
    IDxuiTextRenderer  * text      = host.GetTextRenderer();
    DxuiPopupMenu      & menu      = host.GetContextMenu();
    RECT                 client    = {};
    BOOL                 gotClient = FALSE;



    BAIL_OUT_IF (hwnd == nullptr || text == nullptr, S_OK);

    gotClient = GetClientRect (hwnd, &client);
    CWRA (gotClient);

    menu.SetPopupHost  (&host);
    menu.SetTheme      (host.GetTheme());
    menu.SetDpi        (host.GetScaler().GetDpi());
    menu.SetOnClosed   (nullptr);

    //  The menu is shared, so a width floor a drop-down set is cleared.
    menu.SetMinWidthPx (0);
    ClearListLook      (menu);
    menu.ShowAt        (x, y, std::move (items), *text, client);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiContextMenu::ShowUnder
//
////////////////////////////////////////////////////////////////////////////////

void DxuiContextMenu::ShowUnder (DxuiHwndSource & host, const RECT & anchor, std::vector<DxuiPopupMenuItem> items, DxuiPopupMenu::ClosedFn onClosed)
{
    ShowBelow (host, anchor, std::move (items), std::move (onClosed), 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiContextMenu::ShowUnderMatchingWidth
//
////////////////////////////////////////////////////////////////////////////////

void DxuiContextMenu::ShowUnderMatchingWidth (DxuiHwndSource & host, const RECT & anchor, std::vector<DxuiPopupMenuItem> items, DxuiPopupMenu::ClosedFn onClosed)
{
    ShowBelow (host, anchor, std::move (items), std::move (onClosed), anchor.right - anchor.left);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiContextMenu::ShowBelow
//
////////////////////////////////////////////////////////////////////////////////

void DxuiContextMenu::ShowBelow (DxuiHwndSource & host, const RECT & anchor, std::vector<DxuiPopupMenuItem> items, DxuiPopupMenu::ClosedFn onClosed, int minWidthPx)
{
    HRESULT              hr        = S_OK;
    HWND                 hwnd      = host.GetHwnd();
    IDxuiTextRenderer  * text      = host.GetTextRenderer();
    DxuiPopupMenu      & menu      = host.GetContextMenu();
    RECT                 client    = {};
    BOOL                 gotClient = FALSE;



    BAIL_OUT_IF (hwnd == nullptr || text == nullptr, S_OK);

    gotClient = GetClientRect (hwnd, &client);
    CWRA (gotClient);

    menu.SetPopupHost  (&host);
    menu.SetTheme      (host.GetTheme());
    menu.SetDpi        (host.GetScaler().GetDpi());
    menu.SetOnClosed   (std::move (onClosed));
    menu.SetMinWidthPx (minWidthPx);
    ClearListLook      (menu);
    menu.ShowUnder     (anchor, std::move (items), *text, client);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiContextMenu::ShowAboveAtWidth
//
////////////////////////////////////////////////////////////////////////////////

void DxuiContextMenu::ShowAboveAtWidth (DxuiHwndSource & host, const RECT & anchor, std::vector<DxuiPopupMenuItem> items, const ListLook & look)
{
    HRESULT              hr        = S_OK;
    HWND                 hwnd      = host.GetHwnd();
    IDxuiTextRenderer  * text      = host.GetTextRenderer();
    DxuiPopupMenu      & menu      = host.GetContextMenu();
    RECT                 client    = {};
    BOOL                 gotClient = FALSE;
    int                  width     = anchor.right - anchor.left;



    BAIL_OUT_IF (hwnd == nullptr || text == nullptr, S_OK);

    gotClient = GetClientRect (hwnd, &client);
    CWRA (gotClient);

    menu.SetPopupHost   (&host);
    menu.SetTheme       (host.GetTheme());
    menu.SetDpi         (host.GetScaler().GetDpi());
    menu.SetOnClosed    (nullptr);
    menu.SetMinWidthPx  (width);
    menu.SetMaxWidthPx  (width);
    menu.SetMaxHeightPx (look.maxHeightPx);
    menu.SetCutTipShown (look.isCutTipShown);
    menu.SetFace        (look.face);
    menu.SetMetrics     (look.metrics);
    menu.ShowAbove      (anchor, items, *text, client);

    //  The label column is known once the menu is laid out, so a menu that
    //  has to line its labels up is closed and opened again, moved along by
    //  the gap. ShowAbove on a menu that is open doesn't move it.
    if (look.labelLeftAtPx >= 0 && anchor.left + menu.GetLabelLeftPx() != look.labelLeftAtPx)
    {
        RECT  moved = anchor;

        moved.left  = std::max<LONG> (client.left, look.labelLeftAtPx - menu.GetLabelLeftPx());
        moved.right = std::max<LONG> (moved.left, anchor.right);

        menu.Hide();
        menu.SetMinWidthPx (moved.right - moved.left);
        menu.SetMaxWidthPx (moved.right - moved.left);
        menu.ShowAbove     (moved, std::move (items), *text, client);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiContextMenu::ClearListLook
//
//  The menu is shared, so the face, metrics and limits a list set are put
//  back for the next menu.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiContextMenu::ClearListLook (DxuiPopupMenu & menu)
{
    menu.SetMaxWidthPx  (0);
    menu.SetMaxHeightPx (0);
    menu.SetCutTipShown (true);
    menu.SetFace        (nullptr);
    menu.ClearMetrics();
}
