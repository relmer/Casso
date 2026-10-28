#include "Pch.h"

#include "CassoExplorer/Window/Panes/ExplorerTreePane.h"
#include "CassoExplorer/CassoExplorerNamedControl.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"
#include "CassoExplorer/Window/ExplorerMetrics.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "CassoExplorer/Window/Managers/ExplorerDragDrop.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Window/Managers/ExplorerInput.h"
#include "CassoExplorer/Window/Managers/ExplorerFileOps.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/Window/Managers/CassoLinkManager.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "CassoExplorer/Window/Managers/ExplorerTips.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::ExplorerTreePane
//
////////////////////////////////////////////////////////////////////////////////

ExplorerTreePane::ExplorerTreePane (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::Create
//
//  The tree, as a child of the window, and its settings, handlers and
//  roots once the window has its HWND.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTreePane::Create()
{
    CassoExplorerNamedControl<DxuiTreeView>  * tree = m_context.window.CreateChild<CassoExplorerNamedControl<DxuiTreeView>>();



    tree->SetAccessibleName (L"Folders and disk images");
    m_tree = tree;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::Configure
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTreePane::Configure()
{
    std::vector<DxuiTreeNode>  roots;
    bool                       opened = false;



    m_labelTip.SetPopupHost (m_context.window.GetPopupHost());
    m_context.browser.GetTreeRoots (roots);

    m_tree->SetShowCheckboxes (false);
    ExplorerTextStyles::Apply (*m_tree, ExplorerTextStyles::kProse);
    m_tree->SetIndentDip (ExplorerMetrics::kTreeIndentDip);
    m_tree->SetIconLeadDip (ExplorerMetrics::kTreeIconLeadDip);
    m_tree->SetIconGapDip  (ExplorerMetrics::kTreeIconGapDip);
    m_tree->SetLeftPadDip  (ExplorerMetrics::kTreeLeftPadDip);
    m_tree->SetHorizontalScrollEnabled (false);
    m_tree->SetNodes (std::move (roots));
    m_tree->SetChildProvider ([this] (const std::wstring & id) { return m_context.browser.GetTreeChildren (id); });

    //  A known folder whose drive is away has nothing to show, so selecting
    //  it opens nothing; it stays in the tree to be removed.
    m_tree->SetOnSelect ([this] (const std::wstring & id)
    {
        HRESULT               hr   = S_OK;
        const DxuiTreeNode *  node = m_tree->GetNodeAt (m_tree->FindRowById (id));



        if (node != nullptr && node->dimmed)
        {
            return;
        }

        //  Add a CassoLink connection opens its dialog, and the tree goes back
        //  to where the list is.
        if (TreeModel::StripLinkCopy (id) == TreeModel::kAddLinkId)
        {
            m_context.cassoLinkManager.EditConnection (std::string());
            RevealLocationInTree();
            m_context.window.Invalidate();
            return;
        }

        hr = m_context.browser.SelectTreeNode (id);
        IGNORE_RETURN_VALUE (hr, S_OK);
        m_context.listPane.FillList();
    });

    //  Opening or closing a folder changes which folders are on screen.
    m_tree->SetOnExpand ([this] (const std::wstring &, bool)
    {
        m_context.folderSync.UpdateWatchedFolders();
    });

    //  The Casso places and This PC open on their first level; the shell's
    //  other roots stay closed, as Explorer's do. Network in particular takes
    //  as long as the network takes to answer.
    for (const std::wstring & id : { std::wstring (TreeModel::kCassoRootId), std::wstring (TreeModel::kThisPcRootId) })
    {
        opened = m_tree->SetRowExpanded (m_tree->FindRowById (id), true);
        IGNORE_RETURN_VALUE (opened, true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::Layout
//
//  The tree has no height until its first layout, so the node revealed at
//  startup is scrolled into view here: to the middle, since the layouts that
//  follow as the window settles can make the tree shorter.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTreePane::Layout (const RECT & rect)
{
    m_tree->Layout (rect, m_context.scaler);

    if (m_revealPending && m_tree->GetRowCap() > 0)
    {
        m_tree->SetTopRow (m_tree->GetHighlight() - m_tree->GetRowCap() / 2);
        m_revealPending = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::UpdateLabelTip
//
//  A tree name cut off by the splitter is drawn whole while the pointer is
//  over it, and hidden on any other event.
//
//  Pointer events. The label tip follows every move; a drag armed on a
//  node or one under way, and the hover's end, are ahead of the list's
//  hit test; a press over the tree is after it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTreePane::UpdateLabelTip (const DxuiMouseEvent & ev)
{
    if (ev.kind != DxuiMouseEventKind::Move)
    {
        m_labelTip.Hide();
        return;
    }

    ShowLabelTipAt (ev.positionDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::HandleCapturedMouse
//
//  A press on a drive's, a folder's or an image's node drags it once the
//  pointer has moved far enough, as a list row's press drags the list's
//  selection. The tree's events are in the window's coordinates, as its press
//  was, so a drag that started there has the same origin. The hover is
//  cleared as the pointer moves off the tree, as in Explorer.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerTreePane::HandleCapturedMouse (const DxuiMouseEvent & ev)
{
    POINT  point = ev.positionDip;



    if (m_drag.armed)
    {
        if (ev.kind == DxuiMouseEventKind::Up || ev.kind == DxuiMouseEventKind::Leave)
        {
            m_drag.armed = false;
        }
        else if (ev.kind == DxuiMouseEventKind::Move
              && (abs (point.x - m_drag.start.x) > GetSystemMetrics (SM_CXDRAG)
               || abs (point.y - m_drag.start.y) > GetSystemMetrics (SM_CYDRAG)))
        {
            m_drag.armed = false;
            m_context.dragDrop.BeginTreeDrag (m_drag.location);
            m_context.window.Invalidate();
            return true;
        }
    }

    if (m_tree->IsInteracting())
    {
        m_tree->OnMouse (ev);
        m_context.window.Invalidate();
        return true;
    }

    if (m_tree->GetHoverRow() >= 0 && (ev.kind == DxuiMouseEventKind::Leave || !ExplorerLayout::Contains (m_tree->GetBounds(), point)))
    {
        m_tree->SetHoverRow (-1);
        m_context.window.Invalidate();
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::HandleMouse
//
//  A right-click neither selects nor moves focus, as in Explorer: the hover
//  stays on the node under the pointer, and the menu is the node's. Below
//  the last node, the menu has the pane's options, as Explorer's has.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerTreePane::HandleMouse (const DxuiMouseEvent & ev)
{
    POINT                 point = ev.positionDip;
    bool                  press = ev.kind == DxuiMouseEventKind::Down;
    int                   row   = -1;
    const DxuiTreeNode  * node  = nullptr;
    Location              at;



    if (!ExplorerLayout::Contains (m_tree->GetBounds(), point))
    {
        return false;
    }

    if (press)
    {
        row  = m_tree->HitTestRow (point.x, point.y);
        node = m_tree->GetNodeAt (row);
    }

    if (press && ev.button != DxuiMouseButton::Right)
    {
        m_context.input.SetFocusPane (ExplorerPane::Tree);
    }

    if (press && ev.button == DxuiMouseButton::Right && row < 0)
    {
        ShowTreeEmptyMenu (point.x, point.y);
        return true;
    }

    if (press && ev.button == DxuiMouseButton::Right)
    {
        if (node != nullptr)
        {
            m_tree->SetHoverRow (row);
            m_context.window.Invalidate();
            ShowTreeContextMenu (point.x, point.y, node->id);
        }

        return true;
    }

    if (press && ev.button == DxuiMouseButton::Left && node != nullptr && !node->dimmed && m_context.browser.TryGetNodeLocation (node->id, at)
        && ((at.kind == Location::Kind::AppleDrive && at.innerPath.empty()) || at.kind == Location::Kind::HostFolder || at.kind == Location::Kind::DiskImage))
    {
        m_drag.armed    = true;
        m_drag.start    = point;
        m_drag.location = at;
    }

    m_tree->OnMouse (ev);
    m_context.window.Invalidate();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::TryShortcutKey
//
//  File Explorer's keys for Copy as path and Properties, on the selected
//  node. Returns whether the keys are those two; outHandled is whether the
//  node had a path to act on.
//
//  Keys, and focus.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerTreePane::TryShortcutKey (const DxuiKeyEvent & ev, bool & outHandled)
{
    const DxuiTreeNode  * node     = nullptr;
    Location              location;
    bool                  copyable = false;



    if (ev.kind != DxuiKeyEventKind::Down || !((ev.ctrl && ev.shift && !ev.alt && ev.vk == 'C') || (ev.alt && !ev.ctrl && ev.vk == VK_RETURN)))
    {
        return false;
    }

    node     = m_tree->GetNodeAt (m_tree->GetHighlight());
    copyable = node != nullptr && m_context.browser.TryGetNodeLocation (node->id, location)
               && location.kind != Location::Kind::Root && location.kind != Location::Kind::RecycleBin;

    if (copyable && ev.vk == 'C')
    {
        DxuiClipboard::SetText (m_context.window.GetHwnd(), L"\"" + BrowserModel::FormatAddress (location) + L"\"");
    }
    else if (copyable)
    {
        m_context.fileOps.ShowLocationProperties (location);
    }

    outHandled = copyable;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::GetScrollbarMenu
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ExplorerTreePane::GetScrollbarMenu (POINT point) const
{
    if (!m_tree->IsVisible() || !ExplorerLayout::Contains (m_tree->GetBounds(), point))
    {
        return {};
    }

    return m_tree->GetScrollbarMenu (point);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::ReloadChildren
//
//  The children of a node on display read again; a node not in the tree is
//  read when it is next opened.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTreePane::ReloadChildren (const std::wstring & id)
{
    if (m_tree->FindRowById (id) < 0)
    {
        return;
    }

    m_tree->ReplaceChildren (id, m_context.browser.GetTreeChildren (id));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::ExpandRowForDrop
//
//  A collapsed node a drag has rested on opens, as in File Explorer's
//  navigation pane, so the drop can go into one of its folders.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTreePane::ExpandRowForDrop (int row)
{
    const DxuiTreeNode  * node = m_tree->GetNodeAt (row);



    if (node != nullptr && !node->expanded && (!node->childrenLoaded || !node->children.empty()) && m_tree->SetRowExpanded (row, true))
    {
        m_context.window.Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::ClearDropTarget
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTreePane::ClearDropTarget()
{
    m_tree->SetDropRow  (-1);
    m_tree->SetHoverRow (-1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::ReleasePress
//
//  The tree's press is ended before a drag of its node; otherwise the tree
//  would handle the first move after the drag as part of the press.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTreePane::ReleasePress()
{
    DxuiMouseEvent  release;



    release.kind   = DxuiMouseEventKind::Up;
    release.button = DxuiMouseButton::Left;
    m_tree->OnMouse (release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::TryGetRenameRect
//
//  Over the node's label, to the tree's edge, as File Explorer renames a
//  folder in its navigation pane. The node is scrolled into view first.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerTreePane::TryGetRenameRect (const std::wstring & id, RECT & outRect)
{
    int   row  = m_tree->FindRowById (id);
    RECT  tree = m_tree->GetBounds();



    if (row < 0)
    {
        return false;
    }

    m_tree->EnsureRowVisible (row);

    if (!m_tree->GetLabelTextRect (row, outRect))
    {
        return false;
    }

    outRect.right = (std::max) (outRect.right, (LONG) tree.right - m_context.scaler.ToPx (kRenameEdgeDip));

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::RevealLocationInTree
//
//  Expands This PC, or with the all-folders option on the desktop, down to the
//  current location's folder, highlights it and expands it, as File Explorer's
//  navigation pane does when a window opens on a folder.
//  Each path component is matched against the child labels without regard to
//  case, since a restored path need not match the case on disk.
//
//  The location's node revealed: opened to, highlighted and scrolled into
//  view. RefreshTree re-reads the tree, keeping what was open,
//  highlighted and on screen; ReloadChildren re-reads one node's.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTreePane::RevealLocationInTree()
{
    Location              location = m_context.browser.GetLocation();
    std::wstring          path     = location.path;
    std::wstring          current  = m_tree->GetHighlightedId();
    int                   casso    = m_tree->FindRowById (TreeModel::kCassoRootId);
    int                   desktop  = m_tree->FindRowById (TreeModel::kDesktopRootId);
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
            m_tree->SetRowExpanded (m_tree->FindRowById (TreeModel::kCassoLinkRootId), true);

            if (drive != 0)
            {
                m_tree->SetRowExpanded (m_tree->FindRowById (TreeModel::MakeLinkId (link)), true);
            }
        }

        row = m_tree->FindRowById (location.path);

        if (row >= 0 && TreeModel::StripLinkCopy (current) != location.path)
        {
            m_tree->HighlightRow (row);
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
        m_tree->SetRowExpanded (m_tree->GetHighlight(), true);
        ScrollTreeToRow (m_tree->GetHighlight());
        return;
    }

    //  A click on an ADTPro server's folder node, under its connection,
    //  highlights that node, so no other is searched for.
    if (TreeModel::StripLinkCopy (current) == TreeModel::kAdtProId + path)
    {
        ScrollTreeToRow (m_tree->GetHighlight());
        return;
    }

    //  Casso's known folders come first: the deepest one that holds the
    //  location is where the walk starts. The Casso root is closed again when
    //  none does and it was closed before.
    if (casso >= 0)
    {
        root    = m_tree->GetNodeAt (casso);
        wasOpen = (root != nullptr) && root->expanded;

        m_tree->SetRowExpanded (casso, true);
        root = m_tree->GetNodeAt (m_tree->FindRowById (TreeModel::kCassoRootId));

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
            m_tree->SetRowExpanded (m_tree->FindRowById (TreeModel::kCassoRootId), false);
        }
    }

    //  With the all-folders option on, This PC is under the desktop, beside the
    //  desktop's own folders, as the user's and OneDrive's; the deepest of
    //  those that contains the location is where the walk starts.
    if (knownId.empty() && desktop >= 0)
    {
        m_tree->SetRowExpanded (desktop, true);
        root = m_tree->GetNodeAt (m_tree->FindRowById (TreeModel::kDesktopRootId));

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

    row = knownId.empty() ? WalkTreeLabels (m_tree->FindRowById (TreeModel::kThisPcRootId), path)
                          : WalkTreeLabels (m_tree->FindRowById (knownId), path.substr (known));

    if (row < 0)
    {
        return;
    }

    //  The open folder's own row is opened too, as Explorer's is.
    m_tree->HighlightRow (row);
    m_tree->SetRowExpanded (row, true);
    ScrollTreeToRow (row);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::ScrollTreeToRow
//
//  Into view now, or at the first layout when the tree has no height yet.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTreePane::ScrollTreeToRow (int row)
{

    if (m_tree->GetRowCap() > 0)
    {
        m_tree->EnsureRowVisible (row);
    }
    else
    {
        m_revealPending = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::WalkTreeLabels
//
//  Expands down from a row one path component at a time, matching each against
//  the child labels without regard to case, and returns the deepest row
//  reached. A drive's label is its volume name, so a drive is matched by the
//  letter its id ends with instead.
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerTreePane::WalkTreeLabels (int row, const std::wstring & path)
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

        m_tree->SetRowExpanded (row, true);
        node = m_tree->GetNodeAt (row);

        for (size_t i = 0; node != nullptr && i < node->children.size() && next < 0; i++)
        {
            const std::wstring &  childLabel = node->children[i].label;
            bool                  isDrive    = label.size() == 2 && label[1] == L':';

            if (isDrive ? (_wcsnicmp (node->children[i].id.c_str() + wcslen (TreeModel::kThisPcRootId), label.c_str(), 2) == 0)
                        : (_wcsicmp (childLabel.c_str(), label.c_str()) == 0))
            {
                next = m_tree->FindRowById (node->children[i].id);
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
//  ExplorerTreePane::RefreshTree
//
//  Reads the whole tree again in place, as Explorer's F5 does: the roots, then
//  each open node in row order, which puts every parent ahead of its children.
//  What is open, highlighted and on screen stays, by the tree's own merge.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTreePane::RefreshTree()
{
    std::vector<std::wstring>  open;
    std::vector<DxuiTreeNode>  roots;
    int                        row = 0;



    for (row = 0; row < m_tree->GetVisibleCount(); row++)
    {
        const DxuiTreeNode *  node = m_tree->GetNodeAt (row);

        if (node != nullptr && node->expanded)
        {
            open.push_back (node->id);
        }
    }

    m_context.browser.GetTreeModel().InvalidateAll();
    m_context.browser.GetTreeRoots (roots);
    m_tree->ReplaceRoots (std::move (roots));

    for (const std::wstring & id : open)
    {
        if (m_tree->FindRowById (id) >= 0)
        {
            m_tree->ReplaceChildren (id, m_context.browser.GetTreeChildren (id));
        }
    }

    m_context.folderSync.UpdateWatchedFolders();
    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::ShowTreeContextMenu
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTreePane::ShowTreeContextMenu (int x, int y, const std::wstring & id)
{
    std::vector<DxuiPopupMenuItem>  items;
    std::wstring                    folder;
    Location                        location;
    bool                            hasFolder   = m_context.browser.TryGetNodePath (id, folder);
    bool                            hasLocation = m_context.browser.TryGetNodeLocation (id, location);
    const DxuiTreeNode            * node        = m_tree->GetNodeAt (m_tree->FindRowById (id));



    m_context.menus.GetMenuCommands().clear();

    //  A known folder that is not there offers only its removal; every other
    //  command acts on a folder that cannot be opened.
    if (node != nullptr && node->dimmed)
    {
        hasLocation = false;
    }

    //  A folder on the disk has Explorer's row of Cut, Copy, Paste, Rename and
    //  Delete.
    if (hasLocation && ((location.kind == Location::Kind::HostFolder && location.path.size() > 3) || location.kind == Location::Kind::DiskImage))
    {
        AddTreeIconRow (items, location.path, id);
    }

    //  Explorer's first command expands or collapses the node, in a group
    //  of its own.
    if (node != nullptr && (!node->childrenLoaded || !node->children.empty()))
    {
        m_context.menus.AddMenuCommand (items, node->expanded ? L"&Collapse" : L"&Expand", [this, id, expand = !node->expanded]()
        {
            int   row     = m_tree->FindRowById (id);
            bool  changed = (row >= 0) && m_tree->SetRowExpanded (row, expand);

            if (changed)
            {
                m_context.window.Invalidate();
            }
        });
        m_context.menus.GetMenuCommands().back()->menuSvg = m_context.menus.GetMenuSvg (node->expanded ? L"windows.collapse" : L"windows.expand");
    }

    //  The open rows follow Collapse in its group, with no Open, as in File
    //  Explorer's navigation pane.
    if (hasLocation)
    {
        m_context.menus.AddOpenRows (items, location, false);
    }

    //  A disk image's Casso commands, as its row in the list has them.
    if (hasLocation && location.kind == Location::Kind::DiskImage)
    {
        std::wstring  image = location.path;

        items.push_back (DxuiPopupMenuItem::ForSeparator());
        m_context.menus.AddMenuCommand (items, ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::InsertDrive1),   [this, image]() { m_context.fileOps.InsertIntoDrive (image, 1); });
        m_context.menus.AddMenuCommand (items, ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::InsertDrive2),   [this, image]() { m_context.fileOps.InsertIntoDrive (image, 2); });
        m_context.menus.AddMenuCommand (items, ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::OpenInNewCasso), [this, image]() { m_context.fileOps.OpenInNewCasso (image); });
        m_context.menus.AddMenuCommand (items, ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::ExtractFiles),   [this, image]() { m_context.commandManager.ExtractImageFiles (image); });
        m_context.menus.GetMenuCommands().back()->menuSvg = m_context.menus.GetMenuSvg (L"windows.compressedfile.extract");
    }

    if (hasFolder && m_context.browser.CanRemoveFromCasso (id))
    {
        m_context.menus.AddMenuCommand (items, L"&Remove from Casso", [this, folder]() { m_context.folderSync.ChangeKnownFolder (folder, false); });
    }
    else if (hasFolder && m_context.browser.CanAddToCasso (id))
    {
        m_context.menus.AddMenuCommand (items, L"&Add to Casso", [this, folder]() { m_context.folderSync.ChangeKnownFolder (folder, true); });
    }

    m_context.cassoLinkManager.AddConnectionRows (items, hasLocation, location);

    //  The Recycle Bin's node empties it, as Explorer's does.
    if (hasLocation && location.kind == Location::Kind::RecycleBin)
    {
        m_context.menus.AddMenuCommand (items, ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::EmptyRecycleBin), [this]()
        {
            HRESULT  hr = m_context.shellVerbs.EmptyRecycleBin (m_context.window.GetHwnd());

            IGNORE_RETURN_VALUE (hr, S_OK);
            m_context.folderSync.RefreshAfterHostChange();
        });
        m_context.menus.GetMenuCommands().back()->menuSvg = m_context.menus.GetMenuSvg (ExplorerMenus::GetVerbMenuSvgName (CassoExplorerActions::Verb::EmptyRecycleBin));
    }
    else if (hasLocation && location.kind != Location::Kind::Root && location.kind != Location::Kind::AppleDrive)
    {
        //  One group, as in File Explorer: Windows' commands, Copy as path,
        //  New and Properties follow the open and pin rows.
        if (location.kind == Location::Kind::HostFolder && location.path.size() > 3)
        {
            m_context.menus.AddWindowsCommands (items, { location.path }, Win32PackagedCommands::ItemKind::Folder);
        }

        m_context.menus.AddMenuCommand (items, L"Copy as &path", [this, location]()
        {
            DxuiClipboard::SetText (m_context.window.GetHwnd(), L"\"" + BrowserModel::FormatAddress (location) + L"\"");
        }, L"Ctrl+Shift+C", s_kpszMdl2CopyPath);
        m_context.menus.GetMenuCommands().back()->menuSvg = m_context.menus.GetMenuSvg (L"windows.copyaspath");

        if (location.kind == Location::Kind::HostFolder)
        {
            std::shared_ptr<DxuiCommand>    parent    = std::make_shared<DxuiCommand>();
            std::shared_ptr<DxuiCommand>    newFolder = std::make_shared<DxuiCommand>();
            std::shared_ptr<DxuiCommand>    newDisk   = std::make_shared<DxuiCommand>();
            std::vector<DxuiPopupMenuItem>  choices;

            newFolder->label    = ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::NewFolder);
            newFolder->menuSvg  = m_context.menus.GetMenuSvg (L"newfolder");
            newFolder->dispatch = [this, location]() { m_context.addressBar.GoToLocation (location); m_context.commandManager.RunVerb (CassoExplorerActions::Verb::NewFolder); };
            newDisk->label      = ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::NewDisk);
            newDisk->menuSvg    = m_context.menus.GetMenuSvg (L"casso.floppy525");
            newDisk->dispatch   = [this, location]() { m_context.addressBar.GoToLocation (location); m_context.commandManager.RunVerb (CassoExplorerActions::Verb::NewDisk); };

            m_context.menus.AddNewChoices (choices, location, newFolder, newDisk);

            parent->label     = L"Ne&w";
            parent->menuGlyph = s_kpszMdl2Add;
            parent->menuSvg   = m_context.menus.GetMenuSvg (L"windows.newitem");
            items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (choices)));

            m_context.menus.GetMenuCommands().push_back (std::move (parent));
            m_context.menus.GetMenuCommands().push_back (std::move (newFolder));
            m_context.menus.GetMenuCommands().push_back (std::move (newDisk));
        }

        m_context.menus.AddMenuCommand (items, L"P&roperties", [this, location]() { m_context.fileOps.ShowLocationProperties (location); }, L"Alt+Enter");
        m_context.menus.GetMenuCommands().back()->menuGlyph = s_kpszMdl2Repair;
        m_context.menus.GetMenuCommands().back()->menuSvg   = m_context.menus.GetMenuSvg (L"windows.properties");
    }

    //  A folder on the disk has the commands installed apps add, then the
    //  shell's own menu, as in Explorer's.
    if (hasLocation && location.kind == Location::Kind::HostFolder)
    {
        POINT  screen = { x, y };

        ClientToScreen (m_context.window.GetHwnd(), &screen);

        m_context.menus.AddPackagedCommandsFor (items, { location.path }, (location.path.size() <= 3) ? Win32PackagedCommands::ItemKind::Drive : Win32PackagedCommands::ItemKind::Folder);

        items.push_back (DxuiPopupMenuItem::ForSeparator());
        m_context.menus.AddMenuCommand (items, ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::MoreOptions), [this, location, screen]()
        {
            HRESULT  hr = m_context.shellVerbs.ShowShellMenu (m_context.window.GetHwnd(), { location.path }, screen, {});

            IGNORE_RETURN_VALUE (hr, S_OK);
        });
        m_context.menus.GetMenuCommands().back()->menuGlyph = s_kpszMdl2OpenInNewWindow;
        m_context.menus.GetMenuCommands().back()->menuSvg   = m_context.menus.GetMenuSvg (ExplorerMenus::GetVerbMenuSvgName (CassoExplorerActions::Verb::MoreOptions));
    }

    if (!items.empty())
    {
        DxuiContextMenu::Show (*m_context.window.GetPopupHost(), x, y, std::move (items));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::AddTreeIconRow
//
//  Add a folder's Cut, Copy, Paste, Rename and Delete as buttons atop the
//  menu, as File Explorer does for a node. Cut and Copy place the folder on
//  the clipboard as a file, Paste goes into it while the clipboard has
//  files, Rename edits the node's label, and Delete moves the folder to the
//  Recycle Bin.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTreePane::AddTreeIconRow (std::vector<DxuiPopupMenuItem> & items, const std::wstring & folder, const std::wstring & id)
{
    static constexpr CassoExplorerActions::Verb  kIconVerbs[] =
    {
        CassoExplorerActions::Verb::Cut,
        CassoExplorerActions::Verb::Copy,
        CassoExplorerActions::Verb::Paste,
        CassoExplorerActions::Verb::Rename,
        CassoExplorerActions::Verb::Delete,
    };
    std::vector<std::shared_ptr<const DxuiCommand>>  iconCommands;



    for (CassoExplorerActions::Verb verb : kIconVerbs)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();

        //  Paste is added only with files to paste, as on a folder's row.
        if (verb == CassoExplorerActions::Verb::Paste && !m_context.shellVerbs.ClipboardHasFiles())
        {
            continue;
        }

        command->id       = (int) verb;
        command->label    = ExplorerMenus::GetVerbLabel (verb);
        command->glyph    = ExplorerMenus::GetVerbGlyph (verb);
        command->menuSvg  = m_context.menus.GetMenuSvg (ExplorerMenus::GetVerbMenuSvgName (verb));
        command->dispatch = [this, verb, folder, id]()
        {
            HRESULT  hr = S_OK;

            if (verb == CassoExplorerActions::Verb::Paste)
            {
                m_context.fileOps.PasteHere (folder);
                return;
            }

            if (verb == CassoExplorerActions::Verb::Rename)
            {
                m_context.fileOps.BeginTreeRename (id);
                return;
            }

            if (verb == CassoExplorerActions::Verb::Delete)
            {
                hr = m_context.shellVerbs.Recycle (m_context.window.GetHwnd(), { folder });
                m_context.folderSync.RefreshAfterHostChange();
            }
            else
            {
                hr = m_context.shellVerbs.PlaceOnClipboard (m_context.window.GetHwnd(), { folder }, verb == CassoExplorerActions::Verb::Cut);
            }

            IGNORE_RETURN_VALUE (hr, S_OK);
        };
        iconCommands.push_back (command);
        m_context.menus.GetMenuCommands().push_back (std::move (command));
    }

    items.push_back (DxuiPopupMenuItem::ForIconRow (std::move (iconCommands)));
    items.push_back (DxuiPopupMenuItem::ForSeparator());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::ShowTreeEmptyMenu
//
//  The pane's own options, as Explorer's menu below its last node has them,
//  each kept across runs.
//
//  The menu below the tree's last node, and the pane options it sets.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTreePane::ShowTreeEmptyMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>  items;



    m_context.menus.GetMenuCommands().clear();

    AddNavPaneToggle (items, L"Show This &PC",          &CassoExplorerPrefs::navShowThisPc);
    AddNavPaneToggle (items, L"Show &Network",          &CassoExplorerPrefs::navShowNetwork);
    AddNavPaneToggle (items, L"Show &libraries",        &CassoExplorerPrefs::navShowLibraries);
    AddNavPaneToggle (items, L"Show &all folders",      &CassoExplorerPrefs::navShowAllFolders);
    AddNavPaneToggle (items, L"&Expand to open folder", &CassoExplorerPrefs::navExpandToCurrent);

    if (m_context.app.links != nullptr)
    {
        items.push_back (DxuiPopupMenuItem::ForSeparator());
        m_context.menus.AddMenuCommand (items, L"Add a &CassoLink machine...", [this]() { m_context.cassoLinkManager.EditConnection (std::string()); });
    }

    DxuiContextMenu::Show (*m_context.window.GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::AddNavPaneToggle
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTreePane::AddNavPaneToggle (std::vector<DxuiPopupMenuItem> & items, const wchar_t * label, CassoExplorerPrefs::NavOption option)
{
    //  The first change parts this option from File Explorer's for good.
    m_context.menus.AddMenuCommand (items, label, [this, option]()
    {
        m_context.prefs.*option = !IsNavOptionOn (option);
        ApplyNavPaneOptions();
        RefreshTree();

        if (IsNavOptionOn (&CassoExplorerPrefs::navExpandToCurrent))
        {
            RevealLocationInTree();
        }
    });
    m_context.menus.GetMenuCommands().back()->isChecked = [this, option]() { return IsNavOptionOn (option); };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::IsNavOptionOn
//
//  Casso Explorer's own value where the user set one; otherwise File
//  Explorer's, read from the shell for the roots and from its settings for
//  the other two.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerTreePane::IsNavOptionOn (CassoExplorerPrefs::NavOption option) const
{
    const TreeModel &  tree = m_context.browser.GetTreeModel();



    if (option == &CassoExplorerPrefs::navShowThisPc)
    {
        return tree.IsNavRootShown (TreeModel::NavRoot::ThisPc);
    }

    if (option == &CassoExplorerPrefs::navShowNetwork)
    {
        return tree.IsNavRootShown (TreeModel::NavRoot::Network);
    }

    if (option == &CassoExplorerPrefs::navShowLibraries)
    {
        return tree.IsNavRootShown (TreeModel::NavRoot::Libraries);
    }

    if (option == &CassoExplorerPrefs::navShowAllFolders)
    {
        return m_context.prefs.navShowAllFolders.value_or (m_context.explorerOptions.paneShowsAllFolders);
    }

    return m_context.prefs.navExpandToCurrent.value_or (m_context.explorerOptions.paneExpandsToCurrent);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::ApplyNavPaneOptions
//
//  The pane options, from the preferences.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTreePane::ApplyNavPaneOptions()
{
    TreeModel::NavPaneOptions  options;



    options.showThisPc     = m_context.prefs.navShowThisPc;
    options.showNetwork    = m_context.prefs.navShowNetwork;
    options.showLibraries  = m_context.prefs.navShowLibraries;
    options.showAllFolders = IsNavOptionOn (&CassoExplorerPrefs::navShowAllFolders);

    m_context.browser.GetTreeModel().SetNavPaneOptions (options);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::ShowLabelTipAt
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTreePane::ShowLabelTipAt (POINT point)
{
    int   row = -1;
    RECT  box = {};



    if (m_tree->IsVisible() && ExplorerLayout::Contains (m_tree->GetBounds(), point))
    {
        row = m_tree->HitTestRow (point.x, point.y);
    }

    if (row < 0 || m_context.theme == nullptr || !m_tree->GetClippedLabelRect (row, box))
    {
        m_labelTip.Hide();
        return;
    }

    m_labelTip.Show (box, m_tree->GetLabelFill (row, *m_context.theme), [this, row] (IDxuiPainter & painter, IDxuiTextRenderer & text)
    {
        m_tree->PaintLabel (row, painter, text, *m_context.theme);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePane::UpdateTip
//
//  The Casso node's tip lists what the folders under it are, since nothing
//  else on screen does. A known folder that is not there gets a tip on why,
//  and so does an image that is not a disk, with the same reason as in the list.
//  That reason is read from the file once per node the pointer enters.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTreePane::UpdateTip (const DxuiMouseEvent & ev, POINT point)
{
    int                   row  = -1;
    const DxuiTreeNode *  node = nullptr;
    RECT                  tree = m_tree->GetBounds();
    RECT                  anchor;
    Location              location;



    if (ev.kind == DxuiMouseEventKind::Move && ExplorerLayout::Contains (tree, point))
    {
        row  = m_tree->HitTestRow (point.x, point.y);
        node = m_tree->GetNodeAt (row);
    }

    bool  adtPro  = node != nullptr && TreeModel::StripLinkCopy (node->id).starts_with (TreeModel::kAdtProId);
    bool  broken  = node != nullptr && node->iconBroken;
    bool  warning = node != nullptr && node->iconWarning;



    if (broken && node->id != m_treeTipId)
    {
        m_treeTipId   = node->id;
        m_treeTipText = m_context.browser.TryGetNodeLocation (node->id, location) ? m_context.browser.GetImageProblem (location.path) : std::wstring();
    }

    //  A connection the firewall blocks has the cause as its tip.
    if (warning)
    {
        std::wstring   link;
        int            slot  = 0;
        int            drive = 0;
        LinkTreeEntry  entry;

        m_treeTipId   = node->id;
        m_treeTipText = TreeModel::TryParseLinkId (TreeModel::StripLinkCopy (node->id), link, slot, drive) && m_context.cassoLinkManager.TryGetConnectionEntry (link, entry)
                      ? entry.firewall : std::wstring();
    }

    bool  problem = (broken || warning) && !m_treeTipText.empty();

    if (node == nullptr || (node->id != TreeModel::kCassoRootId && !node->dimmed && !adtPro && !problem))
    {
        m_context.tips.HideHoverTip (ExplorerTips::TipOwner::Tree);
        return;
    }

    anchor.left   = tree.left;
    anchor.right  = tree.right;
    anchor.top    = tree.top + (row - m_tree->GetTopRow()) * m_tree->GetRowHeight();
    anchor.bottom = anchor.top + m_tree->GetRowHeight();

    m_context.tips.ShowHoverTip (ExplorerTips::TipOwner::Tree, anchor, adtPro       ? TreeModel::StripLinkCopy (node->id).substr (wcslen (TreeModel::kAdtProId)).c_str()
                                        : problem      ? m_treeTipText.c_str()
                                        : node->dimmed ? kMissingNodeTip : kCassoNodeTip);
}
