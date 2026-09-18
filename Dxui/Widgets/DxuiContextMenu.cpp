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
    menu.ShowUnder     (anchor, std::move (items), *text, client);

Error:
    return;
}
