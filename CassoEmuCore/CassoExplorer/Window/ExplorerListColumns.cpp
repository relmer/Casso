#include "Pch.h"

#include "CassoExplorer/Window/ExplorerListColumns.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "Core/PathResolver.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::ExplorerListColumns
//
////////////////////////////////////////////////////////////////////////////////

ExplorerListColumns::ExplorerListColumns (ExplorerListPane & pane, CassoExplorerWindow & window)
    : m_pane   (pane),
      m_window (window)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::ShowChosenColumns
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListColumns::ShowChosenColumns()
{
    const Location &  location = m_window.m_browser.GetLocation();



    for (size_t c = 0; c < m_chosen.size(); c++)
    {
        m_pane.m_list->SetColumnVisible (c, IsListColumnShown (c, m_chosen[c], location.kind, SearchQuery::IsId (location.path)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::SetChosen
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListColumns::SetChosen (size_t column, bool chosen)
{
    if (column >= m_chosen.size())
    {
        m_chosen.resize (column + 1, true);
    }

    m_chosen[column] = chosen;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::SetView
//
//  Chosen for this folder alone, as Explorer remembers it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListColumns::SetView (DxuiListView::View view)
{
    m_view    = view;
    m_viewKey = GetFolderViewKey (nullptr);

    m_window.m_prefs.folderViews.Remember (m_viewKey, m_view);
    m_pane.m_list->SetView (m_view);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::IsListColumnShown
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListColumns::IsListColumnShown (size_t column, bool chosen, Location::Kind kind, bool searching)
{
    bool  catalog     = column == (size_t) CatalogModel::Column::Address || column == (size_t) CatalogModel::Column::Locked;
    bool  recycled    = column == (size_t) CatalogModel::Column::OriginalLocation || column == (size_t) CatalogModel::Column::DateDeleted;
    bool  insideImage = kind == Location::Kind::DiskImage || kind == Location::Kind::DiskDirectory;
    bool  inBin       = kind == Location::Kind::RecycleBin;



    if (column == (size_t) CatalogModel::Column::Name)
    {
        return true;
    }

    if (recycled)
    {
        return inBin;
    }

    //  Search results show the folder each is in, as Explorer's do.
    if (column == (size_t) CatalogModel::Column::Folder)
    {
        return searching;
    }

    return chosen && (insideImage || !catalog);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::ApplyColumnOrder
//
//  The order the user dragged the headers into, with any column it predates
//  at the end; in the Recycle Bin, Explorer's order for it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListColumns::ApplyColumnOrder()
{
    using Column = CatalogModel::Column;

    std::vector<size_t>  order;
    size_t               count = BrowserCells::GetColumns().size();



    if (m_window.m_browser.GetLocation().kind == Location::Kind::RecycleBin)
    {
        for (Column column : { Column::Name, Column::OriginalLocation, Column::DateDeleted, Column::Size, Column::Type, Column::Modified, Column::Address, Column::Locked, Column::Folder })
        {
            order.push_back ((size_t) column);
        }
    }
    else
    {
        for (int column : m_folderOrder)
        {
            if (column >= 0 && (size_t) column < count && std::find (order.begin(), order.end(), (size_t) column) == order.end())
            {
                order.push_back ((size_t) column);
            }
        }

        for (size_t column = 0; column < count; column++)
        {
            if (std::find (order.begin(), order.end(), column) == order.end())
            {
                order.push_back (column);
            }
        }
    }

    m_pane.m_list->SetColumnOrder (order);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::GetFolderViewKey
//
//  A host folder by its path, a place inside a disk image by the image and
//  the directory, and a root by its id.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerListColumns::GetFolderViewKey (FolderViews::FolderType * outType) const
{
    Location                 location = m_window.m_browser.GetLocation();
    FolderViews::FolderType  type     = FolderViews::FolderType::Generic;



    switch (location.kind)
    {
        case Location::Kind::None:
            return std::wstring();

        case Location::Kind::Root:
            if (outType != nullptr)
            {
                *outType = (location.path == TreeModel::kThisPcRootId) ? FolderViews::FolderType::Drives : type;
            }

            return location.path;

        case Location::Kind::HostFolder:
            //  A network folder's desktop.ini is not read, so an unreachable
            //  share never stops the window; it takes the generic view.
            if (outType != nullptr)
            {
                *outType = PathResolver::IsNetworkPath (location.path) ? type : FolderViews::ReadFolderType (location.path);
            }

            return location.path;

        default:
            break;
    }

    //  Every search shares one view, Content unless chosen otherwise, as
    //  File Explorer's results do.
    if (SearchQuery::IsId (location.path))
    {
        if (outType != nullptr)
        {
            *outType = FolderViews::FolderType::SearchResults;
        }

        return SearchQuery::kPrefix;
    }

    return location.path + L"|" + TextEncoding::NarrowToWide (location.innerPath);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::ApplyFolderView
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListColumns::ApplyFolderView()
{
    FolderViews::FolderType  type  = FolderViews::FolderType::Generic;
    std::wstring             key   = GetFolderViewKey (nullptr);
    FolderViewEntry          entry;



    if (key == m_viewKey)
    {
        return;
    }

    GetFolderViewKey (&type);

    m_viewKey = key;
    m_view    = m_window.m_prefs.folderViews.GetView (key, type);
    m_pane.m_list->SetView (m_view);
    m_window.m_menus.ShowViewOnButton();
    m_pane.SizeListIcons();

    //  Each folder keeps its own sort and grouping, as Explorer's do; one
    //  never given either opens by name and ungrouped.
    entry = m_window.m_prefs.folderViews.GetEntry (key, type);

    if (entry.sortColumn > (int) CatalogModel::Column::DateDeleted)
    {
        entry.sortColumn = 0;
    }

    if (entry.groupBy > (int) RowGrouping::Field::Size)
    {
        entry.groupBy = 0;
    }

    m_window.m_browser.SetSortAndGroup ((CatalogModel::Column) entry.sortColumn, entry.sortDescending,
                                        (RowGrouping::Field) entry.groupBy, entry.groupDescending);

    ApplyColumnWidths (entry.columnWidthsDip.empty() ? m_window.m_prefs.columnWidthsDip : entry.columnWidthsDip);

    //  Its own column order and choice of columns, or the latest ones.
    m_folderOrder = entry.columnOrder.empty() ? m_window.m_prefs.columnOrder : entry.columnOrder;
    m_chosen.assign (BrowserCells::GetColumns().size(), true);

    for (int hidden : (entry.columnsChosen ? entry.hiddenColumns : m_window.m_prefs.hiddenColumns))
    {
        if (hidden > (int) CatalogModel::Column::Name && (size_t) hidden < m_chosen.size())
        {
            m_chosen[(size_t) hidden] = false;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::RememberFolderSort
//
//  Kept only when the user sorts or groups, so a folder merely visited keeps
//  following the defaults.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListColumns::RememberFolderSort()
{
    FolderViewEntry  entry;



    if (m_viewKey.empty() || !m_window.m_browser.GetBrowserModel().HasTabs())
    {
        return;
    }

    const BrowserModel::Tab &  tab = m_window.m_browser.GetBrowserModel().GetActiveTab();

    entry                 = m_window.m_prefs.folderViews.GetEntry (m_viewKey, FolderViews::FolderType::Generic);
    entry.view            = m_view;
    entry.sortColumn      = (int) tab.sortColumn;
    entry.sortDescending  = tab.sortDescending;
    entry.groupBy         = (int) tab.groupBy;
    entry.groupDescending = tab.groupDescending;

    m_window.m_prefs.folderViews.Remember (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::ReadFolderOptions
//
//  Explorer's options for hidden items, read again. When one has changed,
//  the tree is listed again under them, and true is returned so the caller
//  relists the folder.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListColumns::ReadFolderOptions()
{
    FolderOptions  options = FolderOptions::ReadFromShell();



    if (options == m_window.m_explorerOptions)
    {
        return false;
    }

    //  The pane's options Casso Explorer still follows change with them.
    m_window.m_explorerOptions = options;
    m_window.m_browser.SetFolderOptions (options);
    m_window.m_treePane.ApplyNavPaneOptions();
    m_window.m_browser.GetTreeModel().InvalidateAll();
    m_window.m_treePane.RefreshTree();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::ApplyStoredColumnWidths
//
//  The widths from the last run, where there are any. A stored zero means the
//  column was never given a width of its own and still fits itself.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListColumns::ApplyStoredColumnWidths()
{
    ApplyColumnWidths (m_window.m_prefs.columnWidthsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::ApplyColumnWidths
//
//  A column with no width here goes back to fitting itself, so one folder's
//  widths do not stay on in the next.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListColumns::ApplyColumnWidths (const std::vector<int> & widthsDip)
{
    size_t  c = 0;



    for (c = 0; c < m_pane.m_list->GetColumnCount(); c++)
    {
        int  dip = (c < widthsDip.size()) ? widthsDip[c] : 0;

        //  At the DPI the list holds now, which it rescales from when its
        //  DPI changes; the window's may not have caught up yet.
        m_pane.m_list->SetColumnOverrideWidthPx (c, (dip > 0) ? MulDiv (dip, (int) m_pane.m_list->GetDpi(), (int) DxuiDpiScaler::kBaseDpi) : -1);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::RememberFolderColumns
//
//  The order and the choice of columns the folder shows now, kept for it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListColumns::RememberFolderColumns()
{
    FolderViewEntry  entry;



    if (m_viewKey.empty())
    {
        return;
    }

    entry               = m_window.m_prefs.folderViews.GetEntry (m_viewKey, FolderViews::FolderType::Generic);
    entry.view          = m_view;
    entry.columnOrder   = m_folderOrder;
    entry.columnsChosen = true;
    entry.hiddenColumns.clear();

    for (size_t column = 0; column < m_chosen.size(); column++)
    {
        if (!m_chosen[column])
        {
            entry.hiddenColumns.push_back ((int) column);
        }
    }

    m_window.m_prefs.folderViews.Remember (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::RememberFolderColumnWidths
//
//  The folder starts from the widths it showed, so the columns the user did
//  not touch keep what they had.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListColumns::RememberFolderColumnWidths (int column, int widthDip)
{
    FolderViewEntry  entry;



    if (m_viewKey.empty() || column < 0)
    {
        return;
    }

    entry      = m_window.m_prefs.folderViews.GetEntry (m_viewKey, FolderViews::FolderType::Generic);
    entry.view = m_view;

    if (entry.columnWidthsDip.empty())
    {
        entry.columnWidthsDip = m_window.m_prefs.columnWidthsDip;
    }

    if ((size_t) column >= entry.columnWidthsDip.size())
    {
        entry.columnWidthsDip.resize ((size_t) column + 1, 0);
    }

    entry.columnWidthsDip[(size_t) column] = widthDip;

    m_window.m_prefs.folderViews.Remember (entry);
}
