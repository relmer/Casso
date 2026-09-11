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
};
