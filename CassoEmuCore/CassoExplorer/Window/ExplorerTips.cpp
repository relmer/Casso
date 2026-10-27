#include "Pch.h"

#include "CassoExplorer/Window/ExplorerTips.h"
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
//  ExplorerTips::ExplorerTips
//
////////////////////////////////////////////////////////////////////////////////

ExplorerTips::ExplorerTips (CassoExplorerWindow & window)
    : m_window (window)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTips::UpdateLabelTip
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTips::UpdateLabelTip (POINT point)
{
    int   row = -1;
    RECT  box = {};



    if (m_window.m_tree->IsVisible() && CassoExplorerWindow::Contains (m_window.m_tree->GetBounds(), point))
    {
        row = m_window.m_tree->HitTestRow (point.x, point.y);
    }

    if (row < 0 || m_window.m_theme == nullptr || !m_window.m_tree->GetClippedLabelRect (row, box))
    {
        m_window.m_labelTip.Hide();
        return;
    }

    m_window.m_labelTip.Show (box, m_window.m_tree->GetLabelFill (row, *m_window.m_theme), [this, row] (IDxuiPainter & painter, IDxuiTextRenderer & text)
    {
        m_window.m_tree->PaintLabel (row, painter, text, *m_window.m_theme);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTips::ShowHoverTip
//
//  One tooltip serves every part of the window. Each part hides only a tip it
//  showed itself, so a move that one part has no tip for does not take down
//  another's.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTips::ShowHoverTip (TipOwner owner, const RECT & anchor, const std::wstring & text, float maxWidthDip)
{
    m_tipOwner = owner;
    m_window.m_tooltip.SetMaxTextWidthDip (maxWidthDip);
    m_window.m_tooltip.RequestShow (anchor, text, CassoExplorerWindow::GetNowMs());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTips::HideHoverTip
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTips::HideHoverTip (TipOwner owner)
{
    if (m_tipOwner == owner)
    {
        m_window.m_tooltip.RequestHide (CassoExplorerWindow::GetNowMs());
        m_tipOwner = TipOwner::None;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTips::UpdateTreeTip
//
//  The Casso node's tip lists what the folders under it are, since nothing
//  else on screen does. A known folder that is not there gets a tip on why,
//  and so does an image that is not a disk, with the same reason as in the list.
//  That reason is read from the file once per node the pointer enters.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTips::UpdateTreeTip (const DxuiMouseEvent & ev, POINT point)
{
    int                   row  = -1;
    const DxuiTreeNode *  node = nullptr;
    RECT                  tree = m_window.m_tree->GetBounds();
    RECT                  anchor;
    Location              location;



    if (ev.kind == DxuiMouseEventKind::Move && CassoExplorerWindow::Contains (tree, point))
    {
        row  = m_window.m_tree->HitTestRow (point.x, point.y);
        node = m_window.m_tree->GetNodeAt (row);
    }

    bool  adtPro  = node != nullptr && TreeModel::StripLinkCopy (node->id).starts_with (TreeModel::kAdtProId);
    bool  broken  = node != nullptr && node->iconBroken;
    bool  warning = node != nullptr && node->iconWarning;



    if (broken && node->id != m_treeTipId)
    {
        m_treeTipId   = node->id;
        m_treeTipText = m_window.m_browser.TryGetNodeLocation (node->id, location) ? m_window.m_browser.GetImageProblem (location.path) : std::wstring();
    }

    //  A connection the firewall blocks has the cause as its tip.
    if (warning)
    {
        std::wstring   link;
        int            slot  = 0;
        int            drive = 0;
        LinkTreeEntry  entry;

        m_treeTipId   = node->id;
        m_treeTipText = TreeModel::TryParseLinkId (TreeModel::StripLinkCopy (node->id), link, slot, drive) && m_window.m_links.TryGetLinkEntry (link, entry)
                      ? entry.firewall : std::wstring();
    }

    bool  problem = (broken || warning) && !m_treeTipText.empty();

    if (node == nullptr || (node->id != TreeModel::kCassoRootId && !node->dimmed && !adtPro && !problem))
    {
        HideHoverTip (TipOwner::Tree);
        return;
    }

    anchor.left   = tree.left;
    anchor.right  = tree.right;
    anchor.top    = tree.top + (row - m_window.m_tree->GetTopRow()) * m_window.m_tree->GetRowHeight();
    anchor.bottom = anchor.top + m_window.m_tree->GetRowHeight();

    ShowHoverTip (TipOwner::Tree, anchor, adtPro       ? TreeModel::StripLinkCopy (node->id).substr (wcslen (TreeModel::kAdtProId)).c_str()
                                        : problem      ? m_treeTipText.c_str()
                                        : node->dimmed ? kMissingNodeTip : kCassoNodeTip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTips::UpdateListTip
//
//  An item view's name cut short with an ellipsis shows whole in a tip, as
//  Explorer's do.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTips::UpdateListTip (const DxuiMouseEvent & ev, POINT point)
{
    m_listTipPoint  = point;
    m_listTipActive = ev.kind == DxuiMouseEventKind::Move;

    RefreshListTip();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTips::RefreshListTip
//
//  Explorer's tip for the item under the pointer: what the shell says of a
//  real file or folder, read in the background, or the type, size and date of
//  an entry inside an image; with the whole name first when the view cut it
//  short. Over the whole row in Details, the icon alone in Content, and the
//  icon and its name in the other views.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTips::RefreshListTip()
{
    RECT          list     = m_window.m_list->GetBounds();
    POINT         point    = m_listTipPoint;
    RECT          anchor   = {};
    RECT          text     = {};
    int           row      = -1;
    std::wstring  path;
    std::wstring  details;
    std::wstring  tip;
    bool          overItem = false;



    //  None while a menu is open over the list: the pointer is on the menu.
    if (m_listTipActive && m_window.m_list->IsVisible() && CassoExplorerWindow::Contains (list, point) && !m_window.GetPopupHost()->GetContextMenu().IsVisible())
    {
        row = m_window.m_list->HitTestRow (point.x - list.left, point.y - list.top);
    }

    if (row >= 0 && row < (int) m_window.m_browser.GetRows().size() && m_window.m_list->GetCellTextRectPx (row, 0, text))
    {
        OffsetRect (&text, list.left, list.top);

        //  Content shows its tip over the icon only, left of the text.
        overItem = m_window.m_listView != DxuiListView::View::Content || point.x < text.left;
    }

    if (!overItem)
    {
        HideHoverTip (TipOwner::List);
        return;
    }

    {
        const CatalogRow &  row0 = m_window.m_browser.GetRows()[(size_t) row];

        if (m_window.m_browser.GetLocation().kind == Location::Kind::HostFolder && (size_t) row < m_window.m_rowProblems.size() && !m_window.m_rowProblems[(size_t) row].empty())
        {
            //  A broken image's tip says why it is broken.
            details = m_window.m_rowProblems[(size_t) row];
        }
        else if (m_window.m_browser.GetLocation().kind == Location::Kind::HostFolder && m_window.m_browser.TryGetRowPath (row, path))
        {
            //  Not yet read: the tip comes when it is, by a posted message.
            if (!m_window.m_infoTips.TryGet (path, details) && !m_window.m_list->IsItemNameCut (row))
            {
                HideHoverTip (TipOwner::List);
                return;
            }
        }
        else if (!row0.isDirectory || m_window.m_browser.GetLocation().kind == Location::Kind::RecycleBin)
        {
            details = L"Type: " + row0.typeText;

            if (!row0.isDirectory)
            {
                details += L"\nSize: " + BrowserCells::FormatSize (row0.sizeBytes);
            }

            if (row0.hasModified)
            {
                details += L"\nDate modified: " + BrowserCells::FormatModified (row0.modifiedUnix, row0.modifiedIsWallClock);
            }
        }

        tip = m_window.m_list->IsItemNameCut (row) ? row0.name : std::wstring();
        tip += (!tip.empty() && !details.empty()) ? L"\n" + details : details;
    }

    if (tip.empty())
    {
        HideHoverTip (TipOwner::List);
        return;
    }

    //  Details: the whole row; other views: the item's cell.
    anchor = text;

    if (m_window.m_listView == DxuiListView::View::Details)
    {
        anchor.left  = list.left;
        anchor.right = list.right;
    }

    ShowHoverTip (TipOwner::List, anchor, tip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTips::UpdateStatusTip
//
//  The free space of a disk image says which disk it counts.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTips::UpdateStatusTip (const DxuiMouseEvent & ev, POINT point)
{
    const std::wstring &  tip      = m_window.m_browser.GetStatus().freeSpaceTip;
    RECT                  anchor   = m_window.m_status->GetFieldRect (ExplorerLayout::kStatusFree);
    RECT                  activity = m_window.m_status->GetFieldRect (ExplorerLayout::kStatusFill);



    //  The CassoLink line's tip has the latest entries of the log.
    if (ev.kind == DxuiMouseEventKind::Move && CassoExplorerWindow::Contains (activity, point) && !m_window.m_links.GetLatestActivity().empty())
    {
        m_activityTip = m_window.m_links.FormatActivity (kActivityTipLines) + L"\nClick for the whole log.";
        ShowHoverTip (TipOwner::Status, activity, m_activityTip.c_str(), kActivityTipWidthDip);
        return;
    }

    if (ev.kind != DxuiMouseEventKind::Move || tip.empty() || !CassoExplorerWindow::Contains (anchor, point))
    {
        HideHoverTip (TipOwner::Status);
        return;
    }

    ShowHoverTip (TipOwner::Status, anchor, tip.c_str());
}
