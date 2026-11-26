#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"

struct ExplorerContext;
class ExplorerListMenus;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMenus
//
//  The list's and the tree's context menus in a picker: the main window's
//  clipboard, rename, delete, New folder and Properties commands, and none
//  that open a program, change an image or start Casso.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerPickerMenus
{
public:
    ExplorerPickerMenus (ExplorerListMenus & listMenus, ExplorerContext & context);

    //  The list verbs a picker offers, in the order given.
    static std::vector<CassoExplorerActions::Verb>  GetPickerVerbs (const std::vector<CassoExplorerActions::Verb> & verbs);

    std::vector<DxuiPopupMenuItem>                  GetListItems   (int group);
    std::vector<DxuiPopupMenuItem>                  GetTreeItems   (const std::wstring & id);
    void                                            ShowListMenu   (int x, int y, int group);
    void                                            ShowTreeMenu   (int x, int y, const std::wstring & id);

private:
    void  AddSelectionRows  (std::vector<DxuiPopupMenuItem> & items);
    void  AddBackgroundRows (std::vector<DxuiPopupMenuItem> & items);
    void  AddPropertiesRow  (std::vector<DxuiPopupMenuItem> & items, std::function<void()> dispatch);

    ExplorerListMenus  & m_listMenus;
    ExplorerContext    & m_context;
};
