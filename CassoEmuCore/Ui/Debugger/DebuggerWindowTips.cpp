#include "Pch.h"

#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DebuggerMenuCommand.h"
#include "Ui/Debugger/DroppedFiles.h"
#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/GutterGlyph.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/Source/SourcePathList.h"
#include "Config/WindowPlacementProfile.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/FlagsDialog.h"
#include "Ui/Debugger/BreakpointDialog.h"
#include "Debugger/SymbolDescriptions.h"
#include "Cassque/CassquePromptDialog.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/Panes/TracePaneController.h"
#include "Ui/Debugger/MemoryBarCommands.h"
#include "Ui/Debugger/Panes/MeterBar.h"
#include "Ui/Debugger/MemoryAddressEntry.h"
#include "Ui/Debugger/Panes/BasicProgramController.h"
#include "Ui/Debugger/Panes/BreakpointsPane.h"
#include "Ui/Debugger/Panes/CodeEditCommands.h"
#include "Ui/Debugger/Panes/ConsolePanes.h"
#include "Ui/Debugger/ViewMenuRows.h"
#include "Ui/Debugger/Panes/DisassemblyGutter.h"
#include "Ui/Debugger/Panes/DisassemblyViews.h"
#include "Ui/Debugger/Panes/FindWidgets.h"
#include "Ui/Debugger/Panes/SourceDocumentPanes.h"
#include "Ui/Debugger/Panes/ToolPanes.h"
#include "Ui/Debugger/Panes/HeatMapPane.h"
#include "Ui/Debugger/Panes/MemoryPane.h"
#include "Ui/Debugger/Panes/MemoryWindows.h"
#include "Ui/Debugger/Panes/OperandTipController.h"
#include "Ui/Debugger/Panes/UndoListPanes.h"
#include "Ui/Debugger/Panes/WatchesPane.h"
#include "Ui/Debugger/DebuggerCommands.h"
#include "Ui/Debugger/HistoryBand.h"
#include "Ui/Debugger/HeatStrip.h"
#include "Ui/Debugger/Panes/FlameGraphView.h"
#include "Ui/Debugger/Panes/MemoryMapBar.h"
#include "Ui/Debugger/Panes/ProfileTimeline.h"
#include "Ui/Debugger/Panes/DiagnosticsPane.h"
#include "Ui/Debugger/ColorKeyButton.h"
#include "Ui/Debugger/HeatMapBarCommands.h"
#include "Ui/Debugger/HeatMapRangeBarCommands.h"
#include "Ui/Debugger/ToolbarLabelEntry.h"
#include "Ui/Debugger/ToolbarCheckEntry.h"
#include "Ui/Debugger/UndoBarCommands.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetRoutedTooltip
//
//  The tooltip of the window the event being routed came from, since a
//  tooltip shows only over the window that owns it.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTooltip & DebuggerWindow::GetRoutedTooltip()
{
    DxuiTooltip  * tip = m_floating->GetRoutedTip();



    return (tip != nullptr) ? *tip : m_tooltip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsAnyMenuOpen
//
//  Whether the menu bar, a toolbar's drop-down or a context menu is open,
//  in this window or a floating one.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsAnyMenuOpen() const
{
    DxuiHwndSource  * popups = GetPopupHost();
    bool              open   = false;



    open = (m_menuBar != nullptr && m_menuBar->IsOpen()) || (popups != nullptr && popups->GetContextMenu().IsVisible());

    for (const DxuiToolbar * bar : { m_commandBar })
    {
        open = open || (bar != nullptr && bar->IsMenuOpen());
    }

    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        for (const DxuiToolbar * bar : pane->GetBars())
        {
            open = open || (bar != nullptr && bar->IsMenuOpen());
        }
    }

    open = open || m_floating->IsAnyMenuOpen();

    return open;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::TickTooltip
//
//  A tip requested before a press opened a drop-down's menu would otherwise
//  appear over the menu once its delay ran out, so it is hidden while any
//  menu is open. True while the tip is fading or waiting to appear.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::TickTooltip (int64_t now)
{
    if (IsAnyMenuOpen())
    {
        m_tooltip.HideImmediate();
        return false;
    }

    if (!m_tooltip.WantsTick())
    {
        return false;
    }

    m_tooltip.Tick (now);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::UpdateTooltip
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::UpdateTooltip (POINT clientPx)
{
    m_tips->Update (clientPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetCommandBarTip
//
//  Only for the window the command bar is in.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DebuggerWindow::GetCommandBarTip (POINT clientPx, RECT & anchor) const
{
    bool  isBarWindow = m_floating->GetRoutingPane() == (m_barHost.IsFloating() ? std::wstring (kBarFloatKey) : std::wstring());



    if (!isBarWindow || m_commandBar == nullptr || !m_commandBar->IsVisible())
    {
        return nullptr;
    }

    return m_commandBar->GetTooltipAt (clientPx.x, clientPx.y, anchor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::FindOperandTipView
//
//  The disassembly view under a point in the window the event being routed
//  came from, or -1: one slid out over the panes ahead of any under it.
//
////////////////////////////////////////////////////////////////////////////////

int DebuggerWindow::FindOperandTipView (POINT clientPx) const
{
    std::wstring  slid   = (m_floating->GetRoutingPane().empty() && m_dockSite != nullptr) ? m_dockSite->GetSlidPane() : std::wstring();
    int           found  = -1;
    bool          isSlid = false;



    for (int each = 0; each < DebuggerViewState::kMaxCodeViews && !isSlid; each++)
    {
        DxuiListView  * code   = m_code->GetCodeList (each);
        bool            isOver = code != nullptr && IsRoutable (code) && code->IsVisible() && DxuiDockSite::Contains (code->GetBounds(), clientPx);

        if (isOver)
        {
            found  = each;
            isSlid = !slid.empty() && slid == DebuggerLayout::GetCodePaneId (each);
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::TryFindOperandTipRow
//
//  The disassembly view and row whose operand and result lie under a point.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::TryFindOperandTipRow (
    POINT    clientPx,
    int    & view,
    int    & row) const
{
    HRESULT          hr     = S_OK;
    DxuiListView   * list   = nullptr;
    RECT             bounds = {};
    bool             isOver = false;



    view = FindOperandTipView (clientPx);
    row  = -1;
    list = (view >= 0) ? m_code->GetCodeList (view) : nullptr;
    CBR (list != nullptr);

    bounds = list->GetBounds();
    row    = list->HitTestRow (clientPx.x - bounds.left, clientPx.y - bounds.top);
    isOver = row >= 0 && GetColumnAt (list, clientPx.x - bounds.left) == (int) kCodeOperandColumn;

Error:
    return isOver;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsOperandTipCovered
//
//  Whether anything lying over the disassembly meets the tip or the pointer:
//  the zoom popup, another pane slid out over it, a color key held open, a
//  find widget, or a window lying over this one, such as a floating pane,
//  command bar or timeline. The tip is a window of its own, shown over
//  everything drawn in this one and over the windows it owns, so it would
//  show over each of them; and a pointer on one of them is on it rather than
//  on the cell, though a color key lets it through to the panes.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsOperandTipCovered (
    int                       view,
    POINT                     clientPx,
    const RECT              & tipRect,
    const OperandTipScreen  & screen) const
{
    bool  isMain    = m_floating->GetRoutingPane().empty();
    bool  isCovered = false;
    RECT  pinned    = {};



    //  The zoom popup and a slid pane lie over this window's panes only.
    isCovered = isMain && m_zoomOpen && Meets (m_zoomRect, clientPx, tipRect);

    isCovered = isCovered || (isMain && m_dockSite != nullptr && !m_dockSite->GetSlidPane().empty() &&
                              m_dockSite->GetSlidPane() != DebuggerLayout::GetCodePaneId (view) &&
                              Meets (m_dockSite->GetSlidRect(), clientPx, tipRect));

    isCovered = isCovered || (m_colorKeys->TryGetPinnedRect (pinned) && Meets (pinned, clientPx, tipRect));

    isCovered = isCovered || m_finds->Overlaps (clientPx, tipRect);

    for (const RECT & over : screen.overWindows)
    {
        isCovered = isCovered || Meets (over, clientPx, tipRect);
    }

    return isCovered;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::Meets
//
//  Whether something lying at `over` holds the pointer or overlaps the tip.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::Meets (
    const RECT  & over,
    POINT         clientPx,
    const RECT  & tipRect)
{
    RECT  both = {};



    return DxuiDockSite::Contains (over, clientPx) || IntersectRect (&both, &over, &tipRect) != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetChromeTipAt
//
//  A tab's tip in this window, then a pane's title-bar buttons from the
//  dock site of the window the mouse is in: this one's, or the floating
//  window's the event was routed from.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetChromeTipAt (POINT clientPx, RECT & anchor) const
{
    const DxuiDockSite  * site = m_floating->GetRoutingPane().empty() ? m_dockSite : m_floating->FindSite (m_floating->GetRoutingPane());
    std::wstring          text;



    if (m_floating->GetRoutingPane().empty() && m_dockSite != nullptr && !m_dockSite->GetTabAt (clientPx, anchor, text).empty() && !text.empty())
    {
        return text;
    }

    if (site == nullptr)
    {
        return std::wstring();
    }

    return site->GetTitleButtonTipAt (clientPx, anchor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetTipLists
//
//  The lists in the window the event being routed came from.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView *> DebuggerWindow::GetTipLists() const
{
    std::vector<DxuiListView *>  lists;



    for (DxuiListView * each : GetLists())
    {
        if (each != nullptr && IsRoutable (each))
        {
            lists.push_back (each);
        }
    }

    return lists;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::TryGetGraphicTip
//
//  Over a device panel's disk head, what its colors mean; over the status
//  bar's history section, what its meter's do.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::TryGetGraphicTip (POINT clientPx, RECT & anchor, std::wstring & text) const
{
    if (m_floating->GetRoutingPane().empty() && m_statusBar != nullptr && m_statusBar->IsVisible() &&
        m_statusBar->FindFieldAt (clientPx) == (int) kStatusHistory &&
        DebuggerStatusText::GetHistoryFill (m_snapshot != nullptr ? m_snapshot->history : HistoryStatus()) >= 0.0f)
    {
        anchor = m_statusBar->GetFieldRect (kStatusHistory);
        text   = ColorLegend::GetHistoryMeterTip();
        return true;
    }

    return false;
}
