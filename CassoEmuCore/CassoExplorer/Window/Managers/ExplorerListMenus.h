#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/Model/GroupChoices.h"
#include "CassoExplorer/Window/Managers/ExplorerPickerMenus.h"

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
    void                            AddOpenLocationRow  (std::vector<DxuiPopupMenuItem> & items, int row);

    //  Windows' row is Add to Favorites even for a favorite; its verbs tell them apart.
    static bool                     IsFavorite          (const std::vector<std::wstring> & labels) { return std::find (labels.begin(), labels.end(), L"Remove from Favorites") != labels.end(); }

    bool                            IsListVerbOffered      (CassoExplorerActions::Verb verb) const;

    //  The selection's verbs, built again when the location or the selected
    //  rows change, or after InvalidateOfferedVerbs. The toolbar's buttons
    //  query them on every paint.
    const std::vector<CassoExplorerActions::Verb>  & GetOfferedVerbs        () const;
    void                                             InvalidateOfferedVerbs () const { m_offered.valid = false; }
    int                                              GetOfferedBuildCount   () const { return m_offered.builds; }
    std::vector<DxuiPopupMenuItem>                   GetSortChoices         (bool commandBar);
    std::vector<DxuiPopupMenuItem>                   GetGroupedSortChoices  (GroupChoices::Folder folder);
    std::vector<DxuiPopupMenuItem>                   GetSeeMoreRows         ();
    std::vector<DxuiPopupMenuItem>                   GetGroupChoices        (bool commandBar);
    std::vector<DxuiPopupMenuItem>                   GetColumnGroupChoices  (bool grouped);
    bool                                             IsSearchIndexed        ();
    static std::wstring                              GetSearchScopeFolder   (const Location & shown);
    std::vector<DxuiPopupMenuItem>                   GetSearchOptionChoices ();
    std::vector<DxuiPopupMenuItem>                   GetViewChoices         (bool withKeys);

    //  A separator only between two rows, and never two in a row.
    static void  AddSeparator          (std::vector<DxuiPopupMenuItem> & items);
    static void  RemoveExtraSeparators (std::vector<DxuiPopupMenuItem> & items);

    void                   AddViewMenus    (std::vector<DxuiPopupMenuItem> & items);
    ExplorerPickerMenus  & GetPickerMenus  () { return m_pickerMenus; }

private:
    std::wstring    GetBackgroundShellId (const Location & shown) const;
    void            AddBackgroundRows    (std::vector<DxuiPopupMenuItem> & items, const Location & shown);
    static HRESULT  CreateLibrary        ();

    //  The stack icon File Explorer draws beside each filter choice.
    static constexpr const wchar_t *  s_kFilterGroupIcon = L"imageres.dll,-198";


    void  AddOpenWithMenu (std::vector<DxuiPopupMenuItem> & items);
    void  AddCopyAsMenu   (std::vector<DxuiPopupMenuItem> & items);
    void  AddConvertMenu  (std::vector<DxuiPopupMenuItem> & items);
    void  AddCardRows     (std::vector<DxuiPopupMenuItem> & items, const Location & card);
    void  InsertSubmenu   (std::vector<DxuiPopupMenuItem> & items, size_t at, const wchar_t * label, const wchar_t * svg, std::shared_ptr<const DxuiIconImage> image, std::vector<DxuiPopupMenuItem> choices);

    struct OfferedVerbs
    {
        bool                                     valid  = false;
        Location                                 location;
        std::vector<int>                         rows;
        std::vector<CassoExplorerActions::Verb>  verbs;
        int                                      builds = 0;
    };

    ExplorerListPane       & m_pane;
    ExplorerContext        & m_context;
    ExplorerPickerMenus      m_pickerMenus { *this, m_context };
    mutable OfferedVerbs     m_offered;
};
