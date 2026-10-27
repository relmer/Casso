#include "Pch.h"

#include "CassoExplorer/Window/ExplorerListPane.h"
#include "CassoExplorer/CassoExplorerIcons.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoLink/LinkAudioBootstrap.h"
#include "CassoLink/RunningCassoRegistry.h"
#include "Seams/ILinkAnnouncements.h"
#include "Seams/Win32FirewallHelper.h"
#include "Net/FirewallMonitor.h"
#include "Net/FirewallText.h"
#include "Devices/Disk/DiskImageStore.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoExplorer/CassoExplorerAbout.h"
#include "CassoExplorer/CassoExplorerDragOut.h"
#include "CassoExplorer/CassoExplorerLinkDialog.h"
#include "CassoExplorer/CassoExplorerNewDiskDialog.h"
#include "CassoExplorer/CassoExplorerProperties.h"
#include "CassoExplorer/CassoExplorerRawDialog.h"
#include "CassoExplorer/CassoExplorerOptionsDialog.h"
#include "CassoExplorer/Model/CassoTargeting.h"
#include "CassoExplorer/CassoExplorerPromptDialog.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/KnownFolderStore.h"
#include "CassoExplorer/Model/LaunchCommand.h"
#include "AssetBootstrap.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "Config/FileAssociations.h"
#include "Config/GlobalUserPrefs.h"
#include "Devices/Disk/ChangePrompt.h"
#include "Seams/Win32DiskFileIo.h"
#include "Seams/Win32UserClasses.h"
#include "Config/UserConfigStore.h"
#include "Config/Win32FileSystem.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Core/MachineConfig.h"
#include "Core/MachineScanner.h"
#include "Core/PathResolver.h"
#include "Core/TextEncoding.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::ExplorerListPane
//
////////////////////////////////////////////////////////////////////////////////

ExplorerListPane::ExplorerListPane (CassoExplorerWindow & window)
    : m_window (window)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::RevealLocationInTree
//
//  Expands This PC, or with the all-folders option on the desktop, down to the
//  current location's folder, highlights it and expands it, as File Explorer's
//  navigation pane does when a window opens on a folder.
//  Each path component is matched against the child labels without regard to
//  case, since a restored path need not match the case on disk.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::RevealLocationInTree()
{
    Location              location = m_window.m_browser.GetLocation();
    std::wstring          path     = location.path;
    std::wstring          current  = m_window.m_tree->GetHighlightedId();
    int                   casso    = m_window.m_tree->FindRowById (TreeModel::kCassoRootId);
    int                   desktop  = m_window.m_tree->FindRowById (TreeModel::kDesktopRootId);
    size_t                prefix   = wcslen (TreeModel::kCassoRootId);
    size_t                pcPrefix = wcslen (TreeModel::kThisPcRootId);
    const DxuiTreeNode *  root     = nullptr;
    bool                  wasOpen  = false;
    size_t                known    = 0;
    std::wstring          knownId;
    int                   row      = -1;
    std::wstring          link;
    int                   slot     = 0;
    int                   drive    = 0;



    //  A root is a node of its own. A CassoLink connection or one of its
    //  drives is under CassoLink, which is opened down to it.
    if (location.kind == Location::Kind::Root)
    {
        if (TreeModel::TryParseLinkId (location.path, link, slot, drive) && TreeModel::StripLinkCopy (current) != location.path)
        {
            m_window.m_tree->SetRowExpanded (m_window.m_tree->FindRowById (TreeModel::kCassoLinkRootId), true);

            if (drive != 0)
            {
                m_window.m_tree->SetRowExpanded (m_window.m_tree->FindRowById (TreeModel::MakeLinkId (link)), true);
            }
        }

        row = m_window.m_tree->FindRowById (location.path);

        if (row >= 0 && TreeModel::StripLinkCopy (current) != location.path)
        {
            m_window.m_tree->HighlightRow (row);
            ScrollTreeToRow (row);
        }

        return;
    }

    if (location.kind == Location::Kind::None || path.size() < 2 || path[1] != L':')
    {
        return;
    }

    //  A node the user clicked already shows the location, possibly under the
    //  Casso root rather than This PC, so it stays highlighted, opened, and in
    //  view.
    if (current.size() >= path.size() && _wcsicmp (current.c_str() + current.size() - path.size(), path.c_str()) == 0)
    {
        m_window.m_tree->SetRowExpanded (m_window.m_tree->GetHighlight(), true);
        ScrollTreeToRow (m_window.m_tree->GetHighlight());
        return;
    }

    //  A click on an ADTPro server's folder node, under its connection,
    //  highlights that node, so no other is searched for.
    if (TreeModel::StripLinkCopy (current) == TreeModel::kAdtProId + path)
    {
        ScrollTreeToRow (m_window.m_tree->GetHighlight());
        return;
    }

    //  Casso's known folders come first: the deepest one that holds the
    //  location is where the walk starts. The Casso root is closed again when
    //  none does and it was closed before.
    if (casso >= 0)
    {
        root    = m_window.m_tree->GetNodeAt (casso);
        wasOpen = (root != nullptr) && root->expanded;

        m_window.m_tree->SetRowExpanded (casso, true);
        root = m_window.m_tree->GetNodeAt (m_window.m_tree->FindRowById (TreeModel::kCassoRootId));

        for (size_t i = 0; root != nullptr && i < root->children.size(); i++)
        {
            const std::wstring &  id     = root->children[i].id;
            std::wstring          folder = (id.size() > prefix) ? id.substr (prefix) : std::wstring();
            bool                  within = !folder.empty() && folder.size() > known
                                        && _wcsnicmp (path.c_str(), folder.c_str(), folder.size()) == 0
                                        && (path.size() == folder.size() || path[folder.size()] == L'\\' || folder.back() == L'\\');

            if (within)
            {
                known   = folder.size();
                knownId = id;
            }
        }

        if (knownId.empty() && !wasOpen)
        {
            m_window.m_tree->SetRowExpanded (m_window.m_tree->FindRowById (TreeModel::kCassoRootId), false);
        }
    }

    //  With the all-folders option on, This PC is under the desktop, beside the
    //  desktop's own folders, as the user's and OneDrive's; the deepest of
    //  those that contains the location is where the walk starts.
    if (knownId.empty() && desktop >= 0)
    {
        m_window.m_tree->SetRowExpanded (desktop, true);
        root = m_window.m_tree->GetNodeAt (m_window.m_tree->FindRowById (TreeModel::kDesktopRootId));

        for (size_t i = 0; root != nullptr && i < root->children.size(); i++)
        {
            const std::wstring &  id     = root->children[i].id;
            std::wstring          folder = (id.size() > pcPrefix && id.starts_with (TreeModel::kThisPcRootId)) ? id.substr (pcPrefix) : std::wstring();
            bool                  within = !folder.empty() && folder.size() > known
                                        && _wcsnicmp (path.c_str(), folder.c_str(), folder.size()) == 0
                                        && (path.size() == folder.size() || path[folder.size()] == L'\\' || folder.back() == L'\\');

            if (within)
            {
                known   = folder.size();
                knownId = id;
            }
        }
    }

    row = knownId.empty() ? WalkTreeLabels (m_window.m_tree->FindRowById (TreeModel::kThisPcRootId), path)
                          : WalkTreeLabels (m_window.m_tree->FindRowById (knownId), path.substr (known));

    if (row < 0)
    {
        return;
    }

    //  The open folder's own row is opened too, as Explorer's is.
    m_window.m_tree->HighlightRow (row);
    m_window.m_tree->SetRowExpanded (row, true);
    ScrollTreeToRow (row);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::ScrollTreeToRow
//
//  Into view now, or at the first layout when the tree has no height yet.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::ScrollTreeToRow (int row)
{

    if (m_window.m_tree->GetRowCap() > 0)
    {
        m_window.m_tree->EnsureRowVisible (row);
    }
    else
    {
        m_window.m_treeRevealPending = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::WalkTreeLabels
//
//  Expands down from a row one path component at a time, matching each against
//  the child labels without regard to case, and returns the deepest row
//  reached. A drive's label is its volume name, so a drive is matched by the
//  letter its id ends with instead.
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerListPane::WalkTreeLabels (int row, const std::wstring & path)
{
    size_t  start = 0;



    if (row < 0)
    {
        return -1;
    }

    while (start < path.size())
    {
        size_t                slash = path.find (L'\\', start);
        std::wstring          label = path.substr (start, (slash == std::wstring::npos) ? std::wstring::npos : slash - start);
        const DxuiTreeNode *  node  = nullptr;
        int                   next  = -1;

        start = (slash == std::wstring::npos) ? path.size() : slash + 1;

        if (label.empty())
        {
            continue;
        }

        m_window.m_tree->SetRowExpanded (row, true);
        node = m_window.m_tree->GetNodeAt (row);

        for (size_t i = 0; node != nullptr && i < node->children.size() && next < 0; i++)
        {
            const std::wstring &  childLabel = node->children[i].label;
            bool                  isDrive    = label.size() == 2 && label[1] == L':';

            if (isDrive ? (_wcsnicmp (node->children[i].id.c_str() + wcslen (TreeModel::kThisPcRootId), label.c_str(), 2) == 0)
                        : (_wcsicmp (childLabel.c_str(), label.c_str()) == 0))
            {
                next = m_window.m_tree->FindRowById (node->children[i].id);
            }
        }

        if (next < 0)
        {
            break;
        }

        row = next;
    }

    return row;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::FillList
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::FillList()
{
    const BrowserModel                            & model   = m_window.m_browser.GetBrowserModel();
    std::wstring                                    message = m_window.m_browser.GetListError();
    size_t                                          count   = m_window.m_browser.GetRows().size();
    bool                                            changed = false;



    m_window.m_links.ReleaseIdleLinks();

    //  The rows are about to change under it.
    if (m_window.m_renameRow >= 0)
    {
        m_window.m_fileOps.EndRename (false);
    }

    //  Before the cells are built, since their icons are fetched at the
    //  view's size.
    ApplyFolderView();

    m_window.m_layout.UpdateCommandBar();

    //  A file's size or date may have changed with the listing.
    m_window.m_infoTips.Clear();
    m_contentDetails.Clear();

    //  The rows' cells are built as the list first asks for each, so a folder
    //  of thousands shows as fast as one of ten.
    m_rowCells.assign    (count, {});
    m_rowBuilt.assign    (count, false);
    m_window.m_rowProblems.assign (count, std::wstring());
    m_rowLocation = m_window.m_browser.GetLocation();
    m_rowDark     = DxuiColor::ComputeRelativeLuminance (m_window.m_theme->Background()) < 0.5f;

    m_window.m_list->SetRowSource ((int) count, [this] (int row) -> const std::vector<DxuiListView::Cell> & { return GetListRowCells (row); });

    //  Small icons and List measure every name; this reads them without
    //  building the rows' cells.
    m_window.m_list->SetRowNameSource ([this] (int row)
    {
        return (row >= 0 && (size_t) row < m_window.m_browser.GetRows().size()) ? m_window.m_browser.GetRows()[(size_t) row].name : std::wstring();
    });
    m_window.m_list->SetGroups    (m_window.m_browser.GetListGroups());

    //  A new location opens at its top-left corner, as Explorer's does, rather
    //  than wherever the list was scrolled for the last one. THE COLUMNS STAY AS
    //  THEY ARE: their widths belong to the view rather than to the folder, so
    //  they neither twitch from one folder to the next nor pay to re-measure
    //  every cell of every row on each navigation.
    if (m_window.m_browser.GetLocation() != m_listLocation)
    {
        m_listLocation = m_window.m_browser.GetLocation();
        {
            Location  lead = m_window.m_addressBar.GetAddressSegmentsFor (m_listLocation).front().location;

            m_window.m_address->SetLeadIcon  (BrowserCells::GetLocationIcon (lead, m_window.m_shellIcons));
            m_window.m_address->SetLeadGlyph ((lead.kind == Location::Kind::Root && lead.path == TreeModel::kThisPcRootId) ? s_kpszMdl2ThisPc : nullptr);
        }

        m_window.m_menus.SetCommandBarDropDowns();

        //  Explorer's box says what it will search, and empties on leaving
        //  a search's results.
        if (!SearchQuery::IsId (m_listLocation.path))
        {
            std::vector<BrowserModel::AddressSegment>  segments = BrowserModel::GetAddressSegments (m_listLocation, m_window.m_addressRoot);

            m_window.m_findBox->SetText        (L"");
            m_window.m_findBox->SetPlaceholder (L"Search " + (segments.empty() ? std::wstring() : segments.back().label));
        }
        else
        {
            //  A search's results, a restored tab's among them, show its query
            //  over the folder it searched.
            std::wstring  scope;
            std::wstring  query;
            Location      up;

            SearchQuery::TryParseId (m_listLocation.path, scope, query);
            m_window.m_findBox->SetText        (query);
            m_window.m_findBox->SetPlaceholder (L"Search " + (m_window.m_browser.TryGetUpTarget (up) ? m_window.m_addressBar.GetNavLabel (up) : std::wstring()));
        }

        m_window.m_list->SetTopRow (0);
        m_window.m_list->SetLeftPx (0);

        if (m_window.m_menus.IsNavOptionOn (&CassoExplorerPrefs::navExpandToCurrent))
        {
            RevealLocationInTree();
        }

        m_window.m_folderSync.UpdateWatchedFolders();
        changed = true;
    }

    //  A new location or a new tab is saved as the list is filled for it, so a
    //  process that ends without closing reopens on it.
    if (changed || model.GetTabCount() != m_listTabCount)
    {
        m_listTabCount = model.GetTabCount();
        m_window.SaveSession();
    }

    //  Address and Locked are a disk catalog's; a host folder has neither, so
    //  its list shows only Explorer's four columns. The Recycle Bin adds its
    //  own two.
    ApplyColumnOrder();

    //  Explorer's Recycle Bin titles the type column Item type.
    m_window.m_list->SetColumnTitle ((size_t) CatalogModel::Column::Type, (m_window.m_browser.GetLocation().kind == Location::Kind::RecycleBin) ? L"Item type" : L"Type");

    for (size_t c = 0; c < m_window.m_listColumnChosen.size(); c++)
    {
        m_window.m_list->SetColumnVisible (c, IsListColumnShown (c, m_window.m_listColumnChosen[c], m_window.m_browser.GetLocation().kind, SearchQuery::IsId (m_window.m_browser.GetLocation().path)));
    }

    m_window.m_list->UpdateAutoFitFromRows();

    m_window.m_list->SetSelectedRows (m_window.m_browser.GetSelectedRows(), m_window.m_browser.GetSelectedRows().empty() ? -1 : m_window.m_browser.GetSelectedRows()[0]);


    if (model.HasTabs())
    {
        m_window.m_list->SetSortIndicator ((int) model.GetActiveTab().sortColumn, model.GetActiveTab().sortDescending);
    }

    //  An empty search's message is File Explorer's, at the top of the list;
    //  any other empty location's is centered in the pane.
    m_window.m_listMessageTop = false;

    if (message.empty() && m_window.m_browser.GetRows().empty() && m_window.m_browser.GetLocation().kind != Location::Kind::None)
    {
        m_window.m_listMessageTop = SearchQuery::IsId (m_window.m_browser.GetLocation().path);
        message                   = m_window.m_listMessageTop ? std::wstring (kNoSearchMatches) : GetEmptyLocationMessage (m_window.m_browser.GetLocation().kind);
    }

    m_window.m_listMessage->SetTextAlign (DxuiTextHAlign::Center, m_window.m_listMessageTop ? DxuiTextVAlign::Top : DxuiTextVAlign::Center);
    m_window.m_listMessage->Layout       (GetListMessageRect(), m_window.m_scaler);

    m_window.m_listMessage->SetText (message);
    m_window.m_listMessage->SetVisible (!message.empty());
    m_window.m_list->SetVisible (message.empty());

    m_window.m_links.FillListAction (!message.empty());

    m_window.m_tabStrip.FillTabs();
    m_window.m_addressBar.FillAddress();
    m_window.m_addressBar.FillNavTips();
    m_window.m_previewPane.FillPreview();
    m_window.m_layout.FillStatus();
    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::GetListRowCells
//
//  A row's cells, built the first time the list asks for them.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<DxuiListView::Cell> & ExplorerListPane::GetListRowCells (int row)
{
    static const std::vector<DxuiListView::Cell>  s_kNone;
    const std::vector<CatalogRow>               & rows = m_window.m_browser.GetRows();
    std::wstring                                  path;



    if (row < 0 || (size_t) row >= m_rowCells.size() || (size_t) row >= rows.size())
    {
        return s_kNone;
    }

    if (!m_rowBuilt[(size_t) row])
    {
        std::vector<DxuiListView::Cell> & cells = m_rowCells[(size_t) row];

        cells         = BrowserCells::ToCells (rows[(size_t) row], m_rowLocation, &m_listIcons);
        cells[0].argb = BrowserCells::GetNameArgb (rows[(size_t) row], m_window.m_browser.GetFolderOptions(), m_rowDark);

        //  Cut and not yet pasted, it draws dimmed, as Explorer's does.
        if (!m_window.m_cutPaths.empty() && m_window.m_browser.TryGetRowPath (row, path) &&
            std::any_of (m_window.m_cutPaths.begin(), m_window.m_cutPaths.end(), [&] (const std::wstring & cut) { return ExplorerFolderSync::IsSameFolder (cut, path); }))
        {
            cells[0].iconGhosted = true;
        }

        //  A disk image that is not one says so on its icon.
        if (rows[(size_t) row].isDiskImage && m_rowLocation.kind == Location::Kind::HostFolder && m_window.m_browser.TryGetRowPath (row, path))
        {
            m_window.m_rowProblems[(size_t) row] = m_window.m_browser.GetImageProblem (path);
            cells[0].iconBroken                  = !m_window.m_rowProblems[(size_t) row].empty();
        }

        //  In Content view a host item has the lines the shell lists for its
        //  kind, once read; until then, its date and size.
        TryApplyContentDetails (row, cells[0]);

        m_rowBuilt[(size_t) row] = true;
    }

    return m_rowCells[(size_t) row];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::TryApplyContentDetails
//
//  A host item's Content lines from the shell, when read: true then. A
//  search result has its folder and type, and a drive its type, as in
//  File Explorer's Content view.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListPane::TryApplyContentDetails (int row, DxuiListView::Cell & name)
{
    const std::vector<CatalogRow> &  rows = m_window.m_browser.GetRows();
    Win32ContentDetails::Slots       slots;
    std::wstring                     path;
    bool                             host = false;



    if (m_window.m_listView != DxuiListView::View::Content || row < 0 || (size_t) row >= rows.size())
    {
        return false;
    }

    host = rows[(size_t) row].folderPath.empty() && !rows[(size_t) row].isDrive
        && (m_rowLocation.kind == Location::Kind::HostFolder || !rows[(size_t) row].hostPath.empty());

    if (!host || !m_window.m_browser.TryGetRowPath (row, path) || !m_contentDetails.TryGet (path, slots))
    {
        return false;
    }

    name.contentLeft   = { BrowserCells::MakeContentLine (slots[0].label, slots[0].value) };
    name.contentMiddle = { BrowserCells::MakeContentLine (slots[1].label, slots[1].value),
                           BrowserCells::MakeContentLine (slots[2].label, slots[2].value) };
    name.contentRight  = { BrowserCells::MakeContentLine (slots[3].label, slots[3].value),
                           BrowserCells::MakeContentLine (slots[4].label, slots[4].value) };

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::RefreshContentDetails
//
//  The built rows' Content lines again, now that more have been read.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::RefreshContentDetails()
{
    for (size_t at = 0; at < m_rowCells.size(); at++)
    {
        if (m_rowBuilt[at] && !m_rowCells[at].empty())
        {
            TryApplyContentDetails ((int) at, m_rowCells[at][0]);
        }
    }

    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::RefreshListIcons
//
//  The rows' icons again, now that more have loaded; nothing else about the
//  rows changes, so a rename in progress carries on.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::RefreshListIcons()
{
    const std::vector<CatalogRow> & rows = m_window.m_browser.GetRows();



    for (size_t at = 0; at < m_rowCells.size() && at < rows.size(); at++)
    {
        if (m_rowBuilt[at] && !m_rowCells[at].empty())
        {
            m_rowCells[at][0].icon = BrowserCells::GetRowIcon (rows[at], m_rowLocation, m_listIcons);
        }
    }

    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::GetListMessageRect
//
//  The list's message, over the list; a search's sits a line below the top
//  of the list, as File Explorer's does.
//
////////////////////////////////////////////////////////////////////////////////

RECT ExplorerListPane::GetListMessageRect() const
{
    RECT  rect = m_window.m_layout.GetMessageRect (m_window.m_list->GetBounds());



    if (m_window.m_listMessageTop)
    {
        rect.top += m_window.m_scaler.ToPx (kSearchMessageTopDip);
    }

    //  The firewall's message runs to several lines, so it ends at the
    //  pane's middle, with the actions below it.
    if (m_window.m_listActionFirewall)
    {
        rect.bottom = (rect.top + rect.bottom) / 2;
    }

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::IsListColumnShown
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListPane::IsListColumnShown (size_t column, bool chosen, Location::Kind kind, bool searching)
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
//  ExplorerListPane::ApplyColumnOrder
//
//  The order the user dragged the headers into, with any column it predates
//  at the end; in the Recycle Bin, Explorer's order for it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::ApplyColumnOrder()
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
        for (int column : m_window.m_folderColumnOrder)
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

    m_window.m_list->SetColumnOrder (order);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::SizeListIcons
//
//  The list's icons are drawn at its view's size, so they are fetched at that
//  size rather than scaled up from the tree's. A drag's image follows the
//  same DPI.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::SizeListIcons()
{
    UINT  dpi = GetDpiForWindow (m_window.GetHwnd());



    m_listIcons.SetDpi    (dpi);
    m_listIcons.SetSizePx ((m_window.m_listView == DxuiListView::View::Details) ? MulDiv (DxuiTreeView::s_kIconDip, (int) dpi, (int) DxuiDpiScaler::kBaseDpi)
                                                                        : DxuiListView::GetItemIconPx (m_window.m_listView, dpi));

    m_window.m_dragIcons.SetDpi    (dpi);
    m_window.m_dragIcons.SetSizePx (MulDiv (kDragImageDip, (int) dpi, (int) DxuiDpiScaler::kBaseDpi));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::SelectRowNamed
//
//  False when no row has the name, as when the item was not made.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListPane::SelectRowNamed (const std::wstring & name)
{
    const std::vector<CatalogRow> &  rows = m_window.m_browser.GetRows();



    for (size_t i = 0; i < rows.size(); i++)
    {
        if (_wcsicmp (rows[i].name.c_str(), name.c_str()) == 0)
        {
            m_window.m_browser.SetSelectedRows ({ (int) i });
            m_window.m_list->SetSelectedRows ({ (int) i }, (int) i);
            m_window.m_list->EnsureVisible ((int) i);

            //  As a click on the row would: the preview, the status bar's
            //  count and the command bar follow the new selection.
            m_window.m_previewPane.FillPreview();
            m_window.m_layout.FillStatus();
            m_window.m_layout.UpdateCommandBar();
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::GetFolderViewKey
//
//  A host folder by its path, a place inside a disk image by the image and
//  the directory, and a root by its id.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerListPane::GetFolderViewKey (FolderViews::FolderType * outType) const
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
//  ExplorerListPane::ApplyFolderView
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::ApplyFolderView()
{
    FolderViews::FolderType  type  = FolderViews::FolderType::Generic;
    std::wstring             key   = GetFolderViewKey (nullptr);
    FolderViewEntry          entry;



    if (key == m_window.m_listViewKey)
    {
        return;
    }

    GetFolderViewKey (&type);

    m_window.m_listViewKey = key;
    m_window.m_listView    = m_window.m_prefs.folderViews.GetView (key, type);
    m_window.m_list->SetView (m_window.m_listView);
    m_window.m_menus.ShowViewOnButton();
    SizeListIcons();

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
    m_window.m_folderColumnOrder = entry.columnOrder.empty() ? m_window.m_prefs.columnOrder : entry.columnOrder;
    m_window.m_listColumnChosen.assign (BrowserCells::GetColumns().size(), true);

    for (int hidden : (entry.columnsChosen ? entry.hiddenColumns : m_window.m_prefs.hiddenColumns))
    {
        if (hidden > (int) CatalogModel::Column::Name && (size_t) hidden < m_window.m_listColumnChosen.size())
        {
            m_window.m_listColumnChosen[(size_t) hidden] = false;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::RememberFolderSort
//
//  Kept only when the user sorts or groups, so a folder merely visited keeps
//  following the defaults.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::RememberFolderSort()
{
    FolderViewEntry  entry;



    if (m_window.m_listViewKey.empty() || !m_window.m_browser.GetBrowserModel().HasTabs())
    {
        return;
    }

    const BrowserModel::Tab &  tab = m_window.m_browser.GetBrowserModel().GetActiveTab();

    entry                 = m_window.m_prefs.folderViews.GetEntry (m_window.m_listViewKey, FolderViews::FolderType::Generic);
    entry.view            = m_window.m_listView;
    entry.sortColumn      = (int) tab.sortColumn;
    entry.sortDescending  = tab.sortDescending;
    entry.groupBy         = (int) tab.groupBy;
    entry.groupDescending = tab.groupDescending;

    m_window.m_prefs.folderViews.Remember (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::ReadFolderOptions
//
//  Explorer's options for hidden items, read again. When one has changed,
//  the tree is listed again under them, and true is returned so the caller
//  relists the folder.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListPane::ReadFolderOptions()
{
    FolderOptions  options = FolderOptions::ReadFromShell();



    if (options == m_window.m_explorerOptions)
    {
        return false;
    }

    //  The pane's options Casso Explorer still follows change with them.
    m_window.m_explorerOptions = options;
    m_window.m_browser.SetFolderOptions (options);
    m_window.m_menus.ApplyNavPaneOptions();
    m_window.m_browser.GetTreeModel().InvalidateAll();
    RefreshTree();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::OnShellListed
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::OnShellListed (const std::wstring & key)
{
    std::wstring               listKey  = CassoExplorerBrowser::s_kListKey;
    std::wstring               treeKey  = CassoExplorerBrowser::s_kTreeKey;
    std::wstring               hostKey  = CassoExplorerBrowser::s_kHostListKey;
    std::wstring               hostTree = CassoExplorerBrowser::s_kHostTreeKey;
    std::wstring               nodeId;
    HRESULT                    hr       = S_OK;
    std::vector<std::wstring>  paths;



    if (key.starts_with (hostTree))
    {
        for (bool underCasso : { true, false })
        {
            nodeId = TreeModel::MakeFolderId (underCasso, key.substr (hostTree.size()));

            if (m_window.m_tree->FindRowById (nodeId) >= 0)
            {
                m_window.m_browser.GetTreeModel().Invalidate (nodeId);
                m_window.m_tree->ReplaceChildren (nodeId, m_window.m_browser.GetTreeChildren (nodeId));
            }
        }
    }
    else if (key.starts_with (hostKey))
    {
        if (m_window.m_browser.GetLocation().kind == Location::Kind::HostFolder && m_window.m_browser.GetLocation().path == key.substr (hostKey.size()))
        {
            hr = m_window.m_browser.Refresh();
            IGNORE_RETURN_VALUE (hr, S_OK);
            FillList();
        }
    }
    else if (key.starts_with (CassoExplorerBrowser::s_kPreviewKey))
    {
        //  An archive's contents, read on a network, for the preview of the
        //  archive selected, if it is the selection.
        m_window.m_browser.GetSelectedHostPaths (paths);

        if (paths.size() == 1 && paths[0] == key.substr (wcslen (CassoExplorerBrowser::s_kPreviewKey)))
        {
            m_window.m_browser.RefreshPreview();
            m_window.m_previewPane.FillPreview();
        }
    }
    else if (key.starts_with (listKey))
    {
        if (m_window.m_browser.GetLocation().kind == Location::Kind::ShellFolder && m_window.m_browser.GetLocation().path == key.substr (listKey.size()))
        {
            hr = m_window.m_browser.Refresh();
            IGNORE_RETURN_VALUE (hr, S_OK);
            FillList();
        }
    }
    else if (key.starts_with (treeKey))
    {
        nodeId = std::wstring (TreeModel::kShellRootTag) + key.substr (treeKey.size());

        if (m_window.m_tree->FindRowById (nodeId) >= 0)
        {
            m_window.m_browser.GetTreeModel().Invalidate (nodeId);
            m_window.m_tree->ReplaceChildren (nodeId, m_window.m_browser.GetTreeChildren (nodeId));
        }
    }

    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::RefreshTree
//
//  Reads the whole tree again in place, as Explorer's F5 does: the roots, then
//  each open node in row order, which puts every parent ahead of its children.
//  What is open, highlighted and on screen stays, by the tree's own merge.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::RefreshTree()
{
    std::vector<std::wstring>  open;
    std::vector<DxuiTreeNode>  roots;
    int                        row = 0;



    for (row = 0; row < m_window.m_tree->GetVisibleCount(); row++)
    {
        const DxuiTreeNode *  node = m_window.m_tree->GetNodeAt (row);

        if (node != nullptr && node->expanded)
        {
            open.push_back (node->id);
        }
    }

    m_window.m_browser.GetTreeModel().InvalidateAll();
    m_window.m_browser.GetTreeRoots (roots);
    m_window.m_tree->ReplaceRoots (std::move (roots));

    for (const std::wstring & id : open)
    {
        if (m_window.m_tree->FindRowById (id) >= 0)
        {
            m_window.m_tree->ReplaceChildren (id, m_window.m_browser.GetTreeChildren (id));
        }
    }

    m_window.m_folderSync.UpdateWatchedFolders();
    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::ApplyStoredColumnWidths
//
//  The widths from the last run, where there are any. A stored zero means the
//  column was never given a width of its own and still fits itself.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::ApplyStoredColumnWidths()
{
    ApplyColumnWidths (m_window.m_prefs.columnWidthsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::ApplyColumnWidths
//
//  A column with no width here goes back to fitting itself, so one folder's
//  widths do not stay on in the next.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::ApplyColumnWidths (const std::vector<int> & widthsDip)
{
    size_t  c = 0;



    for (c = 0; c < m_window.m_list->GetColumnCount(); c++)
    {
        int  dip = (c < widthsDip.size()) ? widthsDip[c] : 0;

        //  At the DPI the list holds now, which it rescales from when its
        //  DPI changes; the window's may not have caught up yet.
        m_window.m_list->SetColumnOverrideWidthPx (c, (dip > 0) ? MulDiv (dip, (int) m_window.m_list->GetDpi(), (int) DxuiDpiScaler::kBaseDpi) : -1);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::RememberFolderColumns
//
//  The order and the choice of columns the folder shows now, kept for it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::RememberFolderColumns()
{
    FolderViewEntry  entry;



    if (m_window.m_listViewKey.empty())
    {
        return;
    }

    entry               = m_window.m_prefs.folderViews.GetEntry (m_window.m_listViewKey, FolderViews::FolderType::Generic);
    entry.view          = m_window.m_listView;
    entry.columnOrder   = m_window.m_folderColumnOrder;
    entry.columnsChosen = true;
    entry.hiddenColumns.clear();

    for (size_t column = 0; column < m_window.m_listColumnChosen.size(); column++)
    {
        if (!m_window.m_listColumnChosen[column])
        {
            entry.hiddenColumns.push_back ((int) column);
        }
    }

    m_window.m_prefs.folderViews.Remember (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::RememberFolderColumnWidths
//
//  The folder starts from the widths it showed, so the columns the user did
//  not touch keep what they had.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::RememberFolderColumnWidths (int column, int widthDip)
{
    FolderViewEntry  entry;



    if (m_window.m_listViewKey.empty() || column < 0)
    {
        return;
    }

    entry      = m_window.m_prefs.folderViews.GetEntry (m_window.m_listViewKey, FolderViews::FolderType::Generic);
    entry.view = m_window.m_listView;

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





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::GetEmptyLocationMessage
//
//  What an empty list says, in the words the holding file system uses. ProDOS
//  calls them directories, which is what the command line's mkdir and rmdir
//  make, and Windows calls them folders. An empty disk image is worth telling
//  apart from either: it holds nothing at all, where an empty folder says
//  nothing about the rest of the disk.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerListPane::GetEmptyLocationMessage (Location::Kind kind)
{
    switch (kind)
    {
        case Location::Kind::HostFolder:    return L"This folder is empty.";
        case Location::Kind::DiskDirectory: return L"This directory is empty.";
        case Location::Kind::DiskImage:     return L"This disk image is empty.";
        default:                            return L"This location is empty.";
    }
}
