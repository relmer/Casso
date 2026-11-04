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
#include "Ui/Debugger/Panes/BasicPanes.h"
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
//  DebuggerWindow::GetRoutedTips
//
//  The tip service of the window the event being routed came from, since a
//  tooltip is drawn only over its window.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTipService & DebuggerWindow::GetRoutedTips()
{
    DxuiTipService  * tips = m_floating->GetRoutedTips();



    return (tips != nullptr) ? *tips : m_tipService;
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
//  appear over the menu once its delay ran out, so no tip opens while a menu
//  is open. True while the tip is fading or waiting to appear.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::TickTooltip (int64_t now)
{
    m_tipService.SetSuppressed (IsAnyMenuOpen());
    return m_tipService.Tick (now);
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
    DxuiListView   * basic  = (m_basicProgram != nullptr) ? m_basicProgram->GetList() : nullptr;
    RECT             bounds = {};
    bool             isOver = false;



    view = FindOperandTipView (clientPx);
    row  = -1;

    //  Off the disassembly, the BASIC program's values.
    if (view < 0 && basic != nullptr && IsRoutable (basic) && basic->IsVisible() && DxuiDockSite::Contains (basic->GetBounds(), clientPx))
    {
        view = kBasicTipView;
    }

    list = (view >= 0) ? GetTipList (view) : nullptr;
    CBR (list != nullptr);

    bounds = list->GetBounds();
    row    = list->HitTestRow (clientPx.x - bounds.left, clientPx.y - bounds.top);
    isOver = row >= 0 && GetColumnAt (list, clientPx.x - bounds.left) == (int) GetTipColumn (view);

Error:
    return isOver;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetTipList
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView * DebuggerWindow::GetTipList (int view) const
{
    if (view == kBasicTipView)
    {
        return (m_basicProgram != nullptr) ? m_basicProgram->GetList() : nullptr;
    }

    return m_code->GetCodeList (view);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetTipColumn
//
////////////////////////////////////////////////////////////////////////////////

size_t DebuggerWindow::GetTipColumn (int view) const
{
    return (view == kBasicTipView) ? BasicProgramPane::kValuesColumn : (size_t) kCodeOperandColumn;
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
                              m_dockSite->GetSlidPane() != (view == kBasicTipView ? std::wstring (DebuggerLayout::kBasicProgram) : DebuggerLayout::GetCodePaneId (view)) &&
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
//  DebuggerWindow::TryGetRoutedTip
//
//  A floating window has its site's title bar and tabs, and the panes'
//  controls, which are this window's children. A point there is tested
//  against the site first, then only against the controls of the panes in
//  that window.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::TryGetRoutedTip (POINT clientPx, DxuiTip & tip) const
{
    const std::wstring  & routing = m_floating->GetRoutingPane();
    const DxuiDockSite  * site    = routing.empty() ? nullptr : m_floating->FindSite (routing);



    if (site != nullptr && site->IsHitAt (clientPx))
    {
        return site->TryGetTip (clientPx, tip);
    }

    return TryGetTip (clientPx, tip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::TryGetTip
//
//  The zoom popup and a pinned color key are drawn over the panes but are
//  not controls, so over them no control's tip opens.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::TryGetTip (POINT clientPx, DxuiTip & tip) const
{
    RECT  pinned = {};
    bool  isMain = m_floating->GetRoutingPane().empty();



    if (isMain && m_zoomOpen && DxuiDockSite::Contains (m_zoomRect, clientPx))
    {
        return false;
    }

    if (isMain && m_colorKeys->TryGetPinnedRect (pinned) && DxuiDockSite::Contains (pinned, clientPx))
    {
        return false;
    }

    return DxuiWindow::TryGetTip (clientPx, tip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsChildHitTestable
//
//  A floating pane's controls are children of this window, laid out in the
//  floating window's pixels, so a point counts only the controls of the
//  window the event came from. The command bar is in its floating window
//  while it floats.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsChildHitTestable (const IDxuiControl & child) const
{
    if (&child == m_commandBar)
    {
        return m_floating->GetRoutingPane() == (m_barHost.IsFloating() ? std::wstring (kBarFloatKey) : std::wstring());
    }

    return IsRoutable (&child);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureTips
//
//  The tips that join facts from two parts, installed on the controls they
//  are over: a disassembly row's, a source line's and the status bar's.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureTips()
{
    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        DxuiListView  * list = m_code->GetCodeList (view);

        if (list != nullptr)
        {
            list->SetTipFn ([this, list] (POINT clientPx, DxuiTip & tip) { return TryGetCodeTip (*list, clientPx, tip); });
        }
    }

    for (int slot = 0; slot < SourceDocuments::kMaxDocuments; slot++)
    {
        const SourceDocumentPane  & document = m_sources->GetDocument (slot);

        if (document.GetView() != nullptr)
        {
            document.GetView()->SetTipFn ([this, &document] (POINT clientPx, DxuiTip & tip)
            {
                return TryGetBranchTip (clientPx, tip) || document.TryGetHeatTip (clientPx, tip);
            });
        }
    }

    if (m_statusBar != nullptr)
    {
        m_statusBar->SetTipFn ([this] (POINT clientPx, DxuiTip & tip) { return TryGetGraphicTip (clientPx, tip); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::TryGetCodeTip
//
//  Over a disassembly row: the PC's branch arrow, or a symbol and what the
//  color of its cell means. A symbol's tip is code, so it reads in the
//  fixed-width face. Elsewhere the list has its cells' tips.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::TryGetCodeTip (const DxuiListView & list, POINT clientPx, DxuiTip & tip) const
{
    RECT          bounds   = list.GetBounds();
    RECT          anchor   = {};
    std::wstring  text;
    DxuiTip       colorTip;



    if (TryGetBranchTip (clientPx, tip))
    {
        return true;
    }

    if (!m_code->TryGetSymbolTip (clientPx, anchor, text))
    {
        return false;
    }

    (void) list.TryGetCellTip (POINT { clientPx.x - bounds.left, clientPx.y - bounds.top }, colorTip);

    tip              = DxuiTip();
    tip.anchor       = anchor;
    tip.text         = ColorLegend::JoinLines ({ text, colorTip.text });
    tip.isMonospace  = true;
    tip.gradient     = std::move (colorTip.gradient);
    tip.gradientMark = colorTip.gradientMark;
    tip.lineScales   = std::move (colorTip.lineScales);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::TryGetGraphicTip
//
//  Over the status bar's history section, what its meter's colors mean.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::TryGetGraphicTip (POINT clientPx, DxuiTip & tip) const
{
    if (m_statusBar == nullptr || m_statusBar->FindFieldAt (clientPx) != (int) kStatusHistory ||
        DebuggerStatusText::GetHistoryFill (m_snapshot != nullptr ? m_snapshot->history : HistoryStatus()) < 0.0f)
    {
        return false;
    }

    tip            = DxuiTip();
    tip.anchor     = m_statusBar->GetFieldRect (kStatusHistory);
    tip.text       = ColorLegend::GetHistoryMeterTip();
    tip.isColorKey = true;
    return true;
}
