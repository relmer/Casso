#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"

class CassoExplorerWindow;
class ExplorerListPane;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus
//
//  The file list's menus: a selection's or the background's, built as File
//  Explorer's are, the header's for its columns, and the Sort, Group by and
//  View choices the command bar shares with the background menu.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerListMenus
{
public:
    ExplorerListMenus (ExplorerListPane & pane, CassoExplorerWindow & window);

    void  ShowListHeaderMenu  (int x, int y, int column);
    void  ShowListContextMenu (int x, int y, int group = -1);

    //  Whether the selection's menu has the verb, which the command bar's
    //  buttons and the list's keys follow.
    bool  IsListVerbOffered   (CassoExplorerActions::Verb verb) const;

    //  The rows of the Sort, Group by and View menus, shared by the command
    //  bar and the list's background menu.
    std::vector<DxuiPopupMenuItem>  GetSortChoices  ();
    std::vector<DxuiPopupMenuItem>  GetGroupChoices ();
    std::vector<DxuiPopupMenuItem>  GetViewChoices  ();

private:
    void  AddViewMenus    (std::vector<DxuiPopupMenuItem> & items);
    void  AddOpenWithMenu (std::vector<DxuiPopupMenuItem> & items);
    void  AddCopyAsMenu   (std::vector<DxuiPopupMenuItem> & items);

    ExplorerListPane     & m_pane;
    CassoExplorerWindow  & m_window;
};