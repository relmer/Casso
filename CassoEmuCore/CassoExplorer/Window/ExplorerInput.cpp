#include "Pch.h"

#include "CassoExplorer/Window/ExplorerInput.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "CassoExplorer/Model/FocusRing.h"





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

void ExplorerInput::SetFocusPane (ExplorerPane pane)
{
    if ((pane == ExplorerPane::Preview && !m_window.m_prefs.previewVisible)
        || (pane == ExplorerPane::PreviewToolbar && !m_window.m_previewPane.GetToolbar().IsVisible())
        || ((pane == ExplorerPane::Search || pane == ExplorerPane::GoTo) && m_window.m_previewPane.GetBarMode() != CassoExplorerCommands::kPreviewBarHex))
    {
        pane = ExplorerPane::Tree;
    }

    m_window.m_focus = pane;

    m_window.m_treePane.OnFocusChanged     (pane == ExplorerPane::Tree);
    m_window.m_listPane.OnFocusChanged     (pane == ExplorerPane::List);
    m_window.m_tabStrip.OnFocusChanged     (pane == ExplorerPane::Tabs);
    m_window.m_addressBar.OnFocusChanged   (pane);
    m_window.m_toolbars.GetNavToolbar().SetFocusIndex      (pane == ExplorerPane::Toolbar ? m_toolbarFocus : -1);
    m_window.m_toolbars.GetCommandBar().SetFocusIndex   (pane == ExplorerPane::CommandBar ? m_commandBarFocus : -1);
    m_window.m_previewPane.OnFocusChanged  (pane);

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
        case ExplorerPane::Toolbar: return FocusStop { FocusStop::Kind::ToolbarEntry, m_toolbarFocus };
        case ExplorerPane::CommandBar: return FocusStop { FocusStop::Kind::CommandBarEntry, m_commandBarFocus };
        case ExplorerPane::PreviewToolbar: return FocusStop { FocusStop::Kind::PreviewToolbarEntry, m_window.m_previewPane.GetBarFocus() };
        case ExplorerPane::Search:         return FocusStop { FocusStop::Kind::PreviewToolbarEntry, m_window.m_previewPane.GetPreviewStopIndex (CassoExplorerCommands::kFind) };
        case ExplorerPane::GoTo:           return FocusStop { FocusStop::Kind::PreviewToolbarEntry, m_window.m_previewPane.GetPreviewStopIndex (CassoExplorerCommands::kGoToOffset) };
        case ExplorerPane::Address: return FocusStop { FocusStop::Kind::Address };
        case ExplorerPane::Tabs:    return FocusStop { FocusStop::Kind::Tabs };
        case ExplorerPane::List:    return FocusStop { FocusStop::Kind::List };
        case ExplorerPane::Preview: return FocusStop { FocusStop::Kind::Preview };
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
            SetFocusPane (ExplorerPane::Toolbar);
            break;

        case FocusStop::Kind::CommandBarEntry:
            m_commandBarFocus = stop.entry;
            SetFocusPane (ExplorerPane::CommandBar);
            break;

        case FocusStop::Kind::PreviewToolbarEntry:
            m_window.m_previewPane.SetBarFocus (stop.entry);
            SetFocusPane ((stop.entry == m_window.m_previewPane.GetPreviewStopIndex (CassoExplorerCommands::kFind))       ? ExplorerPane::Search
                        : (stop.entry == m_window.m_previewPane.GetPreviewStopIndex (CassoExplorerCommands::kGoToOffset)) ? ExplorerPane::GoTo
                                                                                             : ExplorerPane::PreviewToolbar);
            break;

        case FocusStop::Kind::Address: m_window.m_addressBar.ShowFocusCue(); SetFocusPane (ExplorerPane::Address); break;
        case FocusStop::Kind::Tabs:    SetFocusPane (ExplorerPane::Tabs);    break;
        case FocusStop::Kind::List:    SetFocusPane (ExplorerPane::List);    break;
        case FocusStop::Kind::Preview: SetFocusPane (ExplorerPane::Preview); break;
        default:                       SetFocusPane (ExplorerPane::Tree);    break;
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
    int  count = m_window.m_toolbars.GetCommandBar().GetEntryCount();
    int  step  = (ev.vk == VK_RIGHT) ? 1 : -1;
    int  at    = m_commandBarFocus;
    int  tries = 0;



    if (m_window.m_toolbars.GetCommandBar().OwnsKeyboard())
    {
        return m_window.m_toolbars.GetCommandBar().HandleKey (ev.vk);
    }

    switch (ev.vk)
    {
        case VK_RETURN:
        case VK_SPACE:
        case VK_DOWN:
            m_window.m_toolbars.GetCommandBar().ActivateFocused();
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



    for (int i = 0; i < m_window.m_toolbars.GetNavToolbar().GetEntryCount(); i++)
    {
        enabled.push_back (m_window.m_dispatch.IsToolbarEntryAvailable (i));
    }

    for (int id : (m_window.m_previewPane.GetBarMode() != CassoExplorerCommands::kPreviewBarNone) ? CassoExplorerCommands::GetPreviewToolbarCommandIds (m_window.m_previewPane.GetBarMode()) : std::vector<int>())
    {
        previewEnabled.push_back (m_window.m_dispatch.IsEnabled (id));
    }

    for (int i = 0; i < m_window.m_toolbars.GetCommandBar().GetEntryCount(); i++)
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
    DxuiToolbar &  toolbar = preview ? m_window.m_previewPane.GetToolbar() : m_window.m_toolbars.GetNavToolbar();



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
    int               at   = (preview ? m_window.m_previewPane.GetBarFocus() : m_toolbarFocus) + step;



    if (preview)
    {
        ids = CassoExplorerCommands::GetPreviewToolbarCommandIds (m_window.m_previewPane.GetBarMode());
    }
    else
    {
        for (int i = 0; i < m_window.m_toolbars.GetNavToolbar().GetEntryCount(); i++)
        {
            ids.push_back (m_window.m_toolbars.GetNavToolbar().GetEntryCommandId (i));
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

    m_window.m_treePane.UpdateLabelTip (ev);
    m_window.m_tabStrip.ClearHoverOnLeave (ev);
    m_window.m_addressBar.CloseSuggestionsOnPress (ev);

    if (m_window.m_previewPane.OnMessageMouse (ev))
    {
        return true;
    }

    m_window.m_toolbars.CloseMenusOutside (ev);

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
        int    changed = (int) m_window.m_previewPane.UpdateScrollbarHover (at)
                       | (int) m_window.m_listPane.UpdateScrollbarHover (at)
                       | (int) m_window.m_treePane.UpdateScrollbarHover (at);

        if (changed != 0)
        {
            m_window.Invalidate();
        }
    }

    if (m_window.m_fileOps.OnRenameMouse (ev))
    {
        return true;
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
        && m_window.m_previewPane.IsOverPane (point))
    {
        m_window.m_dispatch.Dispatch ((ev.wheelDelta > 0.0f) ? CassoExplorerCommands::kZoomIn : CassoExplorerCommands::kZoomOut);
        return true;
    }

    if (m_window.m_toolbars.OnMenuBarMouse (ev))
    {
        return true;
    }

    if (m_window.m_addressBar.OnMouse (ev))
    {
        return true;
    }

    if (m_window.m_previewPane.GetToolbar().IsVisible()
        && (CassoExplorerWindow::Contains (m_window.m_previewPane.GetToolbar().GetBounds(), point) || ev.kind == DxuiMouseEventKind::Up)
        && m_window.m_toolbars.RouteToolbarMouse (m_window.m_previewPane.GetToolbar(), ev))
    {
        m_window.Invalidate();
        return true;
    }

    if (m_window.m_toolbars.OnMouse (ev))
    {
        return true;
    }

    if (m_window.m_treeSplitter->OnMouse (ev) || (m_window.m_previewSplitter->IsVisible() && m_window.m_previewSplitter->OnMouse (ev)))
    {
        m_window.Invalidate();
        return true;
    }

    if (m_window.m_tabStrip.OnMouse (ev))
    {
        return true;
    }

    //  A right-click on a scrollbar opens the bar's own menu, as Windows'
    //  bars do.
    if (press && ev.button == DxuiMouseButton::Right && m_window.m_menus.ShowScrollbarMenu (ev))
    {
        return true;
    }

    if (m_window.m_listPane.OnCapturedMouse (ev))
    {
        return true;
    }

    if (m_window.m_treePane.OnCapturedMouse (ev))
    {
        return true;
    }

    m_window.m_treePane.UpdateTip   (ev, point);
    m_window.m_tips.UpdateStatusTip (ev, point);
    m_window.m_listPane.UpdateTip   (ev, point);

    //  The status bar's CassoLink line opens the whole activity log.
    if (press && ev.button == DxuiMouseButton::Left && CassoExplorerWindow::Contains (m_window.m_status->GetFieldRect (ExplorerLayout::kStatusFill), point) && !m_window.m_links.GetLatestActivity().empty())
    {
        m_window.m_links.ShowActivityLog();
        return true;
    }

    if (m_window.m_treePane.OnMouse (ev))
    {
        return true;
    }

    //  The buttons under the list's message.
    if (m_window.m_links.OnListActionMouse (ev))
    {
        return true;
    }

    if (m_window.m_listPane.OnMouse (ev))
    {
        return true;
    }

    return m_window.m_previewPane.OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR ExplorerInput::GetCursorForPoint (POINT clientPx) const
{
    LPCWSTR  cursor = m_window.m_treeSplitter->GetCursorForPoint (clientPx);



    if (cursor == nullptr)
    {
        cursor = m_window.m_previewPane.GetCursorForPoint (clientPx);
    }

    if (cursor == nullptr)
    {
        cursor = m_window.m_addressBar.GetCursorForPoint (clientPx);
    }

    if (cursor == nullptr && m_window.m_previewSplitter->IsVisible())
    {
        cursor = m_window.m_previewSplitter->GetCursorForPoint (clientPx);
    }

    if (cursor == nullptr)
    {
        cursor = m_window.m_listPane.GetCursorForPoint (clientPx);
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
    int   command = 0;
    bool  handled = false;



    m_window.ArmTick();

    if (m_window.m_tabStrip.OnEscapeKey (ev))
    {
        return true;
    }

    //  A rename in place has every key.
    if (m_window.m_fileOps.IsRenaming())
    {
        m_window.m_fileOps.OnRenameKey (ev);
        return true;
    }

    //  The search box takes keys and characters first while it has focus;
    //  Tab and the keys it has no use for continue as usual.
    if (m_window.m_focus == ExplorerPane::LocationSearch)
    {
        handled = m_window.m_addressBar.OnFindBoxKey (ev);

        if (handled || ev.kind != DxuiKeyEventKind::Down)
        {
            m_window.Invalidate();
            return handled;
        }
    }

    if (m_window.m_focus == ExplorerPane::Search || m_window.m_focus == ExplorerPane::GoTo)
    {
        handled = m_window.m_previewPane.OnBoxKey (m_window.m_focus, ev);

        if (handled || ev.kind != DxuiKeyEventKind::Down)
        {
            m_window.Invalidate();
            return handled;
        }
    }

    //  While the address bar is being edited, keys and characters go to its
    //  text field first; the keys it leaves, Tab and Ctrl+T among them, carry
    //  on as usual.
    if (m_window.m_focus == ExplorerPane::Address && m_window.m_addressBar.IsEditing())
    {
        handled = m_window.m_addressBar.OnKey (ev);

        if (handled || ev.kind != DxuiKeyEventKind::Down)
        {
            m_window.Invalidate();
            return handled;
        }
    }

    //  The preview's shortcuts, for an archive's files and a message.
    if (m_window.m_focus == ExplorerPane::Preview && m_window.m_previewPane.OnShortcutKey (ev))
    {
        return true;
    }

    //  A character typed at the file list jumps to a row. It has to reach the
    //  list before characters are turned away below.
    if (ev.kind == DxuiKeyEventKind::Char && m_window.m_focus == ExplorerPane::List)
    {
        return m_window.m_listPane.OnCharKey (ev);
    }

    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    //  Ctrl+Z on the files undoes the last file operation; in a text box it is
    //  the box's own, which has the keys before this.
    if (ev.ctrl && !ev.alt && !ev.shift && ev.vk == 'Z' && (m_window.m_focus == ExplorerPane::List || m_window.m_focus == ExplorerPane::Tree) && !m_window.m_fileOps.IsRenaming())
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

    if (m_window.m_toolbars.TryMenuBarKey (ev, handled))
    {
        return handled;
    }

    if (m_window.m_focus == ExplorerPane::List && m_window.m_listPane.OnShortcutKey (ev))
    {
        return true;
    }

    //  The same keys on the tree act on its selected node.
    if (m_window.m_focus == ExplorerPane::Tree && m_window.m_treePane.TryShortcutKey (ev, handled))
    {
        return handled;
    }

    //  The preview's stops inside it come before the walk moves on.
    if (ev.vk == VK_TAB && !ev.ctrl && m_window.m_focus == ExplorerPane::Preview && m_window.m_previewPane.OnTabKey (ev))
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
        if (next.kind == FocusStop::Kind::Preview)
        {
            m_window.m_previewPane.OnFocusEntered (!ev.shift);
        }

        return true;
    }

    switch (m_window.m_focus)
    {
        case ExplorerPane::Toolbar: handled = RouteToolbarKey (false, ev); break;
        case ExplorerPane::CommandBar: handled = RouteCommandBarKey (ev);  break;
        case ExplorerPane::PreviewToolbar: handled = RouteToolbarKey (true, ev); break;
        case ExplorerPane::Search:                                               break;
        case ExplorerPane::GoTo:                                                 break;
        case ExplorerPane::Address: handled = m_window.m_addressBar.OnKey (ev); break;
        case ExplorerPane::LocationSearch: handled = m_window.m_addressBar.OnFindBoxKey (ev);  break;
        case ExplorerPane::Tabs:    handled = m_window.m_tabStrip.OnKey (ev); break;
        case ExplorerPane::Tree:    handled = m_window.m_treePane.OnKey (ev); break;
        case ExplorerPane::List:    handled = m_window.m_listPane.OnKey (ev); break;
        case ExplorerPane::Preview: handled = m_window.m_previewPane.OnKey (ev); break;
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
        case ExplorerPane::Tabs:    return m_window.m_tabStrip.GetControl();
        case ExplorerPane::Tree:    return m_window.m_treePane.GetControl();
        case ExplorerPane::List:    return m_window.m_listPane.GetControl();
        case ExplorerPane::Preview: return m_window.m_previewPane.GetFocusedControl();
        default:            return nullptr;
    }
}
