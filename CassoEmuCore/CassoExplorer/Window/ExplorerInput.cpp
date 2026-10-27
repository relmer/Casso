#include "Pch.h"

#include "CassoExplorer/Window/ExplorerInput.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/SearchQuery.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::ExplorerInput
//
////////////////////////////////////////////////////////////////////////////////

ExplorerInput::ExplorerInput (CassoExplorerWindow & window)
    : m_window (window)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::SetFocusPane
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerInput::SetFocusPane (Pane pane)
{
    if ((pane == Pane::Preview && !m_window.m_prefs.previewVisible)
        || (pane == Pane::PreviewToolbar && !m_window.m_previewToolbar->IsVisible())
        || ((pane == Pane::Search || pane == Pane::GoTo) && m_window.m_previewBarMode != CassoExplorerCommands::kPreviewBarHex))
    {
        pane = Pane::Tree;
    }

    m_window.m_focus = pane;

    m_window.m_tree->OnFocusChanged        (pane == Pane::Tree);
    m_window.m_list->OnFocusChanged        (pane == Pane::List);
    m_window.m_previewList->OnFocusChanged (pane == Pane::Preview);
    m_window.m_hexView->OnFocusChanged     (pane == Pane::Preview);
    m_window.m_tabs->OnFocusChanged        (pane == Pane::Tabs);
    m_window.m_address->OnFocusChanged     (pane == Pane::Address);
    m_window.m_findBox->SetFocused         (pane == Pane::LocationSearch);
    m_window.m_toolbar->SetFocusIndex      (pane == Pane::Toolbar ? m_toolbarFocus : -1);
    m_window.m_commandBar->SetFocusIndex   (pane == Pane::CommandBar ? m_commandBarFocus : -1);
    m_window.m_previewToolbar->SetFocusIndex (pane == Pane::PreviewToolbar ? m_window.m_previewBarFocus : -1);
    m_window.m_searchBox.SetFocused          (pane == Pane::Search);
    m_window.m_goToBox.SetFocused            (pane == Pane::GoTo);

    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::GetFocusStop
//
////////////////////////////////////////////////////////////////////////////////

FocusStop ExplorerInput::GetFocusStop() const
{
    switch (m_window.m_focus)
    {
        case Pane::Toolbar: return FocusStop { FocusStop::Kind::ToolbarEntry, m_toolbarFocus };
        case Pane::CommandBar: return FocusStop { FocusStop::Kind::CommandBarEntry, m_commandBarFocus };
        case Pane::PreviewToolbar: return FocusStop { FocusStop::Kind::PreviewToolbarEntry, m_window.m_previewBarFocus };
        case Pane::Search:         return FocusStop { FocusStop::Kind::PreviewToolbarEntry, m_window.m_previewPane.GetPreviewStopIndex (CassoExplorerCommands::kFind) };
        case Pane::GoTo:           return FocusStop { FocusStop::Kind::PreviewToolbarEntry, m_window.m_previewPane.GetPreviewStopIndex (CassoExplorerCommands::kGoToOffset) };
        case Pane::Address: return FocusStop { FocusStop::Kind::Address };
        case Pane::Tabs:    return FocusStop { FocusStop::Kind::Tabs };
        case Pane::List:    return FocusStop { FocusStop::Kind::List };
        case Pane::Preview: return FocusStop { FocusStop::Kind::Preview };
        default:            return FocusStop { FocusStop::Kind::Tree };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::SetFocusStop
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerInput::SetFocusStop (const FocusStop & stop)
{
    switch (stop.kind)
    {
        case FocusStop::Kind::ToolbarEntry:
            m_toolbarFocus = stop.entry;
            SetFocusPane (Pane::Toolbar);
            break;

        case FocusStop::Kind::CommandBarEntry:
            m_commandBarFocus = stop.entry;
            SetFocusPane (Pane::CommandBar);
            break;

        case FocusStop::Kind::PreviewToolbarEntry:
            m_window.m_previewBarFocus = stop.entry;
            SetFocusPane ((stop.entry == m_window.m_previewPane.GetPreviewStopIndex (CassoExplorerCommands::kFind))       ? Pane::Search
                        : (stop.entry == m_window.m_previewPane.GetPreviewStopIndex (CassoExplorerCommands::kGoToOffset)) ? Pane::GoTo
                                                                                             : Pane::PreviewToolbar);
            break;

        case FocusStop::Kind::Address: m_window.m_address->SetFocusCue (true); SetFocusPane (Pane::Address); break;
        case FocusStop::Kind::Tabs:    SetFocusPane (Pane::Tabs);    break;
        case FocusStop::Kind::List:    SetFocusPane (Pane::List);    break;
        case FocusStop::Kind::Preview: SetFocusPane (Pane::Preview); break;
        default:                       SetFocusPane (Pane::Tree);    break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::RouteCommandBarKey
//
//  As on the navigation toolbar: an open drop-down takes every key, Enter,
//  Space and Down press the button, and Left and Right step along the bar to
//  the next button that can be used, wrapping at the ends as a toolbar does.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerInput::RouteCommandBarKey (const DxuiKeyEvent & ev)
{
    int  count = m_window.m_commandBar->GetEntryCount();
    int  step  = (ev.vk == VK_RIGHT) ? 1 : -1;
    int  at    = m_commandBarFocus;
    int  tries = 0;



    if (m_window.m_commandBar->OwnsKeyboard())
    {
        return m_window.m_commandBar->HandleKey (ev.vk);
    }

    switch (ev.vk)
    {
        case VK_RETURN:
        case VK_SPACE:
        case VK_DOWN:
            m_window.m_commandBar->ActivateFocused();
            return true;

        case VK_LEFT:
        case VK_RIGHT:
            for (tries = 0; tries < count; tries++)
            {
                at = (at + step + count) % count;

                if (m_window.m_dispatch.IsCommandBarEntryAvailable (at))
                {
                    SetFocusStop (FocusStop { FocusStop::Kind::CommandBarEntry, at });
                    break;
                }
            }

            return true;

        default:
            return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::BuildFocusStops
//
//  A disabled toolbar button, such as Back with no history, is not a stop,
//  as disabled controls are not tab stops in Windows.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<FocusStop> ExplorerInput::BuildFocusStops() const
{
    std::vector<bool>  enabled;
    std::vector<bool>  previewEnabled;
    std::vector<bool>  commandEnabled;



    for (int i = 0; i < m_window.m_toolbar->GetEntryCount(); i++)
    {
        enabled.push_back (m_window.m_dispatch.IsToolbarEntryAvailable (i));
    }

    for (int id : (m_window.m_previewBarMode != CassoExplorerCommands::kPreviewBarNone) ? CassoExplorerCommands::GetPreviewToolbarCommandIds (m_window.m_previewBarMode) : std::vector<int>())
    {
        previewEnabled.push_back (m_window.m_dispatch.IsEnabled (id));
    }

    for (int i = 0; i < m_window.m_commandBar->GetEntryCount(); i++)
    {
        commandEnabled.push_back (m_window.m_dispatch.IsCommandBarEntryAvailable (i));
    }

    return FocusRing::BuildStops (enabled, m_window.m_prefs.previewVisible, previewEnabled, commandEnabled);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::RouteToolbarKey
//
//  Enter, Space or Down presses the focused button; Left and Right walk the
//  strip's usable buttons and stop at its ends. A picker or a flyout opened
//  from the keyboard owns every key until it closes.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerInput::RouteToolbarKey (bool preview, const DxuiKeyEvent & ev)
{
    DxuiToolbar &  toolbar = preview ? *m_window.m_previewToolbar : *m_window.m_toolbar;



    if (toolbar.OwnsKeyboard())
    {
        return toolbar.HandleKey (ev.vk);
    }

    switch (ev.vk)
    {
        case VK_RETURN:
        case VK_SPACE:
        case VK_DOWN:
            toolbar.ActivateFocused();
            return true;

        case VK_LEFT:
        case VK_RIGHT:
            StepToolbarFocus (preview, ev.vk == VK_RIGHT);
            return true;

        default:
            return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::StepToolbarFocus
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerInput::StepToolbarFocus (bool preview, bool forward)
{
    std::vector<int>  ids;
    int               step = forward ? 1 : -1;
    int               at   = (preview ? m_window.m_previewBarFocus : m_toolbarFocus) + step;



    if (preview)
    {
        ids = CassoExplorerCommands::GetPreviewToolbarCommandIds (m_window.m_previewBarMode);
    }
    else
    {
        for (int i = 0; i < m_window.m_toolbar->GetEntryCount(); i++)
        {
            ids.push_back (m_window.m_toolbar->GetEntryCommandId (i));
        }
    }

    while (at >= 0 && at < (int) ids.size())
    {
        if (preview ? m_window.m_dispatch.IsEnabled (ids[(size_t) at]) : m_window.m_dispatch.IsToolbarEntryAvailable (at))
        {
            SetFocusStop (FocusStop { preview ? FocusStop::Kind::PreviewToolbarEntry : FocusStop::Kind::ToolbarEntry, at });
            return;
        }

        at += step;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::OnMouse
//
//  The menu bar and the splitters take first refusal, since both reach over
//  the panes. A press in a pane moves focus to it; a list that is mid-drag
//  keeps every event until the button comes up.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerInput::OnMouse (const DxuiMouseEvent & ev)
{
    POINT  point = ev.positionDip;
    bool   press = ev.kind == DxuiMouseEventKind::Down;



    m_window.ArmTick();

    //  A tree name cut off by the splitter shows whole as the pointer reaches
    //  it, and goes when the pointer leaves it or does anything else.
    if (ev.kind == DxuiMouseEventKind::Move)
    {
        m_window.m_tips.UpdateLabelTip (point);
    }
    else
    {
        m_window.m_labelTip.Hide();
    }

    //  A pointer leaving the strip by any edge, or the window, takes the
    //  tab's hover with it.
    if (m_window.m_tabs->GetHoverIndex() >= 0 && !m_window.m_tabs->IsInteracting() &&
        ((ev.kind == DxuiMouseEventKind::Move && !CassoExplorerWindow::Contains (m_window.m_tabs->GetBounds(), point)) || ev.kind == DxuiMouseEventKind::Leave))
    {
        DxuiMouseEvent  away = ev;

        away.kind        = DxuiMouseEventKind::Move;
        away.positionDip = { -1, -1 };

        m_window.m_tabs->OnMouse (away);
        m_window.Invalidate();
    }

    //  The address's suggestions hold no capture, so a press anywhere else in
    //  the window is what closes them; the address bar's own presses edit
    //  the path or, on the caret, close them there.
    if (ev.kind == DxuiMouseEventKind::Down && m_window.m_suggest.IsOpen() && !CassoExplorerWindow::Contains (m_window.m_address->GetBounds(), point))
    {
        m_window.m_suggest.Close();
    }

    //  A preview's message selects with the mouse, as text does, and has the
    //  text's own menu.
    if (m_window.m_previewMessage->IsVisible() && (m_window.m_previewMessage->IsInteracting() || CassoExplorerWindow::Contains (m_window.m_previewMessage->GetBounds(), point)))
    {
        if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Right)
        {
            std::vector<DxuiPopupMenuItem>  items;

            SetFocusPane (Pane::Preview);
            m_window.m_menuCommands.clear();
            m_window.m_menus.AddMenuCommand (items, L"&Copy",       [this]() { m_window.m_previewMessage->Copy(); });
            m_window.m_menus.AddMenuCommand (items, L"Select &all", [this]() { m_window.m_previewMessage->SelectAll(); m_window.Invalidate(); });
            DxuiContextMenu::Show (*m_window.GetPopupHost(), point.x, point.y, std::move (items));
            return true;
        }

        if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
        {
            SetFocusPane (Pane::Preview);
        }

        if (m_window.m_previewMessage->OnMouse (ev))
        {
            m_window.Invalidate();
            return true;
        }
    }

    //  A command bar menu leaves the mouse with the window, so a press
    //  anywhere but the bar that opened it closes it here, as a click outside
    //  a menu does.
    if (ev.kind == DxuiMouseEventKind::Down)
    {
        for (DxuiToolbar * bar : { m_window.m_commandBar, m_window.m_toolbar, m_window.m_previewToolbar })
        {
            if (bar->IsMenuOpen() && !CassoExplorerWindow::Contains (bar->GetBounds(), point))
            {
                bar->CloseMenu();
                m_window.Invalidate();
            }
        }
    }

    //  A five-button mouse's back and forward buttons move through the tab's
    //  history wherever the pointer is, as in Explorer.
    if (ev.kind == DxuiMouseEventKind::Up && (ev.button == DxuiMouseButton::X1 || ev.button == DxuiMouseButton::X2))
    {
        m_window.m_dispatch.Dispatch (ev.button == DxuiMouseButton::X1 ? CassoExplorerCommands::kBack : CassoExplorerCommands::kForward);
        return true;
    }

    //  A scrollbar widens while the pointer is over it, as Explorer's do, and
    //  narrows again when the pointer moves off it or out of the window.
    if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Leave)
    {
        POINT  at      = (ev.kind == DxuiMouseEventKind::Leave) ? POINT { -1, -1 } : point;
        int    changed = (int) m_window.m_hexView->SetScrollbarHover (at)
                       | (int) m_window.m_textView->SetScrollbarHover (at)
                       | (int) m_window.m_list->SetScrollbarHover (at)
                       | (int) m_window.m_previewList->SetScrollbarHover (at)
                       | (int) m_window.m_tree->SetScrollbarHover (at)
                       | (int) m_window.m_picture->SetScrollbarHover (at);

        if (changed != 0)
        {
            m_window.Invalidate();
        }
    }

    if (m_window.IsRenaming())
    {
        if (CassoExplorerWindow::Contains (m_window.m_renameBox->GetBounds(), point) || (ev.kind != DxuiMouseEventKind::Down && ev.kind != DxuiMouseEventKind::Wheel))
        {
            m_window.m_renameBox->OnMouse (ev);
            m_window.Invalidate();

            if (CassoExplorerWindow::Contains (m_window.m_renameBox->GetBounds(), point))
            {
                return true;
            }
        }
        else
        {
            m_window.m_fileOps.EndRename (true);
        }
    }

    //  A click on the zoom level puts it back to 100%.
    if (ev.kind == DxuiMouseEventKind::Up && ev.button == DxuiMouseButton::Left
        && CassoExplorerWindow::Contains (m_window.m_status->GetFieldRect (ExplorerLayout::kStatusZoom), point))
    {
        m_window.m_dispatch.Dispatch (CassoExplorerCommands::kZoomReset);
        return true;
    }

    //  Ctrl with the wheel over the preview zooms it, as in a browser.
    if (ev.kind == DxuiMouseEventKind::Wheel && ev.ctrl && !ev.wheelHorizontal
        && m_window.m_prefs.previewVisible && CassoExplorerWindow::Contains (m_window.m_previewRect, point))
    {
        m_window.m_dispatch.Dispatch ((ev.wheelDelta > 0.0f) ? CassoExplorerCommands::kZoomIn : CassoExplorerCommands::kZoomOut);
        return true;
    }

    if (m_window.m_menuBar->OnMouse (ev))
    {
        return true;
    }

    //  A right-click on the address, not while it is being edited, gives
    //  Explorer's address menu.
    if (press && ev.button == DxuiMouseButton::Right && !m_window.m_address->IsEditing() && CassoExplorerWindow::Contains (m_window.m_address->GetBounds(), point))
    {
        m_window.m_addressBar.ShowAddressContextMenu (point.x, point.y);
        return true;
    }

    //  The address bar sits in the toolbar's row, so it answers first. Moves
    //  reach it wherever the pointer is, so its hover clears on the way out.
    if (ev.kind == DxuiMouseEventKind::Move && !m_window.m_address->IsInteracting() && m_window.m_address->OnMouse (ev))
    {
        m_window.Invalidate();
    }

    //  Moves reach the search box wherever the pointer is, so its X lights
    //  under the pointer and goes dark on the way out.
    if (ev.kind == DxuiMouseEventKind::Move && !m_window.m_address->IsInteracting())
    {
        bool  wasLit = m_window.m_findBox->IsClearHovered();

        m_window.m_findBox->OnMouse (ev);

        if (m_window.m_findBox->IsClearHovered() != wasLit)
        {
            m_window.Invalidate();
        }
    }

    if (ev.kind != DxuiMouseEventKind::Move || m_window.m_address->IsInteracting())
    {
        if (CassoExplorerWindow::Contains (m_window.m_findBox->GetBounds(), point))
        {
            if (press)
            {
                SetFocusPane (Pane::LocationSearch);
            }

            m_window.m_findBox->OnMouse (ev);
            m_window.Invalidate();
            return true;
        }

        if (m_window.m_address->IsInteracting() || CassoExplorerWindow::Contains (m_window.m_address->GetBounds(), point))
        {
            if (press)
            {
                SetFocusPane (Pane::Address);
                m_window.m_address->SetFocusCue (false);
            }

            if (m_window.m_address->OnMouse (ev))
            {
                m_window.Invalidate();
                return true;
            }
        }
    }

    if (m_window.m_previewToolbar->IsVisible()
        && (CassoExplorerWindow::Contains (m_window.m_previewToolbar->GetBounds(), point) || ev.kind == DxuiMouseEventKind::Up)
        && RouteToolbarMouse (*m_window.m_previewToolbar, ev))
    {
        m_window.Invalidate();
        return true;
    }

    //  The strip under the pointer takes the event and drives the tooltip; the
    //  other still sees a move, so its hover clears when the pointer crosses.
    {
        DxuiToolbar &  under = GetToolbarUnder (m_window.m_menuBand.GetBounds(), point, *m_window.m_toolbar, *m_window.m_commandBar);
        DxuiToolbar &  other = (&under == m_window.m_commandBar) ? *m_window.m_toolbar : *m_window.m_commandBar;

        if (ev.kind == DxuiMouseEventKind::Move)
        {
            other.OnToolbarMouseMove (ev.positionDip.x, ev.positionDip.y);
        }
        else if (ev.kind == DxuiMouseEventKind::Leave)
        {
            other.OnToolbarMouseLeave();
        }

        if (RouteToolbarMouse (under, ev) || (ev.kind == DxuiMouseEventKind::Up && RouteToolbarMouse (other, ev)))
        {
            m_window.Invalidate();
            return true;
        }
    }

    if (m_window.m_treeSplitter->OnMouse (ev) || (m_window.m_previewSplitter->IsVisible() && m_window.m_previewSplitter->OnMouse (ev)))
    {
        m_window.Invalidate();
        return true;
    }

    //  A move with the button already up means the release went elsewhere,
    //  as when another window took the mouse, so the drag is canceled.
    if (m_window.m_tabs->IsDragging() && ev.kind == DxuiMouseEventKind::Move && ev.button != DxuiMouseButton::Left)
    {
        m_window.m_tabStrip.CancelTabDrag();
        return true;
    }

    //  A tab being dragged keeps the pointer until the button comes up, so the
    //  drag can run past the strip's ends and scroll it.
    if (m_window.m_tabs->IsInteracting())
    {
        m_window.m_tabs->OnMouse (ev);
        m_window.Invalidate();
        return true;
    }

    //  A right-click on a scrollbar opens the bar's own menu, as Windows'
    //  bars do.
    if (press && ev.button == DxuiMouseButton::Right && m_window.m_menus.ShowScrollbarMenu (ev))
    {
        return true;
    }

    //  A right-click on a tab opens its menu, as Explorer's does.
    if (press && ev.button == DxuiMouseButton::Right && CassoExplorerWindow::Contains (m_window.m_tabs->GetBounds(), point)
        && m_window.m_tabs->HitTest (point.x, point.y) >= 0)
    {
        m_window.m_tabStrip.ShowTabContextMenu (point.x, point.y, m_window.m_tabs->HitTest (point.x, point.y));
        return true;
    }

    if (CassoExplorerWindow::Contains (m_window.m_tabs->GetBounds(), point) && m_window.m_tabs->OnMouse (ev))
    {
        m_window.Invalidate();
        return true;
    }

    //  A right press waits for its release to open the menu, or drags the
    //  selection when it moves far enough first.
    if (m_rightPress.active)
    {
        if (ev.kind == DxuiMouseEventKind::Up && ev.button == DxuiMouseButton::Right)
        {
            m_rightPress.active = false;
            m_window.m_menus.ShowListContextMenu (point.x, point.y, m_rightPress.group);
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
    if (m_window.m_dragArmed)
    {
        if (ev.kind == DxuiMouseEventKind::Up || ev.kind == DxuiMouseEventKind::Leave)
        {
            m_window.m_dragArmed = false;
        }
        else if (ev.kind == DxuiMouseEventKind::Move
              && (abs (point.x - m_dragStart.x) > GetSystemMetrics (SM_CXDRAG)
               || abs (point.y - m_dragStart.y) > GetSystemMetrics (SM_CYDRAG)))
        {
            m_window.m_dragArmed = false;
            m_window.m_list->EndDragSelect();
            m_window.m_dragDrop.BeginDragOut();
            m_window.Invalidate();
            return true;
        }
    }

    //  Mouse input goes to a widget mid-drag until the button is released,
    //  wherever the pointer is. Otherwise a scrollbar thumb dragged out of its
    //  pane transfers the drag to the pane under the pointer.
    if (m_window.m_list->IsInteracting())
    {
        m_window.m_list->OnMouse (CassoExplorerWindow::ToLocal (ev, m_window.m_list->GetBounds()));
        m_window.Invalidate();
        return true;
    }

    //  A press on a drive's, a folder's or an image's node drags it once the
    //  pointer has moved far enough, as a list row's press drags its selection.
    if (m_treeDrag.armed)
    {
        if (ev.kind == DxuiMouseEventKind::Up || ev.kind == DxuiMouseEventKind::Leave)
        {
            m_treeDrag.armed = false;
        }
        else if (ev.kind == DxuiMouseEventKind::Move
              && (abs (point.x - m_treeDrag.start.x) > GetSystemMetrics (SM_CXDRAG)
               || abs (point.y - m_treeDrag.start.y) > GetSystemMetrics (SM_CYDRAG)))
        {
            m_treeDrag.armed = false;
            m_window.m_dragDrop.BeginTreeDrag (m_treeDrag.location);
            m_window.Invalidate();
            return true;
        }
    }

    //  The tree takes points in the window's coordinates, as its press did, so
    //  a drag that started there keeps the same origin.
    if (m_window.m_tree->IsInteracting())
    {
        m_window.m_tree->OnMouse (ev);
        m_window.Invalidate();
        return true;
    }

    //  Explorer's tree drops its hover as the pointer leaves it.
    if (m_window.m_tree->GetHoverRow() >= 0 && (ev.kind == DxuiMouseEventKind::Leave || !CassoExplorerWindow::Contains (m_window.m_tree->GetBounds(), point)))
    {
        m_window.m_tree->SetHoverRow (-1);
        m_window.Invalidate();
    }

    m_window.m_tips.UpdateTreeTip   (ev, point);
    m_window.m_tips.UpdateStatusTip (ev, point);
    m_window.m_tips.UpdateListTip   (ev, point);

    //  The status bar's CassoLink line opens the whole activity log.
    if (press && ev.button == DxuiMouseButton::Left && CassoExplorerWindow::Contains (m_window.m_status->GetFieldRect (ExplorerLayout::kStatusFill), point) && !m_window.m_links.GetLatestActivity().empty())
    {
        m_window.m_links.ShowActivityLog();
        return true;
    }

    if (CassoExplorerWindow::Contains (m_window.m_tree->GetBounds(), point))
    {
        //  A right-click neither selects nor takes focus, as in Explorer: the
        //  node under the pointer keeps its hover, and the menu is its own.
        if (press && ev.button != DxuiMouseButton::Right)
        {
            SetFocusPane (Pane::Tree);
        }

        //  Below the last node: the pane's own options, as Explorer's has.
        if (press && ev.button == DxuiMouseButton::Right && m_window.m_tree->HitTestRow (point.x, point.y) < 0)
        {
            m_window.m_menus.ShowTreeEmptyMenu (point.x, point.y);
            return true;
        }

        if (press && ev.button == DxuiMouseButton::Right)
        {
            int                   row  = m_window.m_tree->HitTestRow (point.x, point.y);
            const DxuiTreeNode *  node = m_window.m_tree->GetNodeAt (row);

            if (node != nullptr)
            {
                m_window.m_tree->SetHoverRow (row);
                m_window.Invalidate();
                m_window.m_menus.ShowTreeContextMenu (point.x, point.y, node->id);
            }

            return true;
        }

        if (press && ev.button == DxuiMouseButton::Left)
        {
            int                   row   = m_window.m_tree->HitTestRow (point.x, point.y);
            const DxuiTreeNode  * node  = m_window.m_tree->GetNodeAt (row);
            Location              at;
            bool                  found = node != nullptr && !node->dimmed && m_window.m_browser.TryGetNodeLocation (node->id, at);

            if (found && ((at.kind == Location::Kind::AppleDrive && at.innerPath.empty())
                       || at.kind == Location::Kind::HostFolder || at.kind == Location::Kind::DiskImage))
            {
                m_treeDrag.armed    = true;
                m_treeDrag.start    = point;
                m_treeDrag.location = at;
            }
        }

        m_window.m_tree->OnMouse (ev);
        m_window.Invalidate();
        return true;
    }

    //  The list's buttons under its message. Each is given every move, so its
    //  hover ends as the pointer leaves it, and the presses inside it.
    for (DxuiButton * action : { m_window.m_listAction, m_window.m_listActionAlt })
    {
        bool  inside = action->IsVisible() && CassoExplorerWindow::Contains (action->GetBounds(), point);

        if (action->IsVisible() && (ev.kind == DxuiMouseEventKind::Move || inside))
        {
            action->OnMouse (ev);
            m_window.Invalidate();
        }

        if (inside)
        {
            return true;
        }
    }

    //  An empty image or folder shows a message in the list's place; a right-
    //  click there still opens the menu the list's empty space has.
    if (!m_window.m_list->IsVisible() && m_window.m_listMessage->IsVisible() && press && ev.button == DxuiMouseButton::Right &&
        CassoExplorerWindow::Contains (m_window.m_list->GetBounds(), point))
    {
        SetFocusPane (Pane::List);
        m_window.m_list->ClearSelection();
        m_window.m_menus.ShowListContextMenu (point.x, point.y);
        return true;
    }

    if (m_window.m_list->IsVisible() && CassoExplorerWindow::Contains (m_window.m_list->GetBounds(), point))
    {
        DxuiMouseEvent  local = CassoExplorerWindow::ToLocal (ev, m_window.m_list->GetBounds());

        //  A click gives the list focus without the row a Tab in would pick:
        //  on empty space it selects nothing, and on a row the press picks.
        if (press)
        {
            m_window.m_list->SetSeedRowOnFocus (false);
            SetFocusPane (Pane::List);
            m_window.m_list->SetSeedRowOnFocus (true);
        }

        //  The header has a menu of its own, for its columns.
        if (press && ev.button == DxuiMouseButton::Right && local.positionDip.y < m_window.m_list->GetHeaderHeightPx())
        {
            m_window.m_menus.ShowListHeaderMenu (point.x, point.y, m_window.m_list->HitTestHeaderColumn (local.positionDip.x, local.positionDip.y));
            return true;
        }

        if (press && ev.button == DxuiMouseButton::Right)
        {
            int  row   = m_window.m_list->HitTestRow         (local.positionDip.x, local.positionDip.y);
            int  group = m_window.m_list->HitTestGroupHeader (local.positionDip.x, local.positionDip.y);

            //  A group's header selects its rows and adds the group's own
            //  commands to the menu for them, as Explorer's does.
            if (group >= 0)
            {
                m_window.m_list->SelectGroup (group);
            }
            else if (row >= 0 && !m_window.m_list->IsRowSelected (row))
            {
                m_window.m_list->ClickRow (row, false, false);
            }
            else if (row >= 0)
            {
                //  A row already selected keeps the selection and takes the
                //  focus, so Rename acts on it, as Explorer's does.
                m_window.m_list->SetFocusedRow (row);
            }
            else if (row < 0)
            {
                //  The empty space's menu is the folder's, so nothing stays
                //  selected for it to act on.
                m_window.m_list->ClearSelection();
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
            m_window.m_dragArmed = m_window.m_list->HitTestRow (local.positionDip.x, local.positionDip.y) >= 0;
            m_dragStart          = point;
        }

        m_window.m_list->OnMouse (local);
        m_window.Invalidate();
        return true;
    }

    if (m_window.m_previewArchive && m_window.m_previewPane.RouteArchiveMouse (ev, point))
    {
        m_window.Invalidate();
        return true;
    }

    //  A drag selecting lines keeps the pointer until the button comes up, as
    //  the file list's does.
    if (m_window.m_previewList->IsInteracting())
    {
        m_window.m_previewList->OnMouse (CassoExplorerWindow::ToLocal (ev, m_window.m_previewList->GetBounds()));
        m_window.Invalidate();
        return true;
    }

    if (m_window.m_hexView->IsDragging())
    {
        m_window.m_hexView->OnMouse (ev);
        m_window.Invalidate();
        return true;
    }

    //  A double hi-res picture's own menu switches it between color and its
    //  bits, whichever the file's name or dots chose.
    if (press && ev.button == DxuiMouseButton::Right && m_window.m_picture->IsVisible() && CassoExplorerWindow::Contains (m_window.m_picture->GetBounds(), point)
        && m_window.m_browser.CanTogglePictureColor())
    {
        SetFocusPane (Pane::Preview);
        m_window.m_previewPane.ShowPictureContextMenu (point.x, point.y);
        return true;
    }

    //  A zoomed picture pans by dragging, its scrollbars and the wheel, and
    //  keeps the pointer while a drag is under way.
    if (m_window.m_picture->IsInteracting()
        || (m_window.m_picture->IsVisible() && CassoExplorerWindow::Contains (m_window.m_picture->GetBounds(), point) && m_window.m_picture->OnMouse (ev)))
    {
        if (m_window.m_picture->IsInteracting())
        {
            m_window.m_picture->OnMouse (ev);
        }

        if (press)
        {
            SetFocusPane (Pane::Preview);
        }

        m_window.Invalidate();
        return true;
    }

    if (m_window.m_textView->IsInteracting())
    {
        m_window.m_textView->OnMouse (ev);
        m_window.m_layout.FillStatus();
        m_window.Invalidate();
        return true;
    }

    //  DxuiHexView takes points in the same coordinates as its bounds, so the
    //  event is passed unchanged, not converted to widget-local coordinates as
    //  the older list views require.
    if (m_window.m_hexView->IsVisible() && CassoExplorerWindow::Contains (m_window.m_hexView->GetBounds(), point))
    {
        if (press)
        {
            SetFocusPane (Pane::Preview);
        }

        m_window.m_hexView->OnMouse (ev);
        m_window.Invalidate();
        return true;
    }

    if (m_window.m_textView->IsVisible() && CassoExplorerWindow::Contains (m_window.m_textView->GetBounds(), point))
    {
        if (press)
        {
            SetFocusPane (Pane::Preview);
        }

        m_window.m_textView->OnMouse (ev);
        m_window.m_layout.FillStatus();
        m_window.Invalidate();
        return true;
    }

    if (m_window.m_previewList->IsVisible() && CassoExplorerWindow::Contains (m_window.m_previewList->GetBounds(), point))
    {
        if (press)
        {
            SetFocusPane (Pane::Preview);
        }

        m_window.m_previewList->OnMouse (CassoExplorerWindow::ToLocal (ev, m_window.m_previewList->GetBounds()));
        m_window.Invalidate();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR ExplorerInput::GetCursorForPoint (POINT clientPx) const
{
    LPCWSTR  cursor = m_window.m_treeSplitter->GetCursorForPoint (clientPx);
    RECT     bounds = m_window.m_list->GetBounds();



    //  The preview's text -- a listing, a catalog, a hex dump -- selects as
    //  text does, so it takes the I-beam, except over its scrollbars.
    for (IDxuiControl * view : { (IDxuiControl *) m_window.m_textView, (IDxuiControl *) m_window.m_hexView })
    {
        if (cursor == nullptr && view->IsVisible() && CassoExplorerWindow::Contains (view->GetBounds(), clientPx) && !view->IsOverScrollbar (clientPx))
        {
            cursor = IDC_IBEAM;
        }
    }

    //  The list tests points in its own terms.
    if (cursor == nullptr && m_window.m_previewList->IsVisible() && CassoExplorerWindow::Contains (m_window.m_previewList->GetBounds(), clientPx))
    {
        RECT    list  = m_window.m_previewList->GetBounds();
        POINT   local = { clientPx.x - list.left, clientPx.y - list.top };

        cursor = m_window.m_previewList->GetCursorForPoint (local);
        cursor = (cursor != nullptr || m_window.m_previewList->IsOverScrollbar (local)) ? cursor : (m_window.m_previewArchive ? IDC_ARROW : IDC_IBEAM);
    }

    if (cursor == nullptr && m_window.m_previewMessage->IsVisible())
    {
        cursor = m_window.m_previewMessage->GetCursorForPoint (clientPx);
    }

    //  The address being edited is text, and takes the I-beam over it; its
    //  clear and history buttons keep the arrow.
    if (cursor == nullptr && m_window.m_address->IsEditing() && m_window.m_address->HitTest (clientPx.x, clientPx.y).part == DxuiAddressBar::Part::Blank)
    {
        cursor = IDC_IBEAM;
    }

    if (cursor == nullptr && m_window.m_previewSplitter->IsVisible())
    {
        cursor = m_window.m_previewSplitter->GetCursorForPoint (clientPx);
    }

    if (cursor == nullptr && m_window.m_picture->IsVisible() && CassoExplorerWindow::Contains (m_window.m_picture->GetBounds(), clientPx))
    {
        cursor = m_window.m_picture->GetCursorForPoint (clientPx);
    }

    if (cursor == nullptr && m_window.m_list->IsVisible() && CassoExplorerWindow::Contains (bounds, clientPx))
    {
        cursor = m_window.m_list->GetCursorForPoint (POINT { clientPx.x - bounds.left, clientPx.y - bounds.top });
    }

    return cursor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::OnKey
//
//  Command keys first, then the menu bar's mnemonics and its open dropdown,
//  then Tab between panes, and the rest to the focused pane.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerInput::OnKey (const DxuiKeyEvent & ev)
{
    int                        command = 0;
    bool                       handled = false;
    std::vector<std::wstring>  linkPaths;



    m_window.ArmTick();

    //  Escape cancels a tab drag, as in File Explorer: the card goes, the gap
    //  closes and the tab is back where it was.
    if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_ESCAPE && m_window.m_tabs->IsDragging())
    {
        m_window.m_tabStrip.CancelTabDrag();
        return true;
    }

    //  A rename in place takes every key: Enter keeps the name, Escape puts
    //  the old one back.
    if (m_window.IsRenaming())
    {
        if (ev.kind == DxuiKeyEventKind::Down && (ev.vk == VK_RETURN || ev.vk == VK_ESCAPE))
        {
            m_window.m_fileOps.EndRename (ev.vk == VK_RETURN);
        }
        else
        {
            m_window.m_renameBox->OnKey (ev);
        }

        m_window.Invalidate();
        return true;
    }

    //  The search box takes keys and characters first while it has focus;
    //  Tab and the keys it has no use for continue as usual.
    if (m_window.m_focus == Pane::LocationSearch)
    {
        handled = m_window.m_addressBar.OnFindBoxKey (ev);

        if (handled || ev.kind != DxuiKeyEventKind::Down)
        {
            m_window.Invalidate();
            return handled;
        }
    }

    if (m_window.m_focus == Pane::Search || m_window.m_focus == Pane::GoTo)
    {
        handled = (m_window.m_focus == Pane::Search) ? m_window.m_searchBox.OnKey (ev) : m_window.m_goToBox.OnKey (ev);

        if (handled || ev.kind != DxuiKeyEventKind::Down)
        {
            m_window.Invalidate();
            return handled;
        }
    }

    //  While the address bar is being edited, keys and characters go to its
    //  text field first; the keys it leaves, Tab and Ctrl+T among them, carry
    //  on as usual.
    if (m_window.m_focus == Pane::Address && m_window.m_address->IsEditing())
    {
        handled = m_window.m_address->OnKey (ev);

        if (handled || ev.kind != DxuiKeyEventKind::Down)
        {
            m_window.Invalidate();
            return handled;
        }
    }

    //  An archive's files copy and cut as files do, for the shell to extract
    //  on paste, Ctrl+A selects them all, and Delete deletes them for good,
    //  as in the file list.
    if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && (ev.vk == 'C' || ev.vk == 'X' || ev.vk == 'A')
        && m_window.m_focus == Pane::Preview && m_window.m_previewArchive)
    {
        if (ev.vk == 'A')
        {
            m_window.m_previewList->SelectAllRows();
            m_window.Invalidate();
        }
        else
        {
            m_window.m_previewPane.CopyArchiveSelection (ev.vk == 'X');
        }

        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_DELETE && !ev.ctrl && !ev.alt
        && m_window.m_focus == Pane::Preview && m_window.m_previewArchive)
    {
        m_window.m_previewPane.DeleteArchiveSelection();
        return true;
    }

    //  A preview that is only a message selects and copies as text does.
    if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && (ev.vk == 'C' || ev.vk == 'A')
        && m_window.m_focus == Pane::Preview && m_window.m_previewMessage->IsVisible())
    {
        m_window.m_previewMessage->InvokeCommand ((ev.vk == 'C') ? DxuiStandardCommand::Copy : DxuiStandardCommand::SelectAll);
        m_window.Invalidate();
        return true;
    }

    //  A character typed at the file list jumps to a row. It has to reach the
    //  list before characters are turned away below.
    if (ev.kind == DxuiKeyEventKind::Char && m_window.m_focus == Pane::List)
    {
        handled = m_window.m_list->OnKey (ev);

        if (handled)
        {
            m_window.Invalidate();
        }

        return handled;
    }

    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    //  Ctrl+Z on the files undoes the last file operation; in a text box it is
    //  the box's own, which has the keys before this.
    if (ev.ctrl && !ev.alt && !ev.shift && ev.vk == 'Z' && (m_window.m_focus == Pane::List || m_window.m_focus == Pane::Tree) && !m_window.IsRenaming())
    {
        m_window.m_fileOps.UndoLast();
        return true;
    }

    command = CassoExplorerCommands::TranslateKey (ev.vk, ev.ctrl, ev.alt, ev.shift);

    if (command != 0)
    {
        m_window.m_dispatch.Dispatch (command);
        return true;
    }

    if (m_window.m_menuBar->IsOpen() || m_window.m_menuBar->HasFocus())
    {
        return m_window.m_menuBar->OnKey (ev);
    }

    if (ev.alt && ev.vk >= 0x20 && ev.vk <= 0x7E && m_window.m_menuBar->HandleAltKey ((wchar_t) ev.vk))
    {
        return true;
    }

    if ((ev.vk == VK_APPS || (ev.vk == VK_F10 && ev.shift)) && m_window.m_focus == Pane::List)
    {
        RECT  bounds = m_window.m_list->GetBounds();

        m_window.m_menus.ShowListContextMenu (bounds.left + m_window.m_scaler.ToPx (24), bounds.top + m_window.m_scaler.ToPx (40));
        return true;
    }

    //  File Explorer's keys for Copy as path and Properties.
    if (m_window.m_focus == Pane::List && ev.kind == DxuiKeyEventKind::Down && !m_window.m_browser.GetSelectedRows().empty()
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
    if (m_window.m_focus == Pane::List && ev.kind == DxuiKeyEventKind::Down && ev.alt && !ev.ctrl && ev.vk == VK_RETURN
        && m_window.m_browser.GetSelectedRows().empty()
        && (m_window.m_browser.GetLocation().kind == Location::Kind::HostFolder || m_window.m_browser.IsImageLocation())
        && !SearchQuery::IsId (m_window.m_browser.GetLocation().path))
    {
        m_window.m_fileOps.ShowLocationProperties (m_window.m_browser.GetLocation());
        return true;
    }

    //  The same keys on the tree act on its selected node.
    if (m_window.m_focus == Pane::Tree && ev.kind == DxuiKeyEventKind::Down
        && ((ev.ctrl && ev.shift && !ev.alt && ev.vk == 'C') || (ev.alt && !ev.ctrl && ev.vk == VK_RETURN)))
    {
        const DxuiTreeNode  * node     = m_window.m_tree->GetNodeAt (m_window.m_tree->GetHighlight());
        Location              location;
        bool                  copyable = node != nullptr && m_window.m_browser.TryGetNodeLocation (node->id, location)
                                         && location.kind != Location::Kind::Root && location.kind != Location::Kind::RecycleBin;

        if (copyable && ev.vk == 'C')
        {
            DxuiClipboard::SetText (m_window.GetHwnd(), L"\"" + BrowserModel::FormatAddress (location) + L"\"");
        }
        else if (copyable)
        {
            m_window.m_fileOps.ShowLocationProperties (location);
        }

        return copyable;
    }

    //  Copy in an image writes the files a paste would, as the context menu's
    //  Copy row does; cut and paste are the host's alone.
    if (m_window.m_focus == Pane::List && ev.ctrl && !ev.alt && ev.vk == 'C'
        && m_window.m_browser.IsVolumeLocation() && !m_window.m_browser.GetSelectedRows().empty())
    {
        m_window.m_fileOps.CopyEntriesToClipboard (m_window.m_dispatch.GetNamingStyle());
        return true;
    }

    if (m_window.m_focus == Pane::List && ev.ctrl && !ev.alt && !m_window.m_browser.IsImageLocation()
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

    if (ev.vk == VK_F2 && m_window.m_focus == Pane::List && m_window.m_menus.IsListVerbOffered (CassoExplorerActions::Verb::Rename))
    {
        m_window.m_dispatch.RunVerb (CassoExplorerActions::Verb::Rename);
        return true;
    }

    if (ev.vk == VK_DELETE && m_window.m_focus == Pane::List && !m_window.m_browser.GetSelectedRows().empty())
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

    //  The hex view's two columns are stops of their own inside the preview
    //  pane: Tab moves between them while it has one left, and only then does
    //  the walk move on to the next pane.
    if (ev.vk == VK_TAB && !ev.ctrl && m_window.m_focus == Pane::Preview && m_window.m_previewPane.IsHexPreviewShowing()
        && m_window.m_hexView->OnKey (ev))
    {
        m_window.Invalidate();
        return true;
    }

    //  Tab moves through the enabled toolbar buttons, the address bar, the tab
    //  strip, the tree, the list and the preview, in the order FocusRing
    //  defines.
    if (ev.vk == VK_TAB && !ev.ctrl)
    {
        FocusStop  next = FocusRing::GetNext (BuildFocusStops(), GetFocusStop(), !ev.shift);

        SetFocusStop (next);

        //  A pane with stops inside it starts at the one the walk's direction
        //  calls for.
        if (next.kind == FocusStop::Kind::Preview && m_window.m_previewPane.IsHexPreviewShowing())
        {
            m_window.m_hexView->OnFocusEntered (!ev.shift);
        }

        return true;
    }

    switch (m_window.m_focus)
    {
        case Pane::Toolbar: handled = RouteToolbarKey (false, ev); break;
        case Pane::CommandBar: handled = RouteCommandBarKey (ev);  break;
        case Pane::PreviewToolbar: handled = RouteToolbarKey (true, ev); break;
        case Pane::Search:                                               break;
        case Pane::GoTo:                                                 break;
        case Pane::Address: handled = m_window.m_address->OnKey (ev);     break;
        case Pane::LocationSearch: handled = m_window.m_addressBar.OnFindBoxKey (ev);  break;
        case Pane::Tabs:    handled = m_window.m_tabs->OnKey (ev);        break;
        case Pane::Tree:    handled = m_window.m_tree->OnKey (ev);        break;
        case Pane::List:    handled = m_window.m_list->OnKey (ev);        break;
        case Pane::Preview: handled = m_window.m_previewPane.IsHexPreviewShowing() ? m_window.m_hexView->OnKey (ev)
                                                            : m_window.m_previewPane.IsTextPreviewShowing() ? m_window.m_textView->OnKey (ev)
                                                                                     : m_window.m_previewList->OnKey (ev);
                            m_window.m_layout.FillStatus();
                            break;
    }

    if (ev.vk == VK_BACK && !handled)
    {
        m_window.m_dispatch.Dispatch (CassoExplorerCommands::kUp);
        handled = true;
    }

    m_window.Invalidate();

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::GetFocusedControl
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * ExplorerInput::GetFocusedControl() const
{
    switch (m_window.m_focus)
    {
        case Pane::Tabs:    return m_window.m_tabs;
        case Pane::Tree:    return m_window.m_tree;
        case Pane::List:    return m_window.m_list;
        case Pane::Preview: return m_window.m_previewPane.IsHexPreviewShowing()  ? (IDxuiControl *) m_window.m_hexView
                                 : m_window.m_previewPane.IsTextPreviewShowing() ? (IDxuiControl *) m_window.m_textView
                                                          : (IDxuiControl *) m_window.m_previewList;
        default:            return nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::GetToolbarUnder
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbar & ExplorerInput::GetToolbarUnder (const RECT & commandBarBand, POINT point, DxuiToolbar & navToolbar, DxuiToolbar & commandBar)
{
    return CassoExplorerWindow::Contains (commandBarBand, point) ? commandBar : navToolbar;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::RouteToolbarMouse
//
//  The toolbar takes its pointer input through its own calls rather than
//  OnMouse. A move always reaches it, so its hover clears when the pointer
//  leaves; it is consumed only over the strip. The tooltip follows the hover.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerInput::RouteToolbarMouse (DxuiToolbar & toolbar, const DxuiMouseEvent & ev)
{
    int              x      = ev.positionDip.x;
    int              y      = ev.positionDip.y;
    bool             took   = false;
    RECT             anchor = {};
    const wchar_t *  tip    = nullptr;
    int64_t          nowMs  = CassoExplorerWindow::GetNowMs();



    switch (ev.kind)
    {
        case DxuiMouseEventKind::Move:
            took = toolbar.OnToolbarMouseMove (x, y);
            tip  = took ? toolbar.GetTooltipAt (x, y, anchor) : nullptr;

            //  A button clicked stays quiet until the pointer has left it, as
            //  Explorer's command bar does, rather than tipping again over the
            //  menu it opened.
            if (m_tipMuted && CassoExplorerWindow::Contains (m_tipMuteRect, ev.positionDip))
            {
                tip = nullptr;
            }
            else
            {
                m_tipMuted = false;
            }

            if (tip != nullptr && *tip != L'\0')
            {
                m_window.m_tips.ShowHoverTip (ExplorerTips::TipOwner::Toolbar, anchor, tip);
            }
            else
            {
                m_window.m_tips.HideHoverTip (ExplorerTips::TipOwner::Toolbar);
            }

            break;

        case DxuiMouseEventKind::Down:
            took = ev.button == DxuiMouseButton::Left && toolbar.OnToolbarLButtonDown (x, y);

            //  A right-click on Back or Forward lists where each would go, as
            //  Explorer's does.
            if (ev.button == DxuiMouseButton::Right && toolbar.TryGetEntryRect (CassoExplorerCommands::kBack, anchor) && CassoExplorerWindow::Contains (anchor, ev.positionDip))
            {
                m_window.m_addressBar.ShowHistoryMenu (false, anchor);
                took = true;
            }
            else if (ev.button == DxuiMouseButton::Right && toolbar.TryGetEntryRect (CassoExplorerCommands::kForward, anchor) && CassoExplorerWindow::Contains (anchor, ev.positionDip))
            {
                m_window.m_addressBar.ShowHistoryMenu (true, anchor);
                took = true;
            }

            if (took)
            {
                m_window.m_tooltip.HideImmediate();
                m_tipMuted = toolbar.GetTooltipAt (x, y, m_tipMuteRect) != nullptr;
            }

            break;

        case DxuiMouseEventKind::Up:
            took = ev.button == DxuiMouseButton::Left && toolbar.OnToolbarLButtonUp (x, y);
            break;

        case DxuiMouseEventKind::Leave:
            toolbar.OnToolbarMouseLeave();
            m_window.m_tips.HideHoverTip (ExplorerTips::TipOwner::Toolbar);
            break;

        default:
            break;
    }

    return took;
}
