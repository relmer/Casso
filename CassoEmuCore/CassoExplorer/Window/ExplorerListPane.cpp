#include "Pch.h"

#include "CassoExplorer/Window/ExplorerListPane.h"
#include "CassoExplorer/CassoExplorerNamedControl.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "Core/PathResolver.h"
#include "Core/TextEncoding.h"





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
//  ExplorerListPane::Create
//
//  The list, and the message an empty folder or image has in its place.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::Create()
{
    CassoExplorerNamedControl<DxuiListView>  * list = m_window.CreateChild<CassoExplorerNamedControl<DxuiListView>>();



    list->SetAccessibleName (L"Files");

    m_list        = list;
    m_listMessage = m_window.CreateChild<DxuiLabel> (L"", DxuiTextRole::Muted, DxuiTextHAlign::Center, DxuiTextVAlign::Center);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::Configure
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::Configure()
{
    m_list->SetShowHeader (true);
    m_list->SetIconColumn (0);
    m_list->SetExplorerDetails (true);
    m_list->SetFontDip (CassoExplorerWindow::kProseFontDip);
    m_list->SetRowHeightPxFn (&ExplorerLayout::GetListRowHeightPx);
    m_list->SetColumns (BrowserCells::GetColumns());
    m_columns.ChooseAll (BrowserCells::GetColumns().size());

    //  A header dragged into a new place stays there for the next run.
    m_columns.ApplyColumnOrder();

    m_list->SetOnColumnsReordered ([this] (const std::vector<size_t> & order)
    {
        //  The Recycle Bin's order is its own, as Explorer's is.
        if (m_window.m_browser.GetLocation().kind == Location::Kind::RecycleBin)
        {
            m_window.Invalidate();
            return;
        }

        m_window.m_prefs.columnOrder.clear();

        for (size_t column : order)
        {
            m_window.m_prefs.columnOrder.push_back ((int) column);
        }

        //  The folder keeps its own, and the latest is what others follow.
        m_columns.SetFolderOrder (m_window.m_prefs.columnOrder);
        m_columns.RememberFolderColumns();
        m_window.Invalidate();
    });
    m_list->SetPreciseAutoFit (true);

    //  Columns keep the widths they are given, and a pane too narrow for them
    //  scrolls, the way Explorer's details view does, rather than squeezing
    //  every column to fit.
    m_list->SetHorizontalScrollEnabled (true);
    m_list->SetMultiSelect (true);
    m_list->SetKeyboardColumnNav (true);
    m_list->SetActivateOnDoubleClick (true);
    m_list->SetAlwaysShowSelection (true);

    //  A page slides into view, as Explorer's does. The system tick count moves
    //  in 15.6 ms steps, which left the slide standing still on some frames,
    //  so the list reads the steady clock the window's own ticks read.
    m_list->SetPageSlideEnabled (true);
    m_list->SetClock (&CassoExplorerWindow::GetNowMs);

    m_list->SetOnSelectionChanged ([this] (int)
    {
        m_window.m_browser.SetSelectedRows (m_list->GetSelectedRows());
        m_window.m_previewPane.FillPreview();
        m_window.m_statusBar.Fill();
        m_window.m_toolbars.UpdateCommandBar();
    });

    m_list->SetView (m_columns.GetView());
    m_window.m_toolbars.ShowViewOnButton();

    m_list->SetOnSortColumn ([this] (int column)
    {
        m_window.m_browser.SortByColumn (column);
        m_columns.RememberFolderSort();
        FillList();
    });

    //  A width the user set, by dragging a divider or by double-clicking one
    //  to fit it, is kept for the next run.
    m_list->SetOnColumnResized ([this] (int column, int widthPx)
    {
        if (column < 0)
        {
            return;
        }

        if ((size_t) column >= m_window.m_prefs.columnWidthsDip.size())
        {
            m_window.m_prefs.columnWidthsDip.resize ((size_t) column + 1, 0);
        }

        //  Rounded up, so the width read back is never narrower than the one
        //  the text was fitted to. The latest is the default for folders with
        //  none of their own, and this folder keeps its own, as Explorer's do.
        m_window.m_prefs.columnWidthsDip[(size_t) column] = (widthPx * (int) DxuiDpiScaler::kBaseDpi + (int) m_list->GetDpi() - 1) / (int) m_list->GetDpi();
        m_columns.RememberFolderColumnWidths (column, m_window.m_prefs.columnWidthsDip[(size_t) column]);
    });

    m_list->SetOnActivateRow ([this] (int row)
    {
        //  A deleted item opens its properties, as Explorer's does.
        if (m_window.m_browser.GetLocation().kind == Location::Kind::RecycleBin)
        {
            m_window.m_dispatch.RunRecycleBinVerb (CassoExplorerActions::Verb::Open);
        }
        else if (row >= 0 && (size_t) row < m_window.m_browser.GetRows().size() && m_window.m_browser.GetRows()[(size_t) row].cassoLinkItem == CatalogRow::CassoLinkItem::AddMachine)
        {
            m_window.m_links.EditLink (std::string());
        }
        else if (m_window.m_browser.GetSelectedRows().size() > 1)
        {
            //  Enter on several opens each, as Explorer's does.
            m_window.m_fileOps.OpenEachSelected();
        }
        else if (m_window.m_browser.OpenRow (row))
        {
            FillList();
        }
        else if (m_window.m_browser.IsImageLocation())
        {
            //  A file in an image opens in its own program, from a copy.
            m_window.m_fileOps.OpenSelectedEntries();
        }
        else if (m_window.m_browser.GetLocation().kind == Location::Kind::ShellFolder && row >= 0 && (size_t) row < m_window.m_browser.GetRows().size())
        {
            //  Anything else a shell folder holds does what Explorer's
            //  double-click does with it.
            const CatalogRow &  item = m_window.m_browser.GetRows()[(size_t) row];
            HRESULT             hr   = item.hostPath.empty() ? m_window.m_shellVerbs.OpenShellItem (m_window.GetHwnd(), item.shellId)
                                                             : m_window.m_shellVerbs.Open (m_window.GetHwnd(), item.hostPath);

            IGNORE_RETURN_VALUE (hr, S_OK);
        }
        else
        {
            //  Any other file on the disk opens as Explorer opens it, and a
            //  CassoLink item from the file it is written to.
            std::wstring  path;
            HRESULT       hr = S_OK;

            if (m_window.m_browser.PrepareCassoLinkItems() && m_window.m_browser.TryGetRowPath (row, path))
            {
                hr = m_window.m_shellVerbs.Open (m_window.GetHwnd(), path);
                IGNORE_RETURN_VALUE (hr, S_OK);
            }
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::Layout
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::Layout (const RECT & rect)
{
    m_list->Layout        (rect, m_window.m_scaler);
    m_listMessage->Layout (GetListMessageRect(), m_window.m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::AlignMessage
//
//  The message is at the list's top for a search's results and in its middle
//  otherwise; the firewall's message ends at the middle, with the actions
//  below it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::AlignMessage (bool aboveActions)
{
    m_listMessage->SetTextAlign (DxuiTextHAlign::Center, aboveActions ? DxuiTextVAlign::Bottom : m_listMessageTop ? DxuiTextVAlign::Top : DxuiTextVAlign::Center);
    m_listMessage->Layout       (GetListMessageRect(), m_window.m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::OnCapturedMouse
//
//  A press the list has started, ahead of the panes under the pointer: a
//  right press waits for its release to open the menu, or drags the
//  selection when it moves far enough first; a left press on a row drags
//  the selection once it moves; and the list mid-drag has every event.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListPane::OnCapturedMouse (const DxuiMouseEvent & ev)
{
    POINT  point = ev.positionDip;



    //  A right press waits for its release to open the menu, or drags the
    //  selection when it moves far enough first.
    if (m_rightPress.active)
    {
        if (ev.kind == DxuiMouseEventKind::Up && ev.button == DxuiMouseButton::Right)
        {
            m_rightPress.active = false;
            m_menus.ShowListContextMenu (point.x, point.y, m_rightPress.group);
            return true;
        }

        if (ev.kind == DxuiMouseEventKind::Move && m_rightPress.canDrag
            && (abs (point.x - m_rightPress.start.x) > GetSystemMetrics (SM_CXDRAG)
             || abs (point.y - m_rightPress.start.y) > GetSystemMetrics (SM_CYDRAG)))
        {
            m_rightPress.active = false;
            m_window.m_dragDrop.BeginDragOut();
            m_window.Invalidate();
            return true;
        }
    }

    //  A press on a list row that moves past the drag distance drags the
    //  selection out. This comes ahead of the list's own handling, which would
    //  otherwise take the move as extending the selection. The list never sees
    //  the release, which the drag loop takes, so its drag ends here.
    if (m_dragArmed)
    {
        if (ev.kind == DxuiMouseEventKind::Up || ev.kind == DxuiMouseEventKind::Leave)
        {
            m_dragArmed = false;
        }
        else if (ev.kind == DxuiMouseEventKind::Move
              && (abs (point.x - m_dragStart.x) > GetSystemMetrics (SM_CXDRAG)
               || abs (point.y - m_dragStart.y) > GetSystemMetrics (SM_CYDRAG)))
        {
            m_dragArmed = false;
            m_list->EndDragSelect();
            m_window.m_dragDrop.BeginDragOut();
            m_window.Invalidate();
            return true;
        }
    }

    //  Mouse input goes to a widget mid-drag until the button is released,
    //  wherever the pointer is. Otherwise a scrollbar thumb dragged out of its
    //  pane transfers the drag to the pane under the pointer.
    if (m_list->IsInteracting())
    {
        m_list->OnMouse (CassoExplorerWindow::ToLocal (ev, m_list->GetBounds()));
        m_window.Invalidate();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListPane::OnMouse (const DxuiMouseEvent & ev)
{
    POINT  point = ev.positionDip;
    bool   press = ev.kind == DxuiMouseEventKind::Down;



    //  An empty image or folder shows a message in the list's place; a right-
    //  click there still opens the menu the list's empty space has.
    if (!m_list->IsVisible() && m_listMessage->IsVisible() && press && ev.button == DxuiMouseButton::Right &&
        CassoExplorerWindow::Contains (m_list->GetBounds(), point))
    {
        m_window.m_input.SetFocusPane (ExplorerPane::List);
        m_list->ClearSelection();
        m_menus.ShowListContextMenu (point.x, point.y);
        return true;
    }

    if (m_list->IsVisible() && CassoExplorerWindow::Contains (m_list->GetBounds(), point))
    {
        DxuiMouseEvent  local = CassoExplorerWindow::ToLocal (ev, m_list->GetBounds());

        //  A click gives the list focus without the row a Tab in would pick:
        //  on empty space it selects nothing, and on a row the press picks.
        if (press)
        {
            m_list->SetSeedRowOnFocus (false);
            m_window.m_input.SetFocusPane (ExplorerPane::List);
            m_list->SetSeedRowOnFocus (true);
        }

        //  The header has a menu of its own, for its columns.
        if (press && ev.button == DxuiMouseButton::Right && local.positionDip.y < m_list->GetHeaderHeightPx())
        {
            m_menus.ShowListHeaderMenu (point.x, point.y, m_list->HitTestHeaderColumn (local.positionDip.x, local.positionDip.y));
            return true;
        }

        if (press && ev.button == DxuiMouseButton::Right)
        {
            int  row   = m_list->HitTestRow         (local.positionDip.x, local.positionDip.y);
            int  group = m_list->HitTestGroupHeader (local.positionDip.x, local.positionDip.y);

            //  A group's header selects its rows and adds the group's own
            //  commands to the menu for them, as Explorer's does.
            if (group >= 0)
            {
                m_list->SelectGroup (group);
            }
            else if (row >= 0 && !m_list->IsRowSelected (row))
            {
                m_list->ClickRow (row, false, false);
            }
            else if (row >= 0)
            {
                //  A row already selected keeps the selection and takes the
                //  focus, so Rename acts on it, as Explorer's does.
                m_list->SetFocusedRow (row);
            }
            else if (row < 0)
            {
                //  The empty space's menu is the folder's, so nothing stays
                //  selected for it to act on.
                m_list->ClearSelection();
            }

            //  The menu opens as the button comes up, as Explorer's does; a
            //  press on a row that moves first drags the selection instead.
            m_rightPress = RightPress { true, group < 0 && row >= 0, group, point };
            m_window.Invalidate();
            return true;
        }

        //  A press on a row may start a drag of the selection; one on the
        //  space between rows starts a rubber band instead, as in Explorer.
        if (press && ev.button == DxuiMouseButton::Left)
        {
            m_dragArmed = m_list->HitTestRow (local.positionDip.x, local.positionDip.y) >= 0;
            m_dragStart          = point;
        }

        m_list->OnMouse (local);
        m_window.Invalidate();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::OnCharKey
//
//  A character typed at the list jumps to a row.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListPane::OnCharKey (const DxuiKeyEvent & ev)
{
    bool  handled = m_list->OnKey (ev);



    if (handled)
    {
        m_window.Invalidate();
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::OnShortcutKey
//
//  The list's keys while it has focus: its menu, Explorer's keys for Copy as
//  path and Properties, the clipboard's, Rename and Delete.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListPane::OnShortcutKey (const DxuiKeyEvent & ev)
{
    std::vector<std::wstring>  linkPaths;



    if ((ev.vk == VK_APPS || (ev.vk == VK_F10 && ev.shift)) && m_window.m_focus == ExplorerPane::List)
    {
        RECT  bounds = m_list->GetBounds();

        m_menus.ShowListContextMenu (bounds.left + m_window.m_scaler.ToPx (24), bounds.top + m_window.m_scaler.ToPx (40));
        return true;
    }

    //  File Explorer's keys for Copy as path and Properties.
    if (m_window.m_focus == ExplorerPane::List && ev.kind == DxuiKeyEventKind::Down && !m_window.m_browser.GetSelectedRows().empty()
        && ((ev.ctrl && ev.shift && !ev.alt && ev.vk == 'C') || (ev.alt && !ev.ctrl && ev.vk == VK_RETURN)))
    {
        if (ev.vk == 'C')
        {
            m_window.m_fileOps.CopySelectedPaths();
        }
        else
        {
            m_window.m_fileOps.ShowRowProperties (m_window.m_browser.GetSelectedRows()[0]);
        }

        return true;
    }

    //  With nothing selected, Alt+Enter opens the current folder's Properties,
    //  as the background menu's Properties row does.
    if (m_window.m_focus == ExplorerPane::List && ev.kind == DxuiKeyEventKind::Down && ev.alt && !ev.ctrl && ev.vk == VK_RETURN
        && m_window.m_browser.GetSelectedRows().empty()
        && (m_window.m_browser.GetLocation().kind == Location::Kind::HostFolder || m_window.m_browser.IsImageLocation())
        && !SearchQuery::IsId (m_window.m_browser.GetLocation().path))
    {
        m_window.m_fileOps.ShowLocationProperties (m_window.m_browser.GetLocation());
        return true;
    }

    //  Copy in an image writes the files a paste would, as the context menu's
    //  Copy row does; cut and paste are the host's alone.
    if (m_window.m_focus == ExplorerPane::List && ev.ctrl && !ev.alt && ev.vk == 'C'
        && m_window.m_browser.IsVolumeLocation() && !m_window.m_browser.GetSelectedRows().empty())
    {
        m_window.m_fileOps.CopyEntriesToClipboard (m_window.m_dispatch.GetNamingStyle());
        return true;
    }

    if (m_window.m_focus == ExplorerPane::List && ev.ctrl && !ev.alt && !m_window.m_browser.IsImageLocation()
        && m_window.m_browser.GetLocation().kind == Location::Kind::HostFolder
        && (ev.vk == 'X' || ev.vk == 'C' || ev.vk == 'V'))
    {
        if (ev.vk == 'V')
        {
            m_window.m_dispatch.RunVerb (CassoExplorerActions::Verb::Paste);
        }
        else if (!m_window.m_browser.GetSelectedRows().empty())
        {
            m_window.m_dispatch.RunVerb (ev.vk == 'X' ? CassoExplorerActions::Verb::Cut : CassoExplorerActions::Verb::Copy);
        }

        return true;
    }

    if (ev.vk == VK_F2 && m_window.m_focus == ExplorerPane::List && m_menus.IsListVerbOffered (CassoExplorerActions::Verb::Rename))
    {
        m_window.m_dispatch.RunVerb (CassoExplorerActions::Verb::Rename);
        return true;
    }

    if (ev.vk == VK_DELETE && m_window.m_focus == ExplorerPane::List && !m_window.m_browser.GetSelectedRows().empty())
    {
        //  Shift+Delete deletes host files for good, as in File Explorer;
        //  inside a disk image or an Apple's drive every deletion is for good
        //  and confirmed first, and CassoLink's items are temp copies.
        if (ev.shift && (m_window.m_browser.GetLocation().kind == Location::Kind::HostFolder || m_window.m_browser.GetLocation().kind == Location::Kind::ShellFolder)
            && !m_window.m_browser.TryGetSelectedCassoLinkPaths (linkPaths))
        {
            m_window.m_fileOps.DeleteSelectionPermanently();
        }
        else
        {
            m_window.m_dispatch.RunVerb (CassoExplorerActions::Verb::Delete);
        }

        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::GetScrollbarMenu
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ExplorerListPane::GetScrollbarMenu (const DxuiMouseEvent & ev) const
{
    if (!m_list->IsVisible() || !CassoExplorerWindow::Contains (m_list->GetBounds(), ev.positionDip))
    {
        return {};
    }

    return m_list->GetScrollbarMenu (CassoExplorerWindow::ToLocal (ev, m_list->GetBounds()).positionDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::UpdateTip
//
//  An item view's name cut short with an ellipsis shows whole in a tip, as
//  Explorer's do.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::UpdateTip (const DxuiMouseEvent & ev, POINT point)
{
    m_listTipPoint  = point;
    m_listTipActive = ev.kind == DxuiMouseEventKind::Move;

    RefreshTip();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::RefreshTip
//
//  Explorer's tip for the item under the pointer: what the shell says of a
//  real file or folder, read in the background, or the type, size and date of
//  an entry inside an image; with the whole name first when the view cut it
//  short. Over the whole row in Details, the icon alone in Content, and the
//  icon and its name in the other views.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::RefreshTip()
{
    RECT          list     = m_list->GetBounds();
    POINT         point    = m_listTipPoint;
    RECT          anchor   = {};
    RECT          text     = {};
    int           row      = -1;
    std::wstring  path;
    std::wstring  details;
    std::wstring  tip;
    bool          overItem = false;



    //  None while a menu is open over the list: the pointer is on the menu.
    if (m_listTipActive && m_list->IsVisible() && CassoExplorerWindow::Contains (list, point) && !m_window.GetPopupHost()->GetContextMenu().IsVisible())
    {
        row = m_list->HitTestRow (point.x - list.left, point.y - list.top);
    }

    if (row >= 0 && row < (int) m_window.m_browser.GetRows().size() && m_list->GetCellTextRectPx (row, 0, text))
    {
        OffsetRect (&text, list.left, list.top);

        //  Content shows its tip over the icon only, left of the text.
        overItem = m_columns.GetView() != DxuiListView::View::Content || point.x < text.left;
    }

    if (!overItem)
    {
        m_window.m_tips.HideHoverTip (ExplorerTips::TipOwner::List);
        return;
    }

    {
        const CatalogRow &  row0 = m_window.m_browser.GetRows()[(size_t) row];

        if (m_window.m_browser.GetLocation().kind == Location::Kind::HostFolder && (size_t) row < m_rowProblems.size() && !m_rowProblems[(size_t) row].empty())
        {
            //  A broken image's tip says why it is broken.
            details = m_rowProblems[(size_t) row];
        }
        else if (m_window.m_browser.GetLocation().kind == Location::Kind::HostFolder && m_window.m_browser.TryGetRowPath (row, path))
        {
            //  Not yet read: the tip comes when it is, by a posted message.
            if (!m_window.m_infoTips.TryGet (path, details) && !m_list->IsItemNameCut (row))
            {
                m_window.m_tips.HideHoverTip (ExplorerTips::TipOwner::List);
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

        tip = m_list->IsItemNameCut (row) ? row0.name : std::wstring();
        tip += (!tip.empty() && !details.empty()) ? L"\n" + details : details;
    }

    if (tip.empty())
    {
        m_window.m_tips.HideHoverTip (ExplorerTips::TipOwner::List);
        return;
    }

    //  Details: the whole row; other views: the item's cell.
    anchor = text;

    if (m_columns.GetView() == DxuiListView::View::Details)
    {
        anchor.left  = list.left;
        anchor.right = list.right;
    }

    m_window.m_tips.ShowHoverTip (ExplorerTips::TipOwner::List, anchor, tip);
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
    if (m_window.m_fileOps.GetRenameRow() >= 0)
    {
        m_window.m_fileOps.EndRename (false);
    }

    //  Before the cells are built, since their icons are fetched at the
    //  view's size.
    m_columns.ApplyFolderView();

    m_window.m_toolbars.UpdateCommandBar();

    //  A file's size or date may have changed with the listing.
    m_window.m_infoTips.Clear();
    m_contentDetails.Clear();

    //  The rows' cells are built as the list first asks for each, so a folder
    //  of thousands shows as fast as one of ten.
    m_rowCells.assign    (count, {});
    m_rowBuilt.assign    (count, false);
    m_rowProblems.assign (count, std::wstring());
    m_rowLocation = m_window.m_browser.GetLocation();
    m_rowDark     = DxuiColor::ComputeRelativeLuminance (m_window.m_theme->Background()) < 0.5f;

    m_list->SetRowSource ((int) count, [this] (int row) -> const std::vector<DxuiListView::Cell> & { return GetListRowCells (row); });

    //  Small icons and List measure every name; this reads them without
    //  building the rows' cells.
    m_list->SetRowNameSource ([this] (int row)
    {
        return (row >= 0 && (size_t) row < m_window.m_browser.GetRows().size()) ? m_window.m_browser.GetRows()[(size_t) row].name : std::wstring();
    });
    m_list->SetGroups    (m_window.m_browser.GetListGroups());

    //  A new location opens at its top-left corner, as Explorer's does, rather
    //  than wherever the list was scrolled for the last one. THE COLUMNS STAY AS
    //  THEY ARE: their widths belong to the view rather than to the folder, so
    //  they neither twitch from one folder to the next nor pay to re-measure
    //  every cell of every row on each navigation.
    if (m_window.m_browser.GetLocation() != m_listLocation)
    {
        m_listLocation = m_window.m_browser.GetLocation();
        m_window.m_addressBar.OnLocationChanged (m_listLocation);
        m_window.m_toolbars.SetCommandBarDropDowns();

        m_list->SetTopRow (0);
        m_list->SetLeftPx (0);

        if (m_window.m_treePane.IsNavOptionOn (&CassoExplorerPrefs::navExpandToCurrent))
        {
            m_window.m_treePane.RevealLocationInTree();
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
    m_columns.ApplyColumnOrder();

    //  Explorer's Recycle Bin titles the type column Item type.
    m_list->SetColumnTitle ((size_t) CatalogModel::Column::Type, (m_window.m_browser.GetLocation().kind == Location::Kind::RecycleBin) ? L"Item type" : L"Type");

    m_columns.ShowChosenColumns();

    m_list->UpdateAutoFitFromRows();

    m_list->SetSelectedRows (m_window.m_browser.GetSelectedRows(), m_window.m_browser.GetSelectedRows().empty() ? -1 : m_window.m_browser.GetSelectedRows()[0]);


    if (model.HasTabs())
    {
        m_list->SetSortIndicator ((int) model.GetActiveTab().sortColumn, model.GetActiveTab().sortDescending);
    }

    //  An empty search's message is File Explorer's, at the top of the list;
    //  any other empty location's is centered in the pane.
    m_listMessageTop = false;

    if (message.empty() && m_window.m_browser.GetRows().empty() && m_window.m_browser.GetLocation().kind != Location::Kind::None)
    {
        m_listMessageTop = SearchQuery::IsId (m_window.m_browser.GetLocation().path);
        message                   = m_listMessageTop ? std::wstring (kNoSearchMatches) : GetEmptyLocationMessage (m_window.m_browser.GetLocation().kind);
    }

    m_listMessage->SetTextAlign (DxuiTextHAlign::Center, m_listMessageTop ? DxuiTextVAlign::Top : DxuiTextVAlign::Center);
    m_listMessage->Layout       (GetListMessageRect(), m_window.m_scaler);

    m_listMessage->SetText (message);
    m_listMessage->SetVisible (!message.empty());
    m_list->SetVisible (message.empty());

    m_window.m_links.FillListAction (!message.empty());

    m_window.m_tabStrip.FillTabs();
    m_window.m_addressBar.FillAddress();
    m_window.m_addressBar.FillNavTips();
    m_window.m_previewPane.FillPreview();
    m_window.m_statusBar.Fill();
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
        if (!m_cutPaths.empty() && m_window.m_browser.TryGetRowPath (row, path) &&
            std::any_of (m_cutPaths.begin(), m_cutPaths.end(), [&] (const std::wstring & cut) { return ExplorerFolderSync::IsSameFolder (cut, path); }))
        {
            cells[0].iconGhosted = true;
        }

        //  A disk image that is not one says so on its icon.
        if (rows[(size_t) row].isDiskImage && m_rowLocation.kind == Location::Kind::HostFolder && m_window.m_browser.TryGetRowPath (row, path))
        {
            m_rowProblems[(size_t) row] = m_window.m_browser.GetImageProblem (path);
            cells[0].iconBroken                  = !m_rowProblems[(size_t) row].empty();
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



    if (m_columns.GetView() != DxuiListView::View::Content || row < 0 || (size_t) row >= rows.size())
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
    RECT  rect = m_window.m_layout.GetMessageRect (m_list->GetBounds());



    if (m_listMessageTop)
    {
        rect.top += m_window.m_scaler.ToPx (kSearchMessageTopDip);
    }

    //  The firewall's message runs to several lines, so it ends at the
    //  pane's middle, with the actions below it.
    if (m_window.m_links.IsFirewallMessage())
    {
        rect.bottom = (rect.top + rect.bottom) / 2;
    }

    return rect;
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
    m_listIcons.SetSizePx ((m_columns.GetView() == DxuiListView::View::Details) ? MulDiv (DxuiTreeView::s_kIconDip, (int) dpi, (int) DxuiDpiScaler::kBaseDpi)
                                                                        : DxuiListItemLayout::GetItemIconPx (m_columns.GetView(), dpi));

    m_window.m_dragDrop.SetIconDpi (dpi);
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
            m_list->SetSelectedRows ({ (int) i }, (int) i);
            m_list->EnsureVisible ((int) i);

            //  As a click on the row would: the preview, the status bar's
            //  count and the command bar follow the new selection.
            m_window.m_previewPane.FillPreview();
            m_window.m_statusBar.Fill();
            m_window.m_toolbars.UpdateCommandBar();
            return true;
        }
    }

    return false;
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

            if (m_window.m_treePane.HasNode (nodeId))
            {
                m_window.m_browser.GetTreeModel().Invalidate (nodeId);
                m_window.m_treePane.ReloadChildren (nodeId);
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

        if (m_window.m_treePane.HasNode (nodeId))
        {
            m_window.m_browser.GetTreeModel().Invalidate (nodeId);
            m_window.m_treePane.ReloadChildren (nodeId);
        }
    }

    m_window.Invalidate();
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





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::GetCursorForPoint
//
//  The list tests points relative to its bounds.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR ExplorerListPane::GetCursorForPoint (POINT clientPx) const
{
    RECT  bounds = m_list->GetBounds();



    if (!m_list->IsVisible() || !CassoExplorerWindow::Contains (bounds, clientPx))
    {
        return nullptr;
    }

    return m_list->GetCursorForPoint (POINT { clientPx.x - bounds.left, clientPx.y - bounds.top });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::Tick
//
//  Returns whether a scrollbar, the header or a group is moving.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListPane::Tick (int64_t nowMs)
{
    int  moved = (int) m_list->TickScrollbars (nowMs)
               | (int) m_list->IsHeaderSliding (nowMs)
               | (int) m_list->IsGroupSliding();



    return moved != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::HitTestRow
//
//  The row under a point in the window's client area; the list tests points
//  relative to its bounds.
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerListPane::HitTestRow (POINT client) const
{
    RECT  bounds = m_list->GetBounds();



    return m_list->HitTestRow (client.x - bounds.left, client.y - bounds.top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::ReleasePress
//
//  The list's press ends before a drag of the selection; the drag loop has
//  the release, and the list would otherwise extend the selection on the
//  next move.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::ReleasePress()
{
    DxuiMouseEvent  release;



    release.kind   = DxuiMouseEventKind::Up;
    release.button = DxuiMouseButton::Left;
    m_list->OnMouse (release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::TryGetRenameRect
//
//  The row's name cell, in the window's client area, scrolled into view
//  first; in Details, inside the row, as the selection box is, not over the
//  whole of it.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListPane::TryGetRenameRect (int row, RECT & outRect)
{
    RECT  bounds = m_list->GetBounds();



    m_list->EnsureVisible (row);

    if (!m_list->GetCellTextRectPx (row, 0, outRect))
    {
        return false;
    }

    OffsetRect (&outRect, bounds.left, bounds.top);

    if (m_columns.GetView() == DxuiListView::View::Details)
    {
        InflateRect (&outRect, 0, -m_window.m_scaler.ToPx (DxuiListView::s_kRowBoxInsetYDip));
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::CaptureView
//
//  Where the list is looking and what it has selected, ahead of a re-read.
//  The focused item counts only as part of the selection it belongs to.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::CaptureView (RefreshAnchor::Before & outBefore) const
{
    outBefore.topRow   = m_list->GetTopRow();
    outBefore.capacity = m_list->GetVisibleRowCapacity();
    outBefore.selected = m_list->GetSelectedRows();
    outBefore.focused  = m_list->IsRowSelected (m_list->GetSelectedRow()) ? m_list->GetSelectedRow() : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::RestoreView
//
//  The top row last, since restoring the selection scrolls it into view. By
//  whole rows, so an unmoved top leaves the view as it was, and rows a page
//  down or the list's end lifted stay lifted.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::RestoreView (const RefreshAnchor::After & after)
{
    m_list->SetSelectedRows (after.selected, after.focused);
    m_list->MoveTopRow      (after.topRow);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::StartLoaders
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::StartLoaders()
{
    m_listIcons.LoadInBackground (m_window.GetHwnd(), (UINT) ExplorerMessage::IconsLoaded);
    m_contentDetails.Start (m_window.GetHwnd(), (UINT) ExplorerMessage::ContentDetails);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::OnIconsLoaded
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::OnIconsLoaded()
{
    if (m_listIcons.TakeLoaded())
    {
        RefreshListIcons();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListPane::OnContentDetailsArrived
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListPane::OnContentDetailsArrived()
{
    if (m_contentDetails.TakeArrived())
    {
        RefreshContentDetails();
    }
}