#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"

struct ExplorerContext;
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
    ExplorerListMenus (ExplorerListPane & pane, ExplorerContext & context);

    void                            ShowListHeaderMenu  (int x, int y, int column);

    //  A column's filter choices, each a check that stays open, under the
    //  header's chevron at `chevron`, in the window's pixels.
    void                            ShowFilterMenu      (size_t column, RECT chevron);
    void                            ShowListContextMenu (int x, int y, int group = -1);
    void                            ShowHomeItemMenu    (int x, int y, int row);
    void                            AddHomeItemRows     (std::vector<DxuiPopupMenuItem> & items, const std::wstring & id);
    bool                            IsListVerbOffered   (CassoExplorerActions::Verb verb) const;
    std::vector<DxuiPopupMenuItem>  GetSortChoices      ();
    std::vector<DxuiPopupMenuItem>  GetGroupChoices     ();
    std::vector<DxuiPopupMenuItem>  GetViewChoices      ();

private:
    //  The stack icon File Explorer draws beside each filter choice.
    static constexpr const wchar_t *  s_kFilterGroupIcon = L"imageres.dll,-198";


    void  AddViewMenus    (std::vector<DxuiPopupMenuItem> & items);
    void  AddOpenWithMenu (std::vector<DxuiPopupMenuItem> & items);
    void  AddCopyAsMenu   (std::vector<DxuiPopupMenuItem> & items);
    void  AddConvertMenu  (std::vector<DxuiPopupMenuItem> & items);

    ExplorerListPane  & m_pane;
    ExplorerContext   & m_context;
};
