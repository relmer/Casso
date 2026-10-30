#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerInput.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "CassoExplorer/Model/FocusRing.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "CassoExplorer/CassoExplorerCommands.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/Window/Panes/ExplorerToolbars.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Panes/ExplorerTreePane.h"
#include "CassoExplorer/Window/Managers/ExplorerTabManager.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/Managers/ExplorerFileOps.h"
#include "CassoExplorer/Window/Panes/ExplorerStatusBar.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/Window/Managers/CassoLinkManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::ExplorerInput
//
////////////////////////////////////////////////////////////////////////////////

ExplorerInput::ExplorerInput (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::SetFocusPane
//
//  Keyboard focus. The toolbar is one pane, with its focused button in
//  m_toolbarFocus.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerInput::SetFocusPane (ExplorerPane pane)
{
    if ((pane == ExplorerPane::Preview && !m_context.prefs.previewVisible)
        || (pane == ExplorerPane::PreviewToolbar && !m_context.preview.GetToolbar().IsVisible())
        || ((pane == ExplorerPane::Search || pane == ExplorerPane::GoTo) && m_context.preview.GetBarMode() != CassoExplorerCommands::kPreviewBarHex))
    {
        pane = ExplorerPane::Tree;
    }

    m_focus = pane;

    m_context.treePane.HandleFocusChanged     (pane == ExplorerPane::Tree);
    m_context.listPane.HandleFocusChanged     (pane == ExplorerPane::List);
    m_context.tabManager.HandleFocusChanged     (pane == ExplorerPane::Tabs);
    m_context.addressBar.HandleFocusChanged   (pane);
    m_context.toolbars.GetNavToolbar().SetFocusIndex      (pane == ExplorerPane::Toolbar ? m_toolbarFocus : -1);
    m_context.toolbars.GetCommandBar().SetFocusIndex   (pane == ExplorerPane::CommandBar ? m_commandBarFocus : -1);
    m_context.preview.HandleFocusChanged  (pane);

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::GetFocusStop
//
////////////////////////////////////////////////////////////////////////////////

FocusStop ExplorerInput::GetFocusStop() const
{
    switch (m_focus)
    {
        case ExplorerPane::Toolbar: return FocusStop { FocusStop::Kind::ToolbarEntry, m_toolbarFocus };
        case ExplorerPane::CommandBar: return FocusStop { FocusStop::Kind::CommandBarEntry, m_commandBarFocus };
        case ExplorerPane::PreviewToolbar: return FocusStop { FocusStop::Kind::PreviewToolbarEntry, m_context.preview.GetBarFocus() };
        case ExplorerPane::Search:         return FocusStop { FocusStop::Kind::PreviewToolbarEntry, m_context.preview.GetPreviewStopIndex (CassoExplorerCommands::kFind) };
        case ExplorerPane::GoTo:           return FocusStop { FocusStop::Kind::PreviewToolbarEntry, m_context.preview.GetPreviewStopIndex (CassoExplorerCommands::kGoToOffset) };
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
            m_context.preview.SetBarFocus (stop.entry);
            SetFocusPane ((stop.entry == m_context.preview.GetPreviewStopIndex (CassoExplorerCommands::kFind))       ? ExplorerPane::Search
                        : (stop.entry == m_context.preview.GetPreviewStopIndex (CassoExplorerCommands::kGoToOffset)) ? ExplorerPane::GoTo
                                                                                             : ExplorerPane::PreviewToolbar);
            break;

        case FocusStop::Kind::Address: m_context.addressBar.ShowFocusCue(); SetFocusPane (ExplorerPane::Address); break;
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
    int  count = m_context.toolbars.GetCommandBar().GetEntryCount();
    int  step  = (ev.vk == VK_RIGHT) ? 1 : -1;
    int  at    = m_commandBarFocus;
    int  tries = 0;



    if (m_context.toolbars.GetCommandBar().OwnsKeyboard())
    {
        return m_context.toolbars.GetCommandBar().HandleKey (ev.vk);
    }

    switch (ev.vk)
    {
        case VK_RETURN:
        case VK_SPACE:
        case VK_DOWN:
            m_context.toolbars.GetCommandBar().ActivateFocused();
            return true;

        case VK_LEFT:
        case VK_RIGHT:
            for (tries = 0; tries < count; tries++)
            {
                at = (at + step + count) % count;

                if (m_context.commandManager.IsCommandBarEntryAvailable (at))
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



    for (int i = 0; i < m_context.toolbars.GetNavToolbar().GetEntryCount(); i++)
    {
        enabled.push_back (m_context.commandManager.IsToolbarEntryAvailable (i));
    }

    for (int id : (m_context.preview.GetBarMode() != CassoExplorerCommands::kPreviewBarNone) ? CassoExplorerCommands::GetPreviewToolbarCommandIds (m_context.preview.GetBarMode()) : std::vector<int>())
    {
        previewEnabled.push_back (m_context.commandManager.IsEnabled (id));
    }

    for (int i = 0; i < m_context.toolbars.GetCommandBar().GetEntryCount(); i++)
    {
        commandEnabled.push_back (m_context.commandManager.IsCommandBarEntryAvailable (i));
    }

    return FocusRing::BuildStops (enabled, m_context.prefs.previewVisible, previewEnabled, commandEnabled);
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
    DxuiToolbar &  toolbar = preview ? m_context.preview.GetToolbar() : m_context.toolbars.GetNavToolbar();



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
    int               at   = (preview ? m_context.preview.GetBarFocus() : m_toolbarFocus) + step;



    if (preview)
    {
        ids = CassoExplorerCommands::GetPreviewToolbarCommandIds (m_context.preview.GetBarMode());
    }
    else
    {
        for (int i = 0; i < m_context.toolbars.GetNavToolbar().GetEntryCount(); i++)
        {
            ids.push_back (m_context.toolbars.GetNavToolbar().GetEntryCommandId (i));
        }
    }

    while (at >= 0 && at < (int) ids.size())
    {
        if (preview ? m_context.commandManager.IsEnabled (ids[(size_t) at]) : m_context.commandManager.IsToolbarEntryAvailable (at))
        {
            SetFocusStop (FocusStop { preview ? FocusStop::Kind::PreviewToolbarEntry : FocusStop::Kind::ToolbarEntry, at });
            return;
        }

        at += step;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::HandleMouse
//
//  The menu bar and the splitters take first refusal, since both reach over
//  the panes. A press in a pane moves focus to it; a list that is mid-drag
//  keeps every event until the button comes up.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerInput::HandleMouse (const DxuiMouseEvent & ev)
{
    POINT  point = ev.positionDip;
    bool   press = ev.kind == DxuiMouseEventKind::Down;



    m_context.window.ArmTick();

    m_context.treePane.UpdateLabelTip (ev);
    m_context.tabManager.ClearHoverOnLeave (ev);
    m_context.listPane.ClearHoverOutside (ev);
    m_context.addressBar.CloseSuggestionsOnPress (ev);

    if (m_context.preview.HandleMessageMouse (ev))
    {
        return true;
    }

    m_context.toolbars.CloseMenusOutside (ev);

    //  A five-button mouse's back and forward buttons move through the tab's
    //  history wherever the pointer is, as in Explorer.
    if (ev.kind == DxuiMouseEventKind::Up && (ev.button == DxuiMouseButton::X1 || ev.button == DxuiMouseButton::X2))
    {
        m_context.commandManager.Dispatch (ev.button == DxuiMouseButton::X1 ? CassoExplorerCommands::kBack : CassoExplorerCommands::kForward);
        return true;
    }

    //  A scrollbar widens while the pointer is over it, as Explorer's do, and
    //  narrows again when the pointer moves off it or out of the window.
    if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Leave)
    {
        POINT  at      = (ev.kind == DxuiMouseEventKind::Leave) ? POINT { -1, -1 } : point;
        int    changed = (int) m_context.preview.UpdateScrollbarHover (at)
                       | (int) m_context.listPane.UpdateScrollbarHover (at)
                       | (int) m_context.treePane.UpdateScrollbarHover (at);

        if (changed != 0)
        {
            m_context.window.Invalidate();
        }
    }

    if (m_context.fileOps.HandleRenameMouse (ev))
    {
        return true;
    }

    if (m_context.statusBar.HandleZoomClick (ev))
    {
        return true;
    }

    //  Ctrl with the wheel over the preview zooms it, as in a browser.
    if (ev.kind == DxuiMouseEventKind::Wheel && ev.ctrl && !ev.wheelHorizontal
        && m_context.preview.IsOverPane (point))
    {
        m_context.commandManager.Dispatch ((ev.wheelDelta > 0.0f) ? CassoExplorerCommands::kZoomIn : CassoExplorerCommands::kZoomOut);
        return true;
    }

    //  Ctrl with the wheel over the list sizes its icons, as in File Explorer.
    if (ev.kind == DxuiMouseEventKind::Wheel && ev.ctrl && !ev.wheelHorizontal
        && ExplorerLayout::Contains (m_context.listPane.GetListView()->GetBounds(), point))
    {
        m_iconWheelNotches += ev.wheelDelta;

        while (std::abs (m_iconWheelNotches) >= 1.0f)
        {
            bool  larger = m_iconWheelNotches > 0.0f;

            m_iconWheelNotches += larger ? -1.0f : 1.0f;

            if (m_context.listPane.GetColumns().StepIconSize (larger))
            {
                m_context.toolbars.ShowViewOnButton();
                m_context.listPane.SizeListIcons();
                m_context.listPane.FillList();
                m_context.window.Invalidate();
            }
        }

        return true;
    }

    if (m_context.toolbars.HandleMenuBarMouse (ev))
    {
        return true;
    }

    if (m_context.addressBar.HandleMouse (ev))
    {
        return true;
    }

    if (m_context.preview.GetToolbar().IsVisible()
        && (ExplorerLayout::Contains (m_context.preview.GetToolbar().GetBounds(), point) || ev.kind == DxuiMouseEventKind::Up)
        && m_context.toolbars.RouteToolbarMouse (m_context.preview.GetToolbar(), ev))
    {
        m_context.window.Invalidate();
        return true;
    }

    if (m_context.toolbars.HandleMouse (ev))
    {
        return true;
    }

    if (m_context.layout.GetTreeSplitter()->OnMouse (ev) || (m_context.layout.GetPreviewSplitter()->IsVisible() && m_context.layout.GetPreviewSplitter()->OnMouse (ev)))
    {
        m_context.window.Invalidate();
        return true;
    }

    if (m_context.tabManager.HandleCapturedMouse (ev))
    {
        return true;
    }

    //  A right-click on a scrollbar opens the bar's own menu, as Windows'
    //  bars do.
    if (press && ev.button == DxuiMouseButton::Right && m_context.menus.ShowScrollbarMenu (ev))
    {
        return true;
    }

    if (m_context.tabManager.HandleMouse (ev))
    {
        return true;
    }

    if (m_context.listPane.HandleCapturedMouse (ev))
    {
        return true;
    }

    if (m_context.treePane.HandleCapturedMouse (ev))
    {
        return true;
    }

    m_context.treePane.UpdateTip   (ev, point);
    m_context.statusBar.UpdateTip  (ev, point);
    m_context.listPane.UpdateTip   (ev, point);

    if (m_context.statusBar.HandleActivityClick (ev))
    {
        return true;
    }

    if (m_context.treePane.HandleMouse (ev))
    {
        return true;
    }

    //  The buttons under the list's message.
    if (m_context.cassoLinkManager.HandleListActionMouse (ev))
    {
        return true;
    }

    if (m_context.listPane.HandleMouse (ev))
    {
        return true;
    }

    return m_context.preview.HandleMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR ExplorerInput::GetCursorForPoint (POINT clientPx) const
{
    LPCWSTR  cursor = m_context.layout.GetTreeSplitter()->GetCursorForPoint (clientPx);



    //  An open menu has the mouse while it is open, and the pointer over it is
    //  the arrow, whatever the window has under the menu.
    if (m_context.window.GetPopupHost() != nullptr && m_context.window.GetPopupHost()->GetContextMenu().IsVisible())
    {
        return IDC_ARROW;
    }

    if (cursor == nullptr)
    {
        cursor = m_context.preview.GetCursorForPoint (clientPx);
    }

    if (cursor == nullptr)
    {
        cursor = m_context.addressBar.GetCursorForPoint (clientPx);
    }

    if (cursor == nullptr && m_context.layout.GetPreviewSplitter()->IsVisible())
    {
        cursor = m_context.layout.GetPreviewSplitter()->GetCursorForPoint (clientPx);
    }

    if (cursor == nullptr)
    {
        cursor = m_context.preview.GetPicture().GetCursorForPoint (clientPx);
    }

    if (cursor == nullptr)
    {
        cursor = m_context.listPane.GetCursorForPoint (clientPx);
    }

    return cursor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::HandleKey
//
//  Command keys first, then the menu bar's mnemonics and its open dropdown,
//  then Tab between panes, and the rest to the focused pane.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerInput::HandleKey (const DxuiKeyEvent & ev)
{
    int   command = 0;
    bool  handled = false;



    m_context.window.ArmTick();

    //  Alt+Enter opens a Properties dialog from inside this key-down, and the
    //  dialog's message loop would take the WM_SYSCHAR TranslateMessage has
    //  already queued for it as a menu mnemonic with no match, and beep. Casso
    //  Explorer has no Enter mnemonic, so the character is dropped.
    if (ev.kind == DxuiKeyEventKind::Down && ev.alt && ev.vk == VK_RETURN)
    {
        MSG  pending = {};

        PeekMessageW (&pending, m_context.window.GetHwnd(), WM_SYSCHAR, WM_SYSCHAR, PM_REMOVE);
    }

    if (m_context.tabManager.HandleEscapeKey (ev))
    {
        return true;
    }

    //  A rename in place has every key.
    if (m_context.fileOps.IsRenaming())
    {
        m_context.fileOps.HandleRenameKey (ev);
        return true;
    }

    //  The search box takes keys and characters first while it has focus;
    //  Tab and the keys it has no use for continue as usual.
    if (m_focus == ExplorerPane::LocationSearch)
    {
        handled = m_context.addressBar.HandleFindBoxKey (ev);

        if (handled || ev.kind != DxuiKeyEventKind::Down)
        {
            m_context.window.Invalidate();
            return handled;
        }
    }

    if (m_focus == ExplorerPane::Search || m_focus == ExplorerPane::GoTo)
    {
        handled = m_context.preview.GetFind().HandleBoxKey (m_focus, ev);

        if (handled || ev.kind != DxuiKeyEventKind::Down)
        {
            m_context.window.Invalidate();
            return handled;
        }
    }

    //  While the address bar is being edited, keys and characters go to its
    //  text field first; the keys it leaves, Tab and Ctrl+T among them, carry
    //  on as usual.
    if (m_focus == ExplorerPane::Address && m_context.addressBar.IsEditing())
    {
        handled = m_context.addressBar.HandleKey (ev);

        if (handled || ev.kind != DxuiKeyEventKind::Down)
        {
            m_context.window.Invalidate();
            return handled;
        }
    }

    //  The preview's shortcuts, for an archive's files and a message.
    if (m_focus == ExplorerPane::Preview && m_context.preview.HandleShortcutKey (ev))
    {
        return true;
    }

    //  A character typed at the file list jumps to a row. It has to reach the
    //  list before characters are turned away below.
    if (ev.kind == DxuiKeyEventKind::Char && m_focus == ExplorerPane::List)
    {
        return m_context.listPane.HandleCharKey (ev);
    }

    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    //  Ctrl+Z on the files undoes the last file operation; in a text box it is
    //  the box's own, which has the keys before this.
    if (ev.ctrl && !ev.alt && !ev.shift && ev.vk == 'Z' && (m_focus == ExplorerPane::List || m_focus == ExplorerPane::Tree) && !m_context.fileOps.IsRenaming())
    {
        m_context.fileOps.UndoLast();
        return true;
    }

    command = CassoExplorerCommands::TranslateKey (ev.vk, ev.ctrl, ev.alt, ev.shift);

    if (command != 0)
    {
        m_context.commandManager.Dispatch (command);
        return true;
    }

    if (m_context.toolbars.TryMenuBarKey (ev, handled))
    {
        return handled;
    }

    if (m_focus == ExplorerPane::List && m_context.listPane.HandleShortcutKey (ev))
    {
        return true;
    }

    //  The same keys on the tree act on its selected node.
    if (m_focus == ExplorerPane::Tree && m_context.treePane.TryShortcutKey (ev, handled))
    {
        return handled;
    }

    //  The preview's stops inside it come before the walk moves on.
    if (ev.vk == VK_TAB && !ev.ctrl && m_focus == ExplorerPane::Preview && m_context.preview.HandleTabKey (ev))
    {
        m_context.window.Invalidate();
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
            m_context.preview.HandleFocusEntered (!ev.shift);
        }

        return true;
    }

    switch (m_focus)
    {
        case ExplorerPane::Toolbar: handled = RouteToolbarKey (false, ev); break;
        case ExplorerPane::CommandBar: handled = RouteCommandBarKey (ev);  break;
        case ExplorerPane::PreviewToolbar: handled = RouteToolbarKey (true, ev); break;
        case ExplorerPane::Search:                                               break;
        case ExplorerPane::GoTo:                                                 break;
        case ExplorerPane::Address: handled = m_context.addressBar.HandleKey (ev); break;
        case ExplorerPane::LocationSearch: handled = m_context.addressBar.HandleFindBoxKey (ev);  break;
        case ExplorerPane::Tabs:    handled = m_context.tabManager.HandleKey (ev); break;
        case ExplorerPane::Tree:    handled = m_context.treePane.HandleKey (ev); break;
        case ExplorerPane::List:    handled = m_context.listPane.HandleKey (ev); break;
        case ExplorerPane::Preview: handled = m_context.preview.HandleKey (ev); break;
    }

    if (ev.vk == VK_BACK && !handled)
    {
        m_context.commandManager.Dispatch (CassoExplorerCommands::kUp);
        handled = true;
    }

    m_context.window.Invalidate();

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput::GetFocusedControl
//
//  The focused pane's control, where routing of a standard command such as
//  Copy or Select all starts.
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * ExplorerInput::GetFocusedControl() const
{
    switch (m_focus)
    {
        case ExplorerPane::Tabs:    return m_context.tabManager.GetControl();
        case ExplorerPane::Tree:    return m_context.treePane.GetControl();
        case ExplorerPane::List:    return m_context.listPane.GetControl();
        case ExplorerPane::Preview: return m_context.preview.GetFocusedControl();
        default:            return nullptr;
    }
}
