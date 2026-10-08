#include "Pch.h"

#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureUndoBars
//
//  The registers, stack and watch panes' bars: Undo and Redo, each tip giving
//  the edit it would act on.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureUndoBars()
{
    const std::array<std::pair<const wchar_t *, DxuiListView *>, kUndoBarCount>  panes =
    { {
        { DebuggerLayout::kRegisters, m_registerList },
        { DebuggerLayout::kStack,     m_stackList    },
        { DebuggerLayout::kWatches,   m_watchList    },
    } };



    for (size_t i = 0; i < kUndoBarCount; i++)
    {
        PaneUndoBar                & each     = m_undoBars[i];
        UndoBarCommands::Handlers    handlers;

        each.pane = panes[i].first;
        each.list = panes[i].second;
        each.bar  = CreateChild<DxuiToolbar>();

        handlers.dispatch  = [this, i] (int id) { RunPaneUndo (i, id == UndoBarCommands::kRedo); };
        handlers.isEnabled = [this, i] (int id) { return IsPaneUndoEnabled (i, id == UndoBarCommands::kRedo); };
        handlers.getTip    = [this, i] (int id) { return GetPaneUndoTip (i, id == UndoBarCommands::kRedo); };

        each.commands = std::make_unique<UndoBarCommands> (std::move (handlers));

        each.bar->SetTextRenderer (GetTextRenderer());
        each.bar->SetPopupHost    (GetPopupHost());
        each.bar->SetIconFace     (DxuiToolbar::kMdl2IconFace);
        each.bar->SetCompact      (true);
        each.bar->EnableSeeMore   (s_kpszMdl2More, L"See more");
        each.bar->SetEntries      (each.commands->BuildEntries());
        each.bar->SetVisible      (false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::PlaceUndoBars
//
//  Each bar sits in the place held at the top of its pane and goes with the
//  pane into a floating window, as the breakpoints pane's does.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::PlaceUndoBars()
{
    for (PaneUndoBar & each : m_undoBars)
    {
        bool          shown = each.bar != nullptr && each.slot != nullptr && each.slot->IsVisible();
        DxuiWindow  * host  = nullptr;
        RECT          slot  = {};

        if (each.bar == nullptr)
        {
            continue;
        }

        each.bar->SetVisible (shown);

        if (!shown)
        {
            continue;
        }

        host = GetPaneHost (each.pane);
        slot = each.slot->GetBounds();

        each.bar->SetTextRenderer   (host->GetTextRenderer());
        each.bar->SetPopupHost      (host->GetPopupHost());
        each.bar->SetHostClientRect (host->GetBounds());
        each.bar->Layout            (GetBarStrip (each.pane, slot), m_scaler);

        host->SetChildClip (each.bar, slot);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteUndoBarMouse
//
//  As the breakpoints pane's bar: the strip and whatever menu it has open take
//  the left button.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteUndoBarMouse (const DxuiMouseEvent & ev)
{
    int   x       = ev.positionDip.x;
    int   y       = ev.positionDip.y;
    bool  handled = false;



    for (PaneUndoBar & each : m_undoBars)
    {
        RECT  strip = {};
        bool  open  = false;
        bool  over  = false;

        if (handled || each.bar == nullptr || m_routingPane != GetBarRoutingPane (each.pane))
        {
            continue;
        }

        strip = each.bar->GetBounds();
        open  = each.bar->IsMenuOpen();
        over  = each.bar->IsVisible() && DxuiDockSite::Contains (strip, POINT { x, y });

        if (!over && !open)
        {
            each.bar->OnToolbarMouseLeave();
            continue;
        }

        if ((ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Up) && ev.button != DxuiMouseButton::Left && !open)
        {
            continue;
        }

        switch (ev.kind)
        {
        case DxuiMouseEventKind::Move:
            UpdateTooltip (ev.positionDip);
            handled = each.bar->OnToolbarMouseMove (x, y);
            break;

        case DxuiMouseEventKind::Down:
            handled = each.bar->OnToolbarLButtonDown (x, y);
            break;

        case DxuiMouseEventKind::Up:
            handled = each.bar->OnToolbarLButtonUp (x, y);
            break;

        default:
            handled = open;
            break;
        }
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RunPaneUndo
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::RunPaneUndo (size_t index, bool redo)
{
    switch (index)
    {
    case kRegisterUndoBar: UndoRegisterEdit (redo); break;
    case kStackUndoBar:    UndoStackEdit    (redo); break;
    case kWatchUndoBar:    UndoWatchEdit    (redo); break;
    default:                                        break;
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsPaneUndoEnabled
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsPaneUndoEnabled (size_t index, bool redo) const
{
    switch (index)
    {
    case kRegisterUndoBar: return redo ? m_registerHistory.CanRedo() : m_registerHistory.CanUndo();
    case kStackUndoBar:    return redo ? m_stackHistory.CanRedo()    : m_stackHistory.CanUndo();
    case kWatchUndoBar:    return redo ? m_watchHistory.CanRedo()    : m_watchHistory.CanUndo();
    default:               return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPaneUndoTip
//
//  "Undo changed 1 byte at $01FD"; the bar's own tip when there is nothing
//  to act on.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetPaneUndoTip (size_t index, bool redo) const
{
    std::wstring  text;



    if (!IsPaneUndoEnabled (index, redo))
    {
        return text;
    }

    switch (index)
    {
    case kRegisterUndoBar: text = redo ? m_registerHistory.GetRedoText() : m_registerHistory.GetUndoText(); break;
    case kStackUndoBar:    text = redo ? m_stackHistory.GetRedoText()    : m_stackHistory.GetUndoText();    break;
    case kWatchUndoBar:    text = redo ? m_watchHistory.GetRedoText()    : m_watchHistory.GetUndoText();    break;
    default:                                                                                               break;
    }

    return GetUndoLabel (redo, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetUndoBar
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbar * DebuggerWindow::GetUndoBar (const std::wstring & pane) const
{
    for (const PaneUndoBar & each : m_undoBars)
    {
        if (each.pane == pane)
        {
            return each.bar;
        }
    }

    return nullptr;
}
