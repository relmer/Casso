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

    menu.SetPopupHost (&host);
    menu.SetTheme (host.GetTheme());
    menu.SetDpi (host.GetScaler().GetDpi());
    menu.ShowAt (x, y, std::move (items), *text, client);

Error:
    return;
}
