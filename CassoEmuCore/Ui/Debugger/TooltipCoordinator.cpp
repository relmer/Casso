#include "Pch.h"

#include "Ui/Debugger/TooltipCoordinator.h"

#include "Ui/Debugger/ColorLegend.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TooltipCoordinator::Update
//
//  A color tip put below the operand tip is passed to OperandTipController,
//  so that the two are hidden together.
//
////////////////////////////////////////////////////////////////////////////////

void TooltipCoordinator::Update (POINT clientPx)
{
    RECT                   cell        = {};
    RECT                   spot        = {};
    int64_t                now         = (int64_t) GetTickCount64();
    std::wstring           text;
    std::wstring           colorTip;
    bool                   isPending   = false;
    DxuiTooltip          & tip         = m_sources.GetRoutedTooltip();
    OperandTipController & operandTips = m_sources.GetOperandTips();



    operandTips.SetColorTip (nullptr);

    //  Only the P register's flags color their lines.
    tip.SetLineColors ({});

    if (m_sources.IsAnyMenuOpen())
    {
        tip.HideImmediate();
        return;
    }

    if (TryShowBarTip (clientPx, tip, now) || TryShowPaneTip (clientPx, tip, now, isPending) || isPending)
    {
        return;
    }

    if (m_sources.TryGetBranchTip (clientPx, cell, text))
    {
        tip.SetMonospace (false);
        tip.RequestShow  (cell, text, now);
        return;
    }

    if (m_sources.TryGetSymbolTip (clientPx, cell, text))
    {
        (void) TryGetCellTip (clientPx, spot, colorTip);
        tip.SetMonospace (true);
        tip.RequestShow  (cell, ColorLegend::JoinLines ({ text, colorTip }), now);
        return;
    }

    //  Over anything else drawn in a color that means something, what it
    //  means.
    if (TryGetCellTip (clientPx, cell, text) || m_sources.TryGetGraphicTip (clientPx, cell, text))
    {
        operandTips.SetColorTip (&tip);

        //  A sector's hex dump in the Disk II log lines up in columns.
        tip.SetMonospace (std::ranges::any_of (m_sources.GetTipPanes(), [clientPx] (const std::unique_ptr<IDebuggerPane> & pane) { return pane->IsMonospaceTipAt (clientPx); }));

        //  Under the operand tip when there is one, so the two stack.
        if (operandTips.HasTip())
        {
            tip.RequestShow (operandTips.GetColorTipAnchor (cell), text, now, false);
        }
        else
        {
            tip.RequestShow (cell, text, now);
        }

        return;
    }

    //  A tab's or title-bar button's tip is a sentence, not a table, so it
    //  reads in the body face.
    text = m_sources.GetChromeTipAt (clientPx, cell);

    if (!text.empty())
    {
        tip.SetMonospace (false);
        tip.RequestShow  (cell, text, now);
        return;
    }

    tip.RequestHide (now);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TooltipCoordinator::TryShowBarTip
//
//  The command bar's entries, what each does and its key in the scheme in
//  force; then each pane's bar, then the find widgets'.
//
////////////////////////////////////////////////////////////////////////////////

bool TooltipCoordinator::TryShowBarTip (POINT clientPx, DxuiTooltip & tip, int64_t now)
{
    RECT             cell   = {};
    const wchar_t  * barTip = m_sources.GetCommandBarTip (clientPx, cell);



    for (const std::unique_ptr<IDebuggerPane> & pane : m_sources.GetTipPanes())
    {
        if (barTip == nullptr || *barTip == L'\0')
        {
            barTip = pane->GetBarTip (clientPx, cell);
        }
    }

    if (barTip == nullptr || *barTip == L'\0')
    {
        barTip = m_sources.GetFindTip (clientPx, cell);
    }

    if (barTip == nullptr || *barTip == L'\0')
    {
        return false;
    }

    tip.SetMonospace (false);
    tip.RequestShow  (cell, barTip, now);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TooltipCoordinator::TryShowPaneTip
//
//  Over what a pane draws, such as the heat map or the profiler's timeline:
//  a control's tip, one at once following the pointer, or none yet while
//  what it lists is read, with a tip already up left up.
//
////////////////////////////////////////////////////////////////////////////////

bool TooltipCoordinator::TryShowPaneTip (POINT clientPx, DxuiTooltip & tip, int64_t now, bool & isPending)
{
    isPending = false;

    for (const std::unique_ptr<IDebuggerPane> & pane : m_sources.GetTipPanes())
    {
        PaneTip  paneTip;



        if (!pane->TryGetTip (clientPx, paneTip))
        {
            continue;
        }

        if (paneTip.kind == PaneTip::Kind::Pending)
        {
            isPending = true;
            return false;
        }

        tip.SetMonospace (paneTip.isMonospace);

        if (!paneTip.lineColors.empty())
        {
            tip.SetLineColors (std::move (paneTip.lineColors));
        }

        if (paneTip.kind == PaneTip::Kind::Control)
        {
            tip.RequestShow (paneTip.anchor, paneTip.text, now);
        }
        else
        {
            tip.RequestShowNow (GetPointerAnchor (clientPx), paneTip.text, now);
        }

        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TooltipCoordinator::TryGetCellTip
//
////////////////////////////////////////////////////////////////////////////////

bool TooltipCoordinator::TryGetCellTip (POINT clientPx, RECT & anchor, std::wstring & text) const
{
    const std::vector<DxuiListView::Cell>  * cells  = nullptr;
    DxuiListView                           * list   = nullptr;
    RECT                                     bounds = {};
    int                                      row    = -1;
    int                                      column = -1;
    int                                      right  = 0;



    for (DxuiListView * each : m_sources.GetTipLists())
    {
        if (each != nullptr && each->IsVisible() && DxuiDockSite::Contains (each->GetBounds(), clientPx))
        {
            list = each;
        }
    }

    if (list == nullptr)
    {
        return false;
    }

    bounds = list->GetBounds();
    row    = list->HitTestRow (clientPx.x - bounds.left, clientPx.y - bounds.top);
    column = m_sources.GetListColumnAt (list, clientPx.x - bounds.left);
    right  = list->GetPaneLeadPx() - list->GetLeftPx();

    for (size_t c = 0; c < list->GetColumnCount(); c++)
    {
        right += list->GetColumnEffectiveWidthPx (c);
    }

    if (column < 0 || clientPx.x - bounds.left >= right)
    {
        return false;
    }

    if (clientPx.y - bounds.top < list->GetHeaderHeightPx())
    {
        text   = list->GetColumnAt ((size_t) column).tip;
        anchor = GetPointerAnchor (clientPx);

        return !text.empty();
    }

    if (row < 0 || row >= list->GetRowCount())
    {
        return false;
    }

    cells = &list->GetCellsOfRow (row);

    if ((size_t) column >= cells->size() || (*cells)[(size_t) column].tip.empty())
    {
        return false;
    }

    //  A cell with only an icon, such as a breakpoint's dot, has no text to
    //  anchor on, so the tip goes by the pointer.
    if (!list->GetCellTextRectPx (row, (size_t) column, anchor))
    {
        anchor = RECT { clientPx.x - bounds.left, clientPx.y - bounds.top, clientPx.x - bounds.left + 1, clientPx.y - bounds.top + 1 };
    }

    OffsetRect (&anchor, bounds.left, bounds.top);
    text = (*cells)[(size_t) column].tip;
    return true;
}
