#pragma once

#include "Pch.h"
#include "Widgets/DxuiPopupMenu.h"


class DxuiHwndSource;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiContextMenu
//
//  One call to raise a menu at a point, for a right-click. The menu is the
//  one every window owns for this purpose, configured from the window's own
//  DPI, theme, text renderer and client rect, and hosted through its popup
//  pool so the window routes input to it. A picked row runs its command;
//  there is no completion callback, because a command already carries what
//  should happen.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiContextMenu
{
public:
    static void  Show (DxuiHwndSource & host, int x, int y, std::vector<DxuiPopupMenuItem> items);

    //  Hung below a control, unfolding from its bottom edge, as a menu from a
    //  button or an address bar separator does; `onClosed` runs when it goes.
    static void  ShowUnder (DxuiHwndSource & host, const RECT & anchor, std::vector<DxuiPopupMenuItem> items, DxuiPopupMenu::ClosedFn onClosed = nullptr);

    //  Hung under the anchor and at least as wide as it, as a drop-down under
    //  a field is.
    static void  ShowUnderMatchingWidth (DxuiHwndSource & host, const RECT & anchor, std::vector<DxuiPopupMenuItem> items, DxuiPopupMenu::ClosedFn onClosed = nullptr);

private:
    //  Both of the above: the width floor is set on every show, since the
    //  menu is shared and one caller's width would otherwise leak into the
    //  next.
    static void  ShowBelow (DxuiHwndSource & host, const RECT & anchor, std::vector<DxuiPopupMenuItem> items, DxuiPopupMenu::ClosedFn onClosed, int minWidthPx);
};
