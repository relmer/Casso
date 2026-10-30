#include "Pch.h"

#include "CassoExplorer/Window/Panes/ExplorerListColumns.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "Core/PathResolver.h"
#include "Core/TextEncoding.h"
#include "CassoExplorer/Browser/BrowserCells.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "CassoExplorer/Window/Panes/ExplorerToolbars.h"
#include "CassoExplorer/Model/FolderOptions.h"
#include "CassoExplorer/Window/Panes/ExplorerTreePane.h"
#include "CassoExplorer/ChooseDetailsDialog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::ExplorerListColumns
//
////////////////////////////////////////////////////////////////////////////////

ExplorerListColumns::ExplorerListColumns (ExplorerListPane & pane, ExplorerContext & context)
    : m_pane   (pane),
      m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::ShowChosenColumns
//
//  Every column the user chose made visible, for the location in the list.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListColumns::ShowChosenColumns()
{
    const Location &  location = m_context.browser.GetLocation();



    for (size_t c = 0; c < m_chosen.size(); c++)
    {
        m_pane.GetListView()->SetColumnVisible (c, IsListColumnShown (c, m_chosen[c], location.kind, SearchQuery::IsId (location.path)));

        //  File Explorer's filters, on a folder's or a disk's list.
        m_pane.GetListView()->SetColumnFilter (c, CassoExplorerBrowser::GetFilterField (c) != RowGrouping::Field::None && location.kind != Location::Kind::Root,
                                               m_context.browser.IsFiltering (CassoExplorerBrowser::GetFilterField (c)));
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
//  A view chosen for the folder on display, and written to the preferences
//  for it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListColumns::SetView (DxuiListView::View view)
{
    m_view    = view;
    m_viewKey = GetFolderViewKey (nullptr);

    m_context.prefs.folderViews.Remember (m_viewKey, m_view);
    m_pane.GetListView()->SetView (m_view);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::IsListColumnShown
//
//  Whether a list column is visible: the user's choice from the header's
//  menu, and the catalog's two columns only inside a disk image or on a
//  linked drive.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListColumns::IsListColumnShown (size_t column, bool chosen, Location::Kind kind, bool searching)
{
    bool  catalog     = column == (size_t) CatalogModel::Column::Address || column == (size_t) CatalogModel::Column::Locked;
    bool  recycled    = column == (size_t) CatalogModel::Column::OriginalLocation || column == (size_t) CatalogModel::Column::DateDeleted;
    bool  insideImage = kind == Location::Kind::DiskImage || kind == Location::Kind::DiskDirectory || kind == Location::Kind::AppleDrive;
    bool  inBin       = kind == Location::Kind::RecycleBin;



    if (column == (size_t) CatalogModel::Column::Name)
    {
        return true;
    }

    //  A shell property, for the items of a folder on the PC.
    if (column >= CatalogModel::kColumnCount)
    {
        return chosen && (kind == Location::Kind::HostFolder || kind == Location::Kind::ShellFolder);
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
//  ExplorerListColumns::IsListColumnOffered
//
//  Whether the header's menu lists a column for the folder on display: one
//  that applies there, whatever the user chose.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListColumns::IsListColumnOffered (size_t column, Location::Kind kind, bool searching)
{
    return IsListColumnShown (column, true, kind, searching);
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
    size_t               count = m_listColumns.size();



    if (m_context.browser.GetLocation().kind == Location::Kind::RecycleBin)
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
    }

    for (size_t column = 0; column < count; column++)
    {
        if (std::find (order.begin(), order.end(), column) == order.end())
        {
            order.push_back (column);
        }
    }

    m_pane.GetListView()->SetColumnOrder (order);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::GetFolderViewKey
//
//  A host folder by its path, a place inside a disk image by the image and
//  the directory, and a root by its id.
//
//  The folder the list shows, as FolderViews keys it, and its type when
//  asked for, which a host folder costs a read of its desktop.ini.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerListColumns::GetFolderViewKey (FolderViews::FolderType * outType) const
{
    Location                 location = m_context.browser.GetLocation();
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
//  The list in the view of the folder it now shows, when that folder is
//  not the one the view was last set for.
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
    m_view    = m_context.prefs.folderViews.GetView (key, type);
    m_pane.GetListView()->SetView (m_view);
    m_context.toolbars.ShowViewOnButton();
    m_pane.SizeListIcons();

    //  Each folder keeps its own sort and grouping, as Explorer's do; one
    //  never given either opens by name and ungrouped.
    entry = m_context.prefs.folderViews.GetEntry (key, type);

    if (entry.sortColumn > (int) CatalogModel::Column::DateDeleted && !m_context.browser.IsPropertyColumn (entry.sortColumn))
    {
        entry.sortColumn = 0;
    }

    if (entry.groupBy > (int) RowGrouping::Field::Size)
    {
        entry.groupBy = 0;
    }

    m_context.browser.SetSortAndGroup ((CatalogModel::Column) entry.sortColumn, entry.sortDescending,
                                        (RowGrouping::Field) entry.groupBy, entry.groupDescending);

    ApplyColumnWidths (entry.columnWidthsDip.empty() ? m_context.prefs.columnWidthsDip : entry.columnWidthsDip);

    //  Its own column order and choice of columns, or the latest ones.
    m_folderOrder = entry.columnOrder.empty() ? m_context.prefs.columnOrder : entry.columnOrder;
    m_chosen.assign (m_listColumns.size(), true);

    for (int hidden : (entry.columnsChosen ? entry.hiddenColumns : m_context.prefs.hiddenColumns))
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



    if (m_viewKey.empty() || !m_context.browser.GetBrowserModel().HasTabs())
    {
        return;
    }

    const BrowserModel::Tab &  tab = m_context.browser.GetBrowserModel().GetActiveTab();

    entry                 = m_context.prefs.folderViews.GetEntry (m_viewKey, FolderViews::FolderType::Generic);
    entry.view            = m_view;
    entry.sortColumn      = (int) tab.sortColumn;
    entry.sortDescending  = tab.sortDescending;
    entry.groupBy         = (int) tab.groupBy;
    entry.groupDescending = tab.groupDescending;

    m_context.prefs.folderViews.Remember (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::ReadFolderOptions
//
//  Explorer's options for hidden items, read again. When one has changed,
//  the tree is listed again under them, and true is returned so the caller
//  relists the folder.
//
//  Explorer's folder options read again; true when one changed.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListColumns::ReadFolderOptions()
{
    FolderOptions  options = FolderOptions::ReadFromShell();



    if (options == m_context.explorerOptions)
    {
        return false;
    }

    //  The pane's options Casso Explorer still follows change with them.
    m_context.explorerOptions = options;
    m_context.browser.SetFolderOptions (options);
    m_context.treePane.ApplyNavPaneOptions();
    m_context.browser.GetTreeModel().InvalidateAll();
    m_context.treePane.RefreshTree();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::ApplyStoredColumnWidths
//
//  The widths from the last run, where there are any. A stored zero means the
//  column was never given a width of its own and still fits itself.
//
//  The file list's column widths from the last run.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListColumns::ApplyStoredColumnWidths()
{
    ApplyColumnWidths (m_context.prefs.columnWidthsDip);
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



    for (c = 0; c < m_pane.GetListView()->GetColumnCount(); c++)
    {
        int  dip = (c < widthsDip.size()) ? widthsDip[c] : 0;

        //  At the DPI the list holds now, which it rescales from when its
        //  DPI changes; the window's may not have caught up yet.
        m_pane.GetListView()->SetColumnOverrideWidthPx (c, (dip > 0) ? MulDiv (dip, (int) m_pane.GetListView()->GetDpi(), (int) DxuiDpiScaler::kBaseDpi) : -1);
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

    entry               = m_context.prefs.folderViews.GetEntry (m_viewKey, FolderViews::FolderType::Generic);
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

    m_context.prefs.folderViews.Remember (entry);
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

    entry      = m_context.prefs.folderViews.GetEntry (m_viewKey, FolderViews::FolderType::Generic);
    entry.view = m_view;

    if (entry.columnWidthsDip.empty())
    {
        entry.columnWidthsDip = m_context.prefs.columnWidthsDip;
    }

    if ((size_t) column >= entry.columnWidthsDip.size())
    {
        entry.columnWidthsDip.resize ((size_t) column + 1, 0);
    }

    entry.columnWidthsDip[(size_t) column] = widthDip;

    m_context.prefs.folderViews.Remember (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::LoadPropertyColumns
//
//  A property missing from the shell's property system gets a column
//  titled by its canonical name, so the indexes of the columns after it
//  stay the same.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListColumns::LoadPropertyColumns()
{
    const std::vector<std::wstring> &  names = m_context.prefs.propertyColumns;



    m_listColumns = BrowserCells::GetColumns();

    for (const std::wstring & name : names)
    {
        Win32PropertyColumns::Property  property;
        DxuiListView::Column            column;

        if (!Win32PropertyColumns::TryDescribe (name, property))
        {
            property.title = name;
        }

        column.title    = property.title;
        column.widthDip = property.widthDip;
        column.align    = property.rightAligned ? DxuiTextHAlign::Right : DxuiTextHAlign::Left;

        m_listColumns.push_back (std::move (column));
    }

    m_pane.GetListView()->SetColumns (m_listColumns);
    m_chosen.resize (m_listColumns.size(), true);
    m_pane.GetPropertyValues().SetProperties (names);

    //  A sort by a property reads the values it lacks at once.
    m_context.browser.SetPropertyColumns (names.size(), [this] (size_t row, size_t property)
    {
        const std::vector<CatalogRow> &  rows = m_context.browser.GetRows();
        CatalogModel::PropertyKey        key;
        std::wstring                     path;

        if (row < rows.size() && m_context.browser.TryGetRowPath ((int) row, path))
        {
            Win32PropertyColumns::Values  values = m_pane.GetPropertyValues().ReadNow (path, ExplorerListPane::GetPropertyStamp (rows[row]));

            if (property < values.size())
            {
                key = values[property];
            }
        }

        return key;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListColumns::ChooseDetails
//
//  File Explorer's Choose Details, for the folder on display. A property
//  added becomes a column after the last, hidden in every other folder with
//  a saved choice of columns, as in File Explorer, where a folder's columns
//  apply to it alone. The shell's properties for the fixed columns aren't
//  listed a second time.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListColumns::ChooseDetails()
{
    static constexpr const wchar_t *  s_kOwnColumns[] = { L"System.ItemNameDisplay", L"System.DateModified", L"System.ItemTypeText", L"System.Size",
                                                          L"System.Recycle.DeletedFrom", L"System.Recycle.DateDeleted", L"System.ItemFolderPathDisplay" };
    const Location &                         location  = m_context.browser.GetLocation();
    bool                                     searching = SearchQuery::IsId (location.path);
    std::vector<ChooseDetailsModel::Column>  columns;
    std::vector<std::wstring>                names     = m_context.prefs.propertyColumns;
    ChooseDetailsModel                       model;
    ChooseDetailsModel::Result               result;
    std::vector<FolderViewEntry>             entries;
    size_t                                   first     = m_listColumns.size();



    for (size_t c = 0; c < m_listColumns.size(); c++)
    {
        ChooseDetailsModel::Column  column;

        column.title    = m_listColumns[c].title;
        column.offered  = IsListColumnOffered (c, location.kind, searching);
        column.chosen   = c < m_chosen.size() && m_chosen[c];
        column.fixed    = c == (size_t) CatalogModel::Column::Name;
        column.widthDip = MulDiv (m_pane.GetListView()->GetColumnEffectiveWidthPx (c), (int) DxuiDpiScaler::kBaseDpi, (int) m_pane.GetListView()->GetDpi());

        columns.push_back (std::move (column));
    }

    names.insert (names.end(), std::begin (s_kOwnColumns), std::end (s_kOwnColumns));
    model.Build (columns, m_pane.GetListView()->GetColumnOrder(), names, Win32PropertyColumns::ListViewable());

    if (!ChooseDetailsDialog::Run (m_context.window.GetHwnd(), m_context.theme, model))
    {
        return;
    }

    result = model.GetResult();

    if (!result.added.empty())
    {
        entries = m_context.prefs.folderViews.GetEntries();

        for (FolderViewEntry & entry : entries)
        {
            for (size_t added = 0; entry.columnsChosen && entry.key != m_viewKey && added < result.added.size(); added++)
            {
                entry.hiddenColumns.push_back ((int) (first + added));
            }
        }

        m_context.prefs.folderViews.SetEntries (std::move (entries));
        m_context.prefs.propertyColumns.insert (m_context.prefs.propertyColumns.end(), result.added.begin(), result.added.end());
        LoadPropertyColumns();
    }

    //  This folder's choice, and the default for folders without a saved one.
    m_chosen      = result.chosen;
    m_folderOrder = result.order;
    m_context.prefs.columnOrder = result.order;
    m_context.prefs.hiddenColumns.clear();

    for (size_t column = 0; column < m_chosen.size(); column++)
    {
        if (!m_chosen[column])
        {
            m_context.prefs.hiddenColumns.push_back ((int) column);
        }
    }

    RememberFolderColumns();

    for (const auto & [column, widthDip] : result.widths)
    {
        if ((size_t) column >= m_context.prefs.columnWidthsDip.size())
        {
            m_context.prefs.columnWidthsDip.resize ((size_t) column + 1, 0);
        }

        m_context.prefs.columnWidthsDip[(size_t) column] = widthDip;
        RememberFolderColumnWidths (column, widthDip);
    }

    //  The folder's view applied again, with its new columns.
    m_viewKey.clear();
    m_pane.FillList();
}
