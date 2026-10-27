#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "Seams/Win32ContentDetails.h"
#include "Seams/Win32ShellIcons.h"

class CassoLinkConnections;
class FirewallMonitor;
class RunningCassoRegistry;
class ILinkAnnouncements;
struct CassoExplorerSessionWindow;
class CassoExplorerWindow;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane
//
//  The file list and the tree: the list's rows and cells, each built when
//  it is first drawn, the columns on display and their widths, each
//  folder's view and sort, and the tree's reveal and refresh.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerListPane
{
public:
    explicit ExplorerListPane (CassoExplorerWindow & window);

    static constexpr int       kDragImageDip       = 96;       // a dragged item's icon, as File Explorer draws it
    static constexpr const wchar_t *  kNoSearchMatches = L"No items match your search.";

    //  How far below the list's top a search's message sits: File Explorer's,
    //  measured at 150% as 27 pixels.
    static constexpr int     kSearchMessageTopDip = 18;

    //  Whether a list column shows: the user's choice from the header's menu,
    //  and the catalog's two columns only inside a disk image.
    static bool  IsListColumnShown (size_t column, bool chosen, Location::Kind kind, bool searching = false);
    void         ApplyColumnOrder  ();
    void  FillList();
    void  RevealLocationInTree();
    void  OnShellListed     (const std::wstring & key);
    void  ScrollTreeToRow (int row);
    int   WalkTreeLabels (int row, const std::wstring & path);

    //  What an empty list says, named for the kind of thing being looked at.
    static std::wstring  GetEmptyLocationMessage (Location::Kind kind);

    //  The file list's column widths from the last run.
    void  ApplyStoredColumnWidths ();
    void  ApplyColumnWidths       (const std::vector<int> & widthsDip);
    void  RememberFolderColumnWidths (int column, int widthDip);
    void  RememberFolderColumns      ();
    bool  TryApplyContentDetails (int row, DxuiListView::Cell & name);

    void  RefreshContentDetails  ();
    void  RefreshListIcons    ();
    const std::vector<DxuiListView::Cell> &  GetListRowCells (int row);
    bool  SelectRowNamed         (const std::wstring & name);
    void  SizeListIcons          ();
    RECT  GetListMessageRect() const;

    //  Explorer's folder options read again; true when one changed.
    bool  ReadFolderOptions ();

    //  The folder the list shows, as FolderViews keys it, and its type when
    //  asked for, which a host folder costs a read of its desktop.ini.
    std::wstring  GetFolderViewKey (FolderViews::FolderType * outType) const;

    //  The list in the view of the folder it now shows, when that folder is
    //  not the one the view was last set for.
    void          ApplyFolderView ();
    void          RememberFolderSort ();

    //  Re-reads the tree, keeping what was open, highlighted and on screen.
    void  RefreshTree();

private:
    friend class CassoExplorerWindow;

    CassoExplorerWindow  & m_window;

    Win32ShellIcons      m_listIcons;
    Win32ContentDetails  m_contentDetails;
    Location             m_listLocation;
    size_t               m_listTabCount   = 0;   // the tabs when FillList last saved the session

    //  The list's rows, built as it first asks for each; see GetListRowCells.
    std::vector<std::vector<DxuiListView::Cell>>  m_rowCells;
    std::vector<bool>                             m_rowBuilt;
    Location                                      m_rowLocation;
    bool                                          m_rowDark      = false;
};
