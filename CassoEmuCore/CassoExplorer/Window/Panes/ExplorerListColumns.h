#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/FolderViews.h"
#include "CassoExplorer/Model/Location.h"

struct ExplorerContext;
class ExplorerListPane;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns
//
//  The file list's view and columns: the view each folder opens in, the
//  columns the user chose from the header's menu, their order and widths,
//  and each folder's sort, read from and written to the preferences per
//  folder, as in File Explorer.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerListColumns
{
public:
    ExplorerListColumns (ExplorerListPane & pane, ExplorerContext & context);

    static bool  IsListColumnShown          (size_t column, bool chosen, Location::Kind kind, bool searching = false);
    static bool  IsListColumnOffered        (size_t column, Location::Kind kind, bool searching = false);
    void         ApplyColumnOrder           ();
    void         ShowChosenColumns          ();
    void         ChooseAll                  (size_t count) { m_chosen.assign (count, true); }
    void         SetChosen                  (size_t column, bool chosen);
    void         ApplyStoredColumnWidths    ();
    void         ApplyColumnWidths          (const std::vector<int> & widthsDip);
    void         RememberFolderColumnWidths (int column, int widthDip);
    void         RememberFolderColumns      ();

    //  The column order for the folder on display: the folder's, or the latest.
    void                  SetFolderOrder     (std::vector<int> order) { m_folderOrder = std::move (order); }
    bool                  ReadFolderOptions  ();
    std::wstring          GetFolderViewKey   (FolderViews::FolderType * outType) const;
    void                  ApplyFolderView    ();
    void                  RememberFolderSort ();
    DxuiListView::View    GetView            () const { return m_view; }
    std::vector<bool>   & GetChosen          ()       { return m_chosen; }
    void                  SetView            (DxuiListView::View view);

private:
    ExplorerListPane    & m_pane;
    ExplorerContext     & m_context;
    std::vector<bool>     m_chosen;   // the columns the user has chosen to show
    std::vector<int>      m_folderOrder;   // the column order the folder on display uses
    DxuiListView::View    m_view        = DxuiListView::View::Details;
    std::wstring          m_viewKey;   // the folder the view was chosen for: a new folder opens in its view
};