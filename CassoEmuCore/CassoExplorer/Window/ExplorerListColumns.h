#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/FolderViews.h"
#include "CassoExplorer/Model/Location.h"

class CassoExplorerWindow;
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
    ExplorerListColumns (ExplorerListPane & pane, CassoExplorerWindow & window);

    //  Whether a list column is visible: the user's choice from the header's
    //  menu, and the catalog's two columns only inside a disk image.
    static bool  IsListColumnShown (size_t column, bool chosen, Location::Kind kind, bool searching = false);
    void         ApplyColumnOrder  ();

    //  Every column the user chose made visible, for the location in the list.
    void         ShowChosenColumns ();
    bool         IsChosen          (size_t column) const { return column < m_chosen.size() && m_chosen[column]; }
    void         ChooseAll         (size_t count) { m_chosen.assign (count, true); }
    void         SetChosen         (size_t column, bool chosen);

    //  The file list's column widths from the last run.
    void  ApplyStoredColumnWidths    ();
    void  ApplyColumnWidths          (const std::vector<int> & widthsDip);
    void  RememberFolderColumnWidths (int column, int widthDip);
    void  RememberFolderColumns      ();

    //  The column order for the folder on display: the folder's, or the latest.
    void  SetFolderOrder (std::vector<int> order) { m_folderOrder = std::move (order); }

    //  Explorer's folder options read again; true when one changed.
    bool  ReadFolderOptions ();

    //  The folder the list shows, as FolderViews keys it, and its type when
    //  asked for, which a host folder costs a read of its desktop.ini.
    std::wstring  GetFolderViewKey (FolderViews::FolderType * outType) const;

    //  The list in the view of the folder it now shows, when that folder is
    //  not the one the view was last set for.
    void                ApplyFolderView    ();
    void                RememberFolderSort ();
    DxuiListView::View  GetView            () const { return m_view; }

    //  A view chosen for the folder on display, and written to the preferences
    //  for it.
    void                SetView            (DxuiListView::View view);

private:
    friend class ExplorerListMenus;

    ExplorerListPane     & m_pane;
    CassoExplorerWindow  & m_window;

    std::vector<bool>      m_chosen;        // the columns the user has chosen to show
    std::vector<int>       m_folderOrder;   // the column order the folder on display uses
    DxuiListView::View     m_view    = DxuiListView::View::Details;
    std::wstring           m_viewKey;       // the folder the view was chosen for: a new folder opens in its view
};